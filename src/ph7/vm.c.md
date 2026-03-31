# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4161/5426 lines (76.69%)

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
|        - |   446 | `/*` |
|        - |   447 | ` * Enter a VM frame.` |
|        - |   448 | ` */` |
|    15110 |   449 | `static sxi32 VmEnterFrame(` |
|        - |   450 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   451 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   452 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   453 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   454 | `	)` |
|        2 |   455 |  |
|        - |   456 | `	VmFrame *pFrame;` |
|        - |   457 | `	/* Allocate a new frame */` |
|    15112 |   458 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15112 |   459 | `	if( pFrame == 0 ){` |
|      ! 0 |   460 | `		return SXERR_MEM;` |
|        - |   461 | `	}` |
|        - |   462 | `	/* Link to the list of active VM frame */` |
|    15112 |   463 | `	pFrame->pParent = pVm->pFrame;` |
|    15112 |   464 | `	pVm->pFrame = pFrame;` |
|    15112 |   465 | `	if( ppFrame ){` |
|        - |   466 | `		/* Write a pointer to the new VM frame */` |
|    12562 |   467 | `		*ppFrame = pFrame;` |
|     6280 |   468 | `	}` |
|    15112 |   469 | `	return SXRET_OK;` |
|     7557 |   470 |  |
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
|    12560 |   517 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   518 |  |
|    12562 |   519 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12562 |   520 | `	if( pCurFrame ){` |
|        - |   521 | `		/* Unlink from the list of active VM frame */` |
|    12562 |   522 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12562 |   523 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   524 | `			VmSlot  *aSlot;` |
|        - |   525 | `			sxu32 n;` |
|        - |   526 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12504 |   527 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    88654 |   528 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   529 | `				/* Unset the local variable */` |
|    76152 |   530 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    38077 |   531 | `			}` |
|        - |   532 | `			/* Remove local reference */` |
|    12504 |   533 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    88710 |   534 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    76208 |   535 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    38105 |   536 | `			}` |
|     6251 |   537 | `		}` |
|        - |   538 | `		/* Release internal containers */` |
|    12562 |   539 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12562 |   540 | `		SySetRelease(&pCurFrame->sArg);` |
|    12562 |   541 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12562 |   542 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   543 | `		/* Release the whole structure */` |
|    12562 |   544 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6280 |   545 | `	}` |
|    12562 |   546 |  |
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
|    90310 |   663 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   664 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   665 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   666 | `	)` |
|        2 |   667 |  |
|        - |   668 | `	ph7_class_method *pMeth;` |
|        - |   669 | `	ph7_class_attr *pAttr;` |
|        - |   670 | `	SyHashEntry *pEntry;` |
|        - |   671 | `	sxi32 rc;` |
|        - |   672 | `	/* Reset the loop cursor */` |
|    90312 |   673 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   674 | `	/* Process only static and constant attribute */` |
|   359529 |   675 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   676 | `		/* Extract the current attribute */` |
|   224064 |   677 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   224064 |   678 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    90312 |   700 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   701 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   702 | `		 */` |
|    46446 |   703 | `		return SXRET_OK;` |
|        - |   704 | `	}` |
|        - |   705 | `	/* Create constructor alias if not yet done */` |
|    43868 |   706 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   707 | `		/* User constructor with the same base class name */` |
|      284 |   708 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      284 |   709 | `		if( pEntry ){` |
|      ! 0 |   710 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   711 | `			/* Create the alias */` |
|      ! 0 |   712 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   713 | `		}` |
|      141 |   714 | `	}` |
|        - |   715 | `	/* Install the methods now */` |
|    43868 |   716 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   424241 |   717 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   358442 |   718 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   358442 |   719 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   358436 |   720 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   358436 |   721 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   722 | `				return rc;` |
|        - |   723 | `			}` |
|   179217 |   724 | `		}` |
|        2 |   725 | `	}` |
|        - |   726 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    43868 |   727 | `	pClass->bMounted = TRUE;` |
|    43868 |   728 | `	return SXRET_OK;` |
|    45157 |   729 |  |
|        - |   730 | `/*` |
|        - |   731 | ` * Allocate a private frame for attributes of the given` |
|        - |   732 | ` * class instance (Object in the PHP jargon).` |
|        - |   733 | ` */` |
|     1134 |   734 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   735 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   736 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   737 | `	)` |
|        2 |   738 |  |
|     1136 |   739 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   740 | `	ph7_class_attr *pAttr;` |
|        - |   741 | `	SyHashEntry *pEntry;` |
|        - |   742 | `	sxi32 rc;` |
|        - |   743 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1136 |   744 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4720 |   745 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   746 | `		VmClassAttr *pVmAttr;` |
|        - |   747 | `		/* Extract the current attribute */` |
|     3586 |   748 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3586 |   749 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3586 |   750 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   751 | `			return SXERR_MEM;` |
|        - |   752 | `		}` |
|     3586 |   753 | `		pVmAttr->pAttr = pAttr;` |
|     3586 |   754 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   755 | `			ph7_value *pMemObj;` |
|        - |   756 | `			/* Reserve a memory object for this attribute */` |
|     3580 |   757 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3580 |   758 | `			if( pMemObj == 0 ){` |
|      ! 0 |   759 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   760 | `				return SXERR_MEM;` |
|        - |   761 | `			}` |
|     3580 |   762 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3580 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1174 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      586 |   766 | `			}` |
|     3580 |   767 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3580 |   768 | `			if( rc != SXRET_OK ){` |
|        - |   769 | `				VmSlot sSlot;` |
|        - |   770 | `				/* Restore memory object */` |
|      ! 0 |   771 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   772 | `				sSlot.pUserData = 0;` |
|      ! 0 |   773 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   774 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   775 | `				return SXERR_MEM;` |
|        - |   776 | `			}` |
|        - |   777 | `			/* Install attribute in the reference table */` |
|     3580 |   778 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1791 |   779 | `		}else{` |
|        - |   780 | `			/* Install static/constant attribute */` |
|        8 |   781 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   782 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   783 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   784 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   785 | `				return SXERR_MEM;` |
|        - |   786 | `			}` |
|        - |   787 | `		}` |
|        2 |   788 | `	}` |
|     1136 |   789 | `	return SXRET_OK;` |
|      569 |   790 |  |
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
|   310816 |   802 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   803 |  |
|        - |   804 | `	ph7_value *pObj;` |
|        - |   805 | `	sxi32 rc;` |
|   310818 |   806 | `	if( pIndex ){` |
|        - |   807 | `		/* Object index in the object table */` |
|   303168 |   808 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   151583 |   809 | `	}` |
|        - |   810 | `	/* Reserve a slot for the new object */` |
|   310818 |   811 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   310818 |   812 | `	if( rc != SXRET_OK ){` |
|        - |   813 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   814 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   815 | `		 */` |
|      ! 0 |   816 | `		return 0;` |
|        - |   817 | `	}` |
|   310818 |   818 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   310818 |   819 | `	return pObj;` |
|   155410 |   820 |  |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|  2140752 |   825 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|  2140754 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|  2140754 |   831 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1070376 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|  2140754 |   834 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2140754 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|  2140754 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2140754 |   842 | `	return pObj;` |
|  1070378 |   843 |  |
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
|     2550 |  1196 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1197 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1198 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1199 | `	 )` |
|        2 |  1200 |  |
|        - |  1201 | `	SyString sBuiltin;` |
|        - |  1202 | `	ph7_value *pObj;` |
|        - |  1203 | `	sxi32 rc;` |
|        - |  1204 | `	/* Zero the structure */` |
|     2552 |  1205 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1206 | `	/* Initialize VM fields */` |
|     2552 |  1207 | `	pVm->pEngine = &(*pEngine);` |
|     2552 |  1208 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1209 | `	/* Instructions containers */` |
|     2552 |  1210 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2552 |  1211 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2552 |  1212 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1213 | `	/* Object containers */` |
|     2552 |  1214 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2552 |  1215 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1216 | `	/* Virtual machine internal containers */` |
|     2552 |  1217 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2552 |  1218 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2552 |  1219 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2552 |  1220 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2552 |  1221 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2552 |  1222 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2552 |  1223 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2552 |  1224 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2552 |  1225 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2552 |  1226 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2552 |  1227 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2552 |  1228 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2552 |  1229 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2552 |  1230 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2552 |  1231 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2552 |  1232 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2552 |  1233 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2552 |  1234 | `	pVm->pPendingException = 0;` |
|        - |  1235 | `	/* Configuration containers */` |
|     2552 |  1236 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2552 |  1237 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2552 |  1238 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2552 |  1239 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2552 |  1240 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1241 | `	/* Error callbacks containers */` |
|     2552 |  1242 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2552 |  1243 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2552 |  1244 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2552 |  1245 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2552 |  1246 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1247 | `	/* Set a default recursion limit */` |
|        - |  1248 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2552 |  1249 | `	pVm->nMaxDepth = 32;` |
|        - |  1250 | `#else` |
|        - |  1251 | `	pVm->nMaxDepth = 16;` |
|        - |  1252 | `#endif` |
|        - |  1253 | `	/* Default assertion flags */` |
|     2552 |  1254 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1255 | `	/* JSON return status */` |
|     2552 |  1256 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1257 | `	/* PRNG context */` |
|     2552 |  1258 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1259 | `	/* Install the null constant */` |
|     2552 |  1260 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2552 |  1261 | `	if( pObj == 0 ){` |
|      ! 0 |  1262 | `		rc = SXERR_MEM;` |
|      ! 0 |  1263 | `		goto Err;` |
|        - |  1264 | `	}` |
|     2552 |  1265 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1266 | `	/* Install the boolean TRUE constant */` |
|     2552 |  1267 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2552 |  1268 | `	if( pObj == 0 ){` |
|      ! 0 |  1269 | `		rc = SXERR_MEM;` |
|      ! 0 |  1270 | `		goto Err;` |
|        - |  1271 | `	}` |
|     2552 |  1272 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1273 | `	/* Install the boolean FALSE constant */` |
|     2552 |  1274 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2552 |  1275 | `	if( pObj == 0 ){` |
|      ! 0 |  1276 | `		rc = SXERR_MEM;` |
|      ! 0 |  1277 | `		goto Err;` |
|        - |  1278 | `	}` |
|     2552 |  1279 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1280 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1281 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1282 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2552 |  1283 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2552 |  1284 | `	if( pObj == 0 ){` |
|      ! 0 |  1285 | `		rc = SXERR_MEM;` |
|      ! 0 |  1286 | `		goto Err;` |
|        - |  1287 | `	}` |
|     2552 |  1288 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1289 | `	/* Create the global frame */` |
|     2552 |  1290 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2552 |  1291 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1292 | `		goto Err;` |
|        - |  1293 | `	}` |
|        - |  1294 | `	/* Initialize the code generator */` |
|     2552 |  1295 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2552 |  1296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1297 | `		goto Err;` |
|        - |  1298 | `	}` |
|        - |  1299 | `	/* VM correctly initialized,set the magic number */` |
|     2552 |  1300 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2552 |  1301 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1302 | `	/* Compile the built-in library */` |
|     2552 |  1303 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1304 | `	/* Reset the code generator */` |
|     2552 |  1305 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2552 |  1306 | `	return SXRET_OK;` |
|      ! 0 |  1307 | `Err:` |
|      ! 0 |  1308 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1309 | `	return rc;` |
|     1277 |  1310 |  |
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
|    31082 |  1340 | `static ph7_value * VmNewOperandStack(` |
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
|    31084 |  1353 | `	nInstr += VM_STACK_GUARD;` |
|    31084 |  1354 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    31084 |  1355 | `	if( pStack == 0 ){` |
|      ! 0 |  1356 | `		return 0;` |
|        - |  1357 | `	}` |
|        - |  1358 | `	/* Initialize the operand stack */` |
|  1964858 |  1359 | `	while( nInstr > 0 ){` |
|  1933776 |  1360 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1933776 |  1361 | `		--nInstr;` |
|        2 |  1362 | `	}` |
|        - |  1363 | `	/* Ready for bytecode execution */` |
|    31084 |  1364 | `	return pStack;` |
|    15543 |  1365 |  |
|        - |  1366 | `/* Forward declaration */` |
|        - |  1367 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1368 | `/*` |
|        - |  1369 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1370 | ` * This routine gets called by the PH7 engine after` |
|        - |  1371 | ` * successful compilation of the target PHP program.` |
|        - |  1372 | ` */` |
|     2288 |  1373 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1374 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1375 | `	)` |
|        2 |  1376 |  |
|        - |  1377 | `	SyHashEntry *pEntry;` |
|        - |  1378 | `	sxi32 rc;` |
|     2290 |  1379 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1380 | `		/* Initialize your VM first */` |
|      ! 0 |  1381 | `		return SXERR_CORRUPT;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* Mark the VM ready for byte-code execution */` |
|     2290 |  1384 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1385 | `	/* Release the code generator now we have compiled our program */` |
|     2290 |  1386 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1387 | `	/* Emit the DONE instruction */` |
|     2290 |  1388 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2290 |  1389 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1390 | `		return SXERR_MEM;` |
|        - |  1391 | `	}` |
|        - |  1392 | `	/* Script return value */` |
|     2290 |  1393 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1394 | `	/* Allocate a new operand stack */` |
|     2290 |  1395 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2290 |  1396 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1397 | `		return SXERR_MEM;` |
|        - |  1398 | `	}` |
|        - |  1399 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1400 | `	 * private data. */` |
|     2290 |  1401 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2290 |  1402 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1403 | `	/* Allocate the reference table */` |
|     2290 |  1404 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2290 |  1405 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2290 |  1406 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return SXERR_MEM;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Zero the reference table */` |
|     2290 |  1411 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1412 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2290 |  1413 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2290 |  1414 | `	if( rc != SXRET_OK ){` |
|        - |  1415 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1416 | `		return rc;` |
|        - |  1417 | `	}` |
|        - |  1418 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2290 |  1419 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2290 |  1420 | `	if( rc != SXRET_OK ){` |
|        - |  1421 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1422 | `		return rc;` |
|        - |  1423 | `	}` |
|        - |  1424 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2290 |  1425 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1426 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2290 |  1427 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1428 | `	/* Initialize and install static and constants class attributes */` |
|     2290 |  1429 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    29910 |  1430 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    27622 |  1431 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    27622 |  1432 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1433 | `			return rc;` |
|        - |  1434 | `		}` |
|        2 |  1435 | `	}` |
|        - |  1436 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2290 |  1437 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1438 | `	/* VM is ready for bytecode execution */` |
|     2290 |  1439 | `	return SXRET_OK;` |
|     1146 |  1440 |  |
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
|     2280 |  1460 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1461 |  |
|        - |  1462 | `	/* Set the stale magic number */` |
|     2282 |  1463 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1464 | `	/* Release the private memory subsystem */` |
|     2282 |  1465 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2282 |  1466 | `	return SXRET_OK;` |
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
|   548894 |  1478 | `static sxi32 VmInitCallContext(` |
|        - |  1479 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1480 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1481 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1482 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1483 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1484 | `	)` |
|        2 |  1485 |  |
|   548896 |  1486 | `	pOut->pFunc = pFunc;` |
|   548896 |  1487 | `	pOut->pVm   = pVm;` |
|   548896 |  1488 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   548896 |  1489 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1490 | `	/* Assume a null return value */` |
|   548896 |  1491 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   548896 |  1492 | `	pOut->pRet = pRet;` |
|   548896 |  1493 | `	pOut->iFlags = iFlags;` |
|   548896 |  1494 | `	return SXRET_OK;` |
|        2 |  1495 |  |
|        - |  1496 | `/*` |
|        - |  1497 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1498 | ` * left behind.` |
|        - |  1499 | ` */` |
|   548894 |  1500 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1501 |  |
|        - |  1502 | `	sxu32 n;` |
|   548896 |  1503 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6698 |  1504 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19110 |  1505 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12414 |  1506 | `			if( apObj[n] == 0 ){` |
|        - |  1507 | `				/* Already released */` |
|      250 |  1508 | `				continue;` |
|        - |  1509 | `			}` |
|    12166 |  1510 | `			PH7_MemObjRelease(apObj[n]);` |
|    12166 |  1511 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6084 |  1512 | `		}` |
|     6698 |  1513 | `		SySetRelease(&pCtx->sVar);` |
|     3348 |  1514 | `	}` |
|   548896 |  1515 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   548896 |  1531 |  |
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
|  3242928 |  1562 | `static void VmPopOperand(` |
|        - |  1563 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1564 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1565 | `	)` |
|        2 |  1566 |  |
|  3242930 |  1567 | `	ph7_value *pTos = *ppTos;` |
|  6887586 |  1568 | `	while( nPop > 0 ){` |
|  3644658 |  1569 | `		PH7_MemObjRelease(pTos);` |
|  3644658 |  1570 | `		pTos--;` |
|  3644658 |  1571 | `		nPop--;` |
|        2 |  1572 | `	}` |
|        - |  1573 | `	/* Top of the stack */` |
|  3242930 |  1574 | `	*ppTos = pTos;` |
|  3242930 |  1575 |  |
|        - |  1576 | `/*` |
|        - |  1577 | ` * Reserve a memory object.` |
|        - |  1578 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1579 | ` */` |
|  2998714 |  1580 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1581 |  |
|  2998716 |  1582 | `	ph7_value *pObj = 0;` |
|        - |  1583 | `	VmSlot *pSlot;` |
|        - |  1584 | `	sxu32 nIdx;` |
|        - |  1585 | `	/* Check for a free slot */` |
|  2998716 |  1586 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2998716 |  1587 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2998716 |  1588 | `	if( pSlot ){` |
|   857964 |  1589 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   857964 |  1590 | `		nIdx = pSlot->nIdx;` |
|   428981 |  1591 | `	}` |
|  2998716 |  1592 | `	if( pObj == 0 ){` |
|        - |  1593 | `		/* Reserve a new memory object */` |
|  2140754 |  1594 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2140754 |  1595 | `		if( pObj == 0 ){` |
|      ! 0 |  1596 | `			return 0;` |
|        - |  1597 | `		}` |
|  1070376 |  1598 | `	}` |
|        - |  1599 | `	/* Set a null default value */` |
|  2998716 |  1600 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2998716 |  1601 | `	pObj->nIdx = nIdx;` |
|  2998716 |  1602 | `	return pObj;` |
|  1499359 |  1603 |  |
|        - |  1604 | `/*` |
|        - |  1605 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1606 | ` */` |
|    28612 |  1607 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1608 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1609 | `	const char *zKey,  /* Entry key */` |
|        - |  1610 | `	sxu32 nByte,       /* Key length */` |
|        - |  1611 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1612 | `	)` |
|        2 |  1613 |  |
|        - |  1614 | `	ph7_value sKey;` |
|        - |  1615 | `	sxi32 rc;` |
|    28614 |  1616 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    28614 |  1617 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1618 | `	/* Perform the insertion */` |
|    28614 |  1619 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    28614 |  1620 | `	PH7_MemObjRelease(&sKey);` |
|    28614 |  1621 | `	return rc;` |
|        2 |  1622 |  |
|        - |  1623 | `/*` |
|        - |  1624 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1625 | ` * Return a pointer to the variable value on success.` |
|        - |  1626 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1627 | ` */` |
|  3041096 |  1628 | `static ph7_value * VmExtractMemObj(` |
|        - |  1629 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1630 | `	const SyString *pName, /* Variable name */` |
|        - |  1631 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1632 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1633 | `	)` |
|        2 |  1634 |  |
|  3041098 |  1635 | `	int bNullify = FALSE;` |
|        - |  1636 | `	SyHashEntry *pEntry;` |
|        - |  1637 | `	VmFrame *pFrame;` |
|        - |  1638 | `	ph7_value *pObj;` |
|        - |  1639 | `	sxu32 nIdx;` |
|        - |  1640 | `	sxi32 rc;` |
|        - |  1641 | `	/* Point to the top active frame */` |
|  3041098 |  1642 | `	pFrame = pVm->pFrame;` |
|  3041126 |  1643 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1644 | `		/* Safely ignore the exception frame */` |
|       30 |  1645 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        2 |  1646 | `	}` |
|        - |  1647 | `	/* Perform the lookup */` |
|  3041098 |  1648 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1649 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1650 | `		pName = &sAnnon;` |
|        - |  1651 | `		/* Always nullify the object */` |
|      ! 0 |  1652 | `		bNullify = TRUE;` |
|      ! 0 |  1653 | `		bDup = FALSE;` |
|      ! 0 |  1654 | `	}` |
|        - |  1655 | `	/* Check the superglobals table first */` |
|  3041098 |  1656 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3041098 |  1657 | `	if( pEntry == 0 ){` |
|        - |  1658 | `		/* Query the top active frame */` |
|  3041062 |  1659 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3041062 |  1660 | `		if( pEntry == 0 ){` |
|    82490 |  1661 | `			char *zName = (char *)pName->zString;` |
|        - |  1662 | `			VmSlot sLocal;` |
|    82490 |  1663 | `			if( !bCreate ){` |
|        - |  1664 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1665 | `				return 0;` |
|        - |  1666 | `			}` |
|        - |  1667 | `			/* No such variable,automatically create a new one and install` |
|        - |  1668 | `			 * it in the current frame.` |
|        - |  1669 | `			 */` |
|    81860 |  1670 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    81860 |  1671 | `			if( pObj == 0 ){` |
|      ! 0 |  1672 | `				return 0;` |
|        - |  1673 | `			}` |
|    81860 |  1674 | `			nIdx = pObj->nIdx;` |
|    81860 |  1675 | `			if( bDup ){` |
|        - |  1676 | `				/* Duplicate name */` |
|      164 |  1677 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1678 | `				if( zName == 0 ){` |
|      ! 0 |  1679 | `					return 0;` |
|        - |  1680 | `				}` |
|       81 |  1681 | `			}` |
|        - |  1682 | `			/* Link to the top active VM frame */` |
|    81860 |  1683 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    81860 |  1684 | `			if( rc != SXRET_OK ){` |
|        - |  1685 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1686 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1687 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1688 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1689 | `				return 0;` |
|        - |  1690 | `			}` |
|    81860 |  1691 | `			if( pFrame->pParent != 0 ){` |
|        - |  1692 | `				/* Local variable */` |
|    76152 |  1693 | `				sLocal.nIdx = nIdx;` |
|    76152 |  1694 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    38077 |  1695 | `			}else{` |
|        - |  1696 | `				/* Register in the $GLOBALS array */` |
|     5710 |  1697 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1698 | `			}` |
|        - |  1699 | `			/* Install in the reference table */` |
|    81860 |  1700 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1701 | `			/* Save object index */` |
|    81860 |  1702 | `			pObj->nIdx = nIdx;` |
|    40931 |  1703 | `		}else{` |
|        - |  1704 | `			/* Extract variable contents */` |
|  2958574 |  1705 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2958574 |  1706 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2958574 |  1707 | `			if( bNullify && pObj ){` |
|      ! 0 |  1708 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1709 | `			}` |
|        - |  1710 | `		}` |
|  1520327 |  1711 | `	}else{` |
|        - |  1712 | `		/* Superglobal */` |
|       38 |  1713 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1714 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1715 | `	}` |
|  3040468 |  1716 | `	return pObj;` |
|  1520660 |  1717 |  |
|        - |  1718 | `/*` |
|        - |  1719 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1720 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1721 | ` */` |
|     2314 |  1722 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1723 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1724 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1725 | `	sxu32 nByte        /* zName length */` |
|        - |  1726 | `	)` |
|        2 |  1727 |  |
|        - |  1728 | `	SyHashEntry *pEntry;` |
|        - |  1729 | `	ph7_value *pValue;` |
|        - |  1730 | `	sxu32 nIdx;` |
|        - |  1731 | `	/* Query the superglobal table */` |
|     2316 |  1732 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2316 |  1733 | `	if( pEntry == 0 ){` |
|        - |  1734 | `		/* No such entry */` |
|      ! 0 |  1735 | `		return 0;` |
|        - |  1736 | `	}` |
|        - |  1737 | `	/* Extract the superglobal index in the global object pool */` |
|     2316 |  1738 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1739 | `	/* Extract the variable value  */` |
|     2316 |  1740 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2316 |  1741 | `	return pValue;` |
|     1159 |  1742 |  |
|        - |  1743 | `/*` |
|        - |  1744 | ` * Perform a raw hashmap insertion.` |
|        - |  1745 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1746 | ` */` |
|     2312 |  1747 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1748 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1749 | `	const char *zKey,   /* Entry key */` |
|        - |  1750 | `	int nKeylen,        /* zKey length*/` |
|        - |  1751 | `	const char *zData,  /* Entry data */` |
|        - |  1752 | `	int nLen            /* zData length */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|        - |  1755 | `	ph7_value sKey,sValue;` |
|        - |  1756 | `	sxi32 rc;` |
|     2314 |  1757 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2314 |  1758 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2314 |  1759 | `	if( zKey ){` |
|     2292 |  1760 | `		if( nKeylen < 0 ){` |
|     2292 |  1761 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1145 |  1762 | `		}` |
|     2292 |  1763 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1145 |  1764 | `	}` |
|     2314 |  1765 | `	if( zData ){` |
|     2314 |  1766 | `		if( nLen < 0 ){` |
|        - |  1767 | `			/* Compute length automatically */` |
|      ! 0 |  1768 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1769 | `		}` |
|     2314 |  1770 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1156 |  1771 | `	}` |
|        - |  1772 | `	/* Perform the insertion */` |
|     2314 |  1773 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2314 |  1774 | `	PH7_MemObjRelease(&sKey);` |
|     2314 |  1775 | `	PH7_MemObjRelease(&sValue);` |
|     2314 |  1776 | `	return rc;` |
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
|    36632 |  1791 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1792 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1793 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1794 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1795 | `	)` |
|        2 |  1796 |  |
|    36634 |  1797 | `	sxi32 rc = SXRET_OK;` |
|    36634 |  1798 | `	switch(nOp){` |
|     1144 |  1799 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2290 |  1800 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2290 |  1801 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1802 | `		/* VM output consumer callback */` |
|        - |  1803 | `#ifdef UNTRUST` |
|        - |  1804 | `		if( xConsumer == 0 ){` |
|        - |  1805 | `			rc = SXERR_CORRUPT;` |
|        - |  1806 | `			break;` |
|        - |  1807 | `		}` |
|        - |  1808 | `#endif` |
|        - |  1809 | `		/* Install the output consumer */` |
|     2290 |  1810 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2290 |  1811 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2290 |  1812 | `		break;` |
|        - |  1813 | `							   }` |
|     1144 |  1814 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1815 | `		/* Import path */` |
|        - |  1816 | `		  const char *zPath;` |
|        - |  1817 | `		  SyString sPath;` |
|     2290 |  1818 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1819 | `#if defined(UNTRUST)` |
|        - |  1820 | `		  if( zPath == 0 ){` |
|        - |  1821 | `			  rc = SXERR_EMPTY;` |
|        - |  1822 | `			  break;` |
|        - |  1823 | `		  }` |
|        - |  1824 | `#endif` |
|     2290 |  1825 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1826 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1827 | `#ifdef __WINNT__` |
|        2 |  1828 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1829 | `#endif` |
|     4578 |  1830 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1831 | `		  /* Remove leading and trailing white spaces */` |
|     2290 |  1832 | `		  SyStringFullTrim(&sPath);` |
|     2290 |  1833 | `		  if( sPath.nByte > 0 ){` |
|        - |  1834 | `			  /* Store the path in the corresponding conatiner */` |
|     2290 |  1835 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1144 |  1836 | `		  }` |
|     2290 |  1837 | `		  break;` |
|        - |  1838 | `									 }` |
|     1144 |  1839 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1840 | `		/* Run-Time Error report */` |
|     2290 |  1841 | `		pVm->bErrReport = 1;` |
|     2290 |  1842 | `		break;` |
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
|    11440 |  1864 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1865 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1866 | `		/* Create a new superglobal/global variable */` |
|    22882 |  1867 | `		const char *zName = va_arg(ap,const char *);` |
|    22882 |  1868 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    22882 |  1879 | `		nByte = SyStrlen(zName);` |
|    22882 |  1880 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1881 | `			/* Check if the superglobal is already installed */` |
|    22882 |  1882 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11442 |  1883 | `		}else{` |
|        - |  1884 | `			/* Query the top active VM frame */` |
|      ! 0 |  1885 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1886 | `		}` |
|    22882 |  1887 | `		if( pEntry ){` |
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
|    22882 |  1898 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    22882 |  1899 | `			if( pObj == 0 ){` |
|      ! 0 |  1900 | `				rc = SXERR_MEM;` |
|      ! 0 |  1901 | `				break;` |
|        - |  1902 | `			}` |
|    22882 |  1903 | `			nIdx = pObj->nIdx;` |
|        - |  1904 | `			/* Copy value */` |
|    22882 |  1905 | `			PH7_MemObjStore(pValue,pObj);` |
|    22882 |  1906 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1907 | `				/* Install the superglobal */` |
|    22882 |  1908 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11442 |  1909 | `			}else{` |
|        - |  1910 | `				/* Install in the current frame */` |
|      ! 0 |  1911 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1912 | `			}` |
|    22882 |  1913 | `			if( rc == SXRET_OK ){` |
|        - |  1914 | `				SyHashEntry *pRef;` |
|    22882 |  1915 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    22882 |  1916 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11442 |  1917 | `				}else{` |
|      ! 0 |  1918 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1919 | `				}` |
|        - |  1920 | `				/* Install in the reference table */` |
|    22882 |  1921 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    22882 |  1922 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1923 | `					/* Register in the $GLOBALS array */` |
|    22882 |  1924 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11440 |  1925 | `				}` |
|    11440 |  1926 | `			}` |
|        - |  1927 | `		}` |
|    22882 |  1928 | `		break;` |
|        - |  1929 | `									}` |
|     1145 |  1930 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1931 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1932 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1933 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1934 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1935 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1936 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2292 |  1937 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2292 |  1938 | `		const char *zValue = va_arg(ap,const char *);` |
|     2292 |  1939 | `		int nLen = va_arg(ap,int);` |
|        - |  1940 | `		ph7_hashmap *pMap;` |
|        - |  1941 | `		ph7_value *pValue;` |
|     2292 |  1942 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1943 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1944 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2291 |  1945 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1946 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1947 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2290 |  1948 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1949 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1950 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2290 |  1951 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1952 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1953 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2290 |  1954 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1955 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1956 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2290 |  1957 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1958 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1959 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1960 | `		}else{` |
|        - |  1961 | `			/* Extract the $_SERVER superglobal */` |
|     2290 |  1962 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1963 | `		}` |
|     2292 |  1964 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1965 | `			/* No such entry */` |
|      ! 0 |  1966 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1967 | `			break;` |
|        - |  1968 | `		}` |
|        - |  1969 | `		/* Point to the hashmap */` |
|     2292 |  1970 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1971 | `		/* Perform the insertion */` |
|     2292 |  1972 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2292 |  1973 | `		break;` |
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
|     2288 |  2024 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2025 | `		/* Register an IO stream device */` |
|     4578 |  2026 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2027 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6864 |  2028 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4578 |  2029 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2030 | `				/* Invalid stream */` |
|      ! 0 |  2031 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `		}` |
|     4578 |  2034 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2035 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2290 |  2036 | `			pVm->pDefStream = pStream;` |
|     1144 |  2037 | `		}` |
|        - |  2038 | `		/* Insert in the appropriate container */` |
|     4578 |  2039 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4578 |  2040 | `		break;` |
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
|    36634 |  2077 | `	return rc;` |
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
|    31082 |  2553 | `static sxi32 VmByteCodeExec(` |
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
|        - |  2566 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2567 | `	sxi32 pc;` |
|        - |  2568 | `	sxi32 rc;` |
|        - |  2569 | `	/* Argument container */` |
|    31084 |  2570 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    31084 |  2571 | `	if( nTos < 0 ){` |
|    29340 |  2572 | `		pTos = &pStack[-1];` |
|    14671 |  2573 | `	}else{` |
|     1746 |  2574 | `		pTos = &pStack[nTos];` |
|        - |  2575 | `	}` |
|    31084 |  2576 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    31084 |  2577 | `	pc = 0;` |
|        - |  2578 | `	/* Execute as much as we can */` |
|  4858204 |  2579 | `	for(;;){` |
|        - |  2580 | `		/* Fetch the instruction to execute */` |
|  9715706 |  2581 | `		pInstr = &aInstr[pc];` |
|  9715706 |  2582 | `		rc = SXRET_OK;` |
|        - |  2583 | `/*` |
|        - |  2584 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2585 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2586 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2587 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2588 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2589 | ` */` |
|  9715706 |  2590 | `		switch(pInstr->iOp){` |
|        - |  2591 | `/*` |
|        - |  2592 | ` * DONE: P1 * *` |
|        - |  2593 | ` *` |
|        - |  2594 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2595 | ` * and return immediately.` |
|        - |  2596 | ` */` |
|    15298 |  2597 | `case PH7_OP_DONE:` |
|    30598 |  2598 | `	if( pInstr->iP1 ){` |
|        - |  2599 | `#ifdef UNTRUST` |
|        - |  2600 | `		if( pTos < pStack ){` |
|        - |  2601 | `			goto Abort;` |
|        - |  2602 | `		}` |
|        - |  2603 | `#endif` |
|    17688 |  2604 | `		if( pLastRef ){` |
|    11514 |  2605 | `			*pLastRef = pTos->nIdx;` |
|     5756 |  2606 | `		}` |
|    17688 |  2607 | `		if( pResult ){` |
|        - |  2608 | `			/* Execution result */` |
|    16834 |  2609 | `			PH7_MemObjStore(pTos,pResult);` |
|     8416 |  2610 | `		}` |
|    17688 |  2611 | `		VmPopOperand(&pTos,1);` |
|    21755 |  2612 | `	}else if( pLastRef ){` |
|        - |  2613 | `		/* Nothing referenced */` |
|      956 |  2614 | `		*pLastRef = SXU32_HIGH;` |
|      477 |  2615 | `	}` |
|        - |  2616 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2617 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2618 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2619 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2620 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2621 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2622 | `	 * block can override it.` |
|        - |  2623 | `	 */` |
|    30600 |  2624 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2625 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2626 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2627 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2628 | `		pExc->pFrame = 0;` |
|        3 |  2629 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2630 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2631 | `			pExc->iFinallyDone = 1;` |
|        - |  2632 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2633 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2634 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2635 | `				goto Abort;` |
|        - |  2636 | `			}` |
|        1 |  2637 | `		}` |
|        1 |  2638 | `	}` |
|    30598 |  2639 | `	goto Done;` |
|        - |  2640 | `/*` |
|        - |  2641 | ` * HALT: P1 * *` |
|        - |  2642 | ` *` |
|        - |  2643 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2644 | ` * and abort immediately.` |
|        - |  2645 | ` */` |
|        4 |  2646 | `case PH7_OP_HALT:` |
|        9 |  2647 | `	if( pInstr->iP1 ){` |
|        - |  2648 | `#ifdef UNTRUST` |
|        - |  2649 | `		if( pTos < pStack ){` |
|        - |  2650 | `			goto Abort;` |
|        - |  2651 | `		}` |
|        - |  2652 | `#endif` |
|        9 |  2653 | `		if( pLastRef ){` |
|      ! 0 |  2654 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2655 | `		}` |
|        9 |  2656 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2657 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2658 | `				/* Output the exit message */` |
|        7 |  2659 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2660 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2661 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2662 | `					/* Increment output length */` |
|        5 |  2663 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2664 | `				}` |
|        3 |  2665 | `			}` |
|        7 |  2666 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2667 | `			/* Record exit status */` |
|        5 |  2668 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2669 | `		}` |
|        9 |  2670 | `		VmPopOperand(&pTos,1);` |
|        4 |  2671 | `	}else if( pLastRef ){` |
|        - |  2672 | `		/* Nothing referenced */` |
|      ! 0 |  2673 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2674 | `	}` |
|        - |  2675 | `	/* Check if we're in an included file context */` |
|        9 |  2676 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2677 | `		/* Terminate the entire process */` |
|        9 |  2678 | `		exit(pVm->iExitStatus);` |
|        - |  2679 | `	}` |
|      ! 0 |  2680 | `	goto Abort;` |
|        - |  2681 | `/*` |
|        - |  2682 | ` * JMP: * P2 *` |
|        - |  2683 | ` *` |
|        - |  2684 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2685 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2686 | ` */` |
|   209739 |  2687 | `case PH7_OP_JMP:` |
|   419524 |  2688 | `	pc = pInstr->iP2 - 1;` |
|   419524 |  2689 | `	break;` |
|        - |  2690 | `/*` |
|        - |  2691 | ` * JZ: P1 P2 *` |
|        - |  2692 | ` *` |
|        - |  2693 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2694 | ` * entry in the stack if P1 is zero.` |
|        - |  2695 | ` */` |
|   489395 |  2696 | `case PH7_OP_JZ:` |
|        - |  2697 | `#ifdef UNTRUST` |
|        - |  2698 | `	if( pTos < pStack ){` |
|        - |  2699 | `		goto Abort;` |
|        - |  2700 | `	}` |
|        - |  2701 | `#endif` |
|        - |  2702 | `	/* Get a boolean value */` |
|   978880 |  2703 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2704 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2705 | `	}` |
|   978880 |  2706 | `	if( !pTos->x.iVal ){` |
|        - |  2707 | `		/* Take the jump */` |
|   492642 |  2708 | `		pc = pInstr->iP2 - 1;` |
|   246320 |  2709 | `	}` |
|   978880 |  2710 | `	if( !pInstr->iP1 ){` |
|   781140 |  2711 | `		VmPopOperand(&pTos,1);` |
|   390591 |  2712 | `	}` |
|   978880 |  2713 | `	break;` |
|        - |  2714 | `/*` |
|        - |  2715 | ` * JNZ: P1 P2 *` |
|        - |  2716 | ` *` |
|        - |  2717 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2718 | ` * entry in the stack if P1 is zero.` |
|        - |  2719 | ` */` |
|    53316 |  2720 | `case PH7_OP_JNZ:` |
|        - |  2721 | `#ifdef UNTRUST` |
|        - |  2722 | `	if( pTos < pStack ){` |
|        - |  2723 | `		goto Abort;` |
|        - |  2724 | `	}` |
|        - |  2725 | `#endif` |
|        - |  2726 | `	/* Get a boolean value */` |
|   106634 |  2727 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2728 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2729 | `	}` |
|   106634 |  2730 | `	if( pTos->x.iVal ){` |
|        - |  2731 | `		/* Take the jump */` |
|     4316 |  2732 | `		pc = pInstr->iP2 - 1;` |
|     2157 |  2733 | `	}` |
|   106634 |  2734 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2735 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2736 | `	}` |
|   106634 |  2737 | `	break;` |
|        - |  2738 | `/*` |
|        - |  2739 | ` * NOOP: * * *` |
|        - |  2740 | ` *` |
|        - |  2741 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2742 | ` * destination.` |
|        - |  2743 | ` */` |
|      ! 0 |  2744 | `case PH7_OP_NOOP:` |
|      ! 0 |  2745 | `	break;` |
|        - |  2746 | `/*` |
|        - |  2747 | ` * POP: P1 * *` |
|        - |  2748 | ` *` |
|        - |  2749 | ` * Pop P1 elements from the operand stack.` |
|        - |  2750 | ` */` |
|   381062 |  2751 | `case PH7_OP_POP: {` |
|   762170 |  2752 | `	sxi32 n = pInstr->iP1;` |
|   762170 |  2753 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2754 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2755 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2756 | `	}` |
|   762170 |  2757 | `	VmPopOperand(&pTos,n);` |
|   762170 |  2758 | `	break;` |
|        - |  2759 | `				 }` |
|        - |  2760 | `/*` |
|        - |  2761 | ` * DUP: * * *` |
|        - |  2762 | ` *` |
|        - |  2763 | ` * Duplicate the top of the stack.` |
|        - |  2764 | ` */` |
|       35 |  2765 | `case PH7_OP_DUP:` |
|        - |  2766 | `#ifdef UNTRUST` |
|        - |  2767 | `	if( pTos < pStack ){` |
|        - |  2768 | `		goto Abort;` |
|        - |  2769 | `	}` |
|        - |  2770 | `#endif` |
|       72 |  2771 | `	pTos++;` |
|       72 |  2772 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2773 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2774 | `	break;` |
|        - |  2775 | `/*` |
|        - |  2776 | ` * NSSWITCH: * * P3` |
|        - |  2777 | ` *` |
|        - |  2778 | ` * Switch the active namespace at runtime.` |
|        - |  2779 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2780 | ` */` |
|     6218 |  2781 | `case PH7_OP_NSSWITCH:` |
|    12438 |  2782 | `	SyBlobReset(&pVm->sNamespace);` |
|    12438 |  2783 | `	if( pInstr->p3 ){` |
|       51 |  2784 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2785 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2786 | `	}` |
|    12438 |  2787 | `	break;` |
|        - |  2788 | `/*` |
|        - |  2789 | ` * CVT_INT: * * *` |
|        - |  2790 | ` *` |
|        - |  2791 | ` * Force the top of the stack to be an integer.` |
|        - |  2792 | ` */` |
|       35 |  2793 | `case PH7_OP_CVT_INT:` |
|        - |  2794 | `#ifdef UNTRUST` |
|        - |  2795 | `	if( pTos < pStack ){` |
|        - |  2796 | `		goto Abort;` |
|        - |  2797 | `	}` |
|        - |  2798 | `#endif` |
|       72 |  2799 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2800 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2801 | `	}` |
|        - |  2802 | `	/* Invalidate any prior representation */` |
|       72 |  2803 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2804 | `	break;` |
|        - |  2805 | `/*` |
|        - |  2806 | ` * CVT_REAL: * * *` |
|        - |  2807 | ` *` |
|        - |  2808 | ` * Force the top of the stack to be a real.` |
|        - |  2809 | ` */` |
|        4 |  2810 | `case PH7_OP_CVT_REAL:` |
|        - |  2811 | `#ifdef UNTRUST` |
|        - |  2812 | `	if( pTos < pStack ){` |
|        - |  2813 | `		goto Abort;` |
|        - |  2814 | `	}` |
|        - |  2815 | `#endif` |
|        9 |  2816 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2817 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2818 | `	}` |
|        - |  2819 | `	/* Invalidate any prior representation */` |
|        9 |  2820 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2821 | `	break;` |
|        - |  2822 | `/*` |
|        - |  2823 | ` * CVT_STR: * * *` |
|        - |  2824 | ` *` |
|        - |  2825 | ` * Force the top of the stack to be a string.` |
|        - |  2826 | ` */` |
|      146 |  2827 | `case PH7_OP_CVT_STR:` |
|        - |  2828 | `#ifdef UNTRUST` |
|        - |  2829 | `	if( pTos < pStack ){` |
|        - |  2830 | `		goto Abort;` |
|        - |  2831 | `	}` |
|        - |  2832 | `#endif` |
|      294 |  2833 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2834 | `		PH7_MemObjToString(pTos);` |
|      146 |  2835 | `	}` |
|      294 |  2836 | `	break;` |
|        - |  2837 | `/*` |
|        - |  2838 | ` * CVT_BOOL: * * *` |
|        - |  2839 | ` *` |
|        - |  2840 | ` * Force the top of the stack to be a boolean.` |
|        - |  2841 | ` */` |
|        5 |  2842 | `case PH7_OP_CVT_BOOL:` |
|        - |  2843 | `#ifdef UNTRUST` |
|        - |  2844 | `	if( pTos < pStack ){` |
|        - |  2845 | `		goto Abort;` |
|        - |  2846 | `	}` |
|        - |  2847 | `#endif` |
|       11 |  2848 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2849 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2850 | `	}` |
|       11 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * CVT_NULL: * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Nullify the top of the stack.` |
|        - |  2856 | ` */` |
|        3 |  2857 | `case PH7_OP_CVT_NULL:` |
|        - |  2858 | `#ifdef UNTRUST` |
|        - |  2859 | `	if( pTos < pStack ){` |
|        - |  2860 | `		goto Abort;` |
|        - |  2861 | `	}` |
|        - |  2862 | `#endif` |
|        7 |  2863 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2864 | `	break;` |
|        - |  2865 | `/*` |
|        - |  2866 | ` * CVT_NUMC: * * *` |
|        - |  2867 | ` *` |
|        - |  2868 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2869 | ` */` |
|      ! 0 |  2870 | `case PH7_OP_CVT_NUMC:` |
|        - |  2871 | `#ifdef UNTRUST` |
|        - |  2872 | `	if( pTos < pStack ){` |
|        - |  2873 | `		goto Abort;` |
|        - |  2874 | `	}` |
|        - |  2875 | `#endif` |
|        - |  2876 | `	/* Force a numeric cast */` |
|      ! 0 |  2877 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2878 | `	break;` |
|        - |  2879 | `/*` |
|        - |  2880 | ` * CVT_ARRAY: * * *` |
|        - |  2881 | ` *` |
|        - |  2882 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2883 | ` */` |
|       10 |  2884 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2885 | `#ifdef UNTRUST` |
|        - |  2886 | `	if( pTos < pStack ){` |
|        - |  2887 | `		goto Abort;` |
|        - |  2888 | `	}` |
|        - |  2889 | `#endif` |
|        - |  2890 | `	/* Force a hashmap cast */` |
|       21 |  2891 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2892 | `	if( rc != SXRET_OK ){` |
|        - |  2893 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2894 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2895 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2896 | `	}` |
|       21 |  2897 | `	break;` |
|        - |  2898 | `/*` |
|        - |  2899 | ` * CVT_OBJ: * * *` |
|        - |  2900 | ` *` |
|        - |  2901 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2902 | ` */` |
|        8 |  2903 | `case PH7_OP_CVT_OBJ:` |
|        - |  2904 | `#ifdef UNTRUST` |
|        - |  2905 | `	if( pTos < pStack ){` |
|        - |  2906 | `		goto Abort;` |
|        - |  2907 | `	}` |
|        - |  2908 | `#endif` |
|       17 |  2909 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2910 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2911 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2912 | `	}` |
|       17 |  2913 | `	break;` |
|        - |  2914 | `/*` |
|        - |  2915 | ` * ERR_CTRL * * *` |
|        - |  2916 | ` *` |
|        - |  2917 | ` * Error control operator.` |
|        - |  2918 | ` */` |
|    12363 |  2919 | `case PH7_OP_ERR_CTRL:` |
|        - |  2920 | `	/*` |
|        - |  2921 | `	 * TICKET 1433-038:` |
|        - |  2922 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2923 | `	 * use the public API,to control error output.` |
|        - |  2924 | `	 */` |
|    24726 |  2925 | `	break;` |
|        - |  2926 | `/*` |
|        - |  2927 | ` * IS_A * * *` |
|        - |  2928 | ` *` |
|        - |  2929 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2930 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2931 | ` * holding a class name or an object).` |
|        - |  2932 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2933 | ` */` |
|       23 |  2934 | `case PH7_OP_IS_A:{` |
|       48 |  2935 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  2936 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2937 | `#ifdef UNTRUST` |
|        - |  2938 | `	if( pNos < pStack ){` |
|        - |  2939 | `		goto Abort;` |
|        - |  2940 | `	}` |
|        - |  2941 | `#endif` |
|       48 |  2942 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  2943 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  2944 | `		ph7_class *pClass = 0;` |
|        - |  2945 | `		/* Extract the target class */` |
|       46 |  2946 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2947 | `			/* Instance already loaded */` |
|      ! 0 |  2948 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  2949 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  2950 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  2951 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  2952 | `			/* Handle self/static/parent keywords */` |
|       46 |  2953 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  2954 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  2955 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  2956 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  2957 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  2958 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  2959 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  2960 | `					pClass = pSelf->pBase;` |
|        2 |  2961 | `				}` |
|        3 |  2962 | `			}else{` |
|       36 |  2963 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  2964 | `			}` |
|       22 |  2965 | `		}` |
|       46 |  2966 | `		if( pClass ){` |
|        - |  2967 | `			/* Perform the query */` |
|       46 |  2968 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  2969 | `		}` |
|       22 |  2970 | `	}` |
|        - |  2971 | `	/* Push result */` |
|       48 |  2972 | `	VmPopOperand(&pTos,1);` |
|       48 |  2973 | `	PH7_MemObjRelease(pTos);` |
|       48 |  2974 | `	pTos->x.iVal = iRes;` |
|       48 |  2975 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  2976 | `	break;` |
|        - |  2977 | `				 }` |
|        - |  2978 |  |
|        - |  2979 | `/*` |
|        - |  2980 | ` * LOADC P1 P2 *` |
|        - |  2981 | ` *` |
|        - |  2982 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2983 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2984 | ` */` |
|   803772 |  2985 | `case PH7_OP_LOADC: {` |
|        - |  2986 | `	ph7_value *pObj;` |
|        - |  2987 | `	/* Reserve a room */` |
|  1607590 |  2988 | `	pTos++;` |
|  2403466 |  2989 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1607590 |  2990 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2991 | `			SyHashEntry *pEntry;` |
|        - |  2992 | `			/* Candidate for expansion via user defined callbacks */` |
|    15884 |  2993 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15884 |  2994 | `			if( pEntry ){` |
|    15880 |  2995 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2996 | `				/* Set a NULL default value */` |
|    15880 |  2997 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15880 |  2998 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2999 | `				/* Invoke the callback and deal with the expanded value */` |
|    15880 |  3000 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3001 | `				/* Mark as constant */` |
|    15880 |  3002 | `				pTos->nIdx = SXU32_HIGH;` |
|    15880 |  3003 | `				break;` |
|        - |  3004 | `			}` |
|        - |  3005 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3006 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3007 | `			 * through to string value for backward compatibility. */` |
|        - |  3008 | `			{` |
|        6 |  3009 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3010 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3011 | `				sxu32 j;` |
|       32 |  3012 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3013 | `					if( zLit[j] == '\\' ){` |
|        - |  3014 | `						/* Qualified name: must be a real constant.` |
|        - |  3015 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3016 | `						{` |
|        3 |  3017 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3018 | `							SyBlob sErr;` |
|        3 |  3019 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3020 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3021 | `							if( pErrFile ){` |
|        3 |  3022 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3023 | `							}` |
|        3 |  3024 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3025 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3026 | `							SyBlobRelease(&sErr);` |
|        - |  3027 | `						}` |
|        3 |  3028 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3029 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3030 | `						goto LoadC_Done;` |
|        - |  3031 | `					}` |
|       15 |  3032 | `				}` |
|        - |  3033 | `			}` |
|        1 |  3034 | `		}` |
|  1591710 |  3035 | `		PH7_MemObjLoad(pObj,pTos);` |
|   795878 |  3036 | `	}else{` |
|        - |  3037 | `		/* Set a NULL value */` |
|      ! 0 |  3038 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3039 | `	}` |
|   795833 |  3040 | `LoadC_Done:` |
|        - |  3041 | `	/* Mark as constant */` |
|  1591712 |  3042 | `	pTos->nIdx = SXU32_HIGH;` |
|  1591712 |  3043 | `	break;` |
|        - |  3044 | `				  }` |
|        - |  3045 | `/*` |
|        - |  3046 | ` * LOAD: P1 * P3` |
|        - |  3047 | ` *` |
|        - |  3048 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3049 | ` * from the P3 operand.` |
|        - |  3050 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3051 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3052 | ` */` |
|  1326384 |  3053 | `case PH7_OP_LOAD:{` |
|        - |  3054 | `	ph7_value *pObj;` |
|        - |  3055 | `	SyString sName;` |
|  2652990 |  3056 | `	if( pInstr->p3 == 0 ){` |
|        - |  3057 | `		/* Take the variable name from the top of the stack */` |
|        - |  3058 | `#ifdef UNTRUST` |
|        - |  3059 | `		if( pTos < pStack ){` |
|        - |  3060 | `			goto Abort;` |
|        - |  3061 | `		}` |
|        - |  3062 | `#endif` |
|        - |  3063 | `		/* Force a string cast */` |
|       19 |  3064 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3065 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3066 | `		}` |
|       19 |  3067 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3068 | `	}else{` |
|  2652972 |  3069 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3070 | `		/* Reserve a room for the target object */` |
|  2652972 |  3071 | `		pTos++;` |
|        - |  3072 | `	}` |
|        - |  3073 | `	/* Extract the requested memory object */` |
|  2652990 |  3074 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2652990 |  3075 | `	if( pObj == 0 ){` |
|      624 |  3076 | `		if( pInstr->iP1 ){` |
|        - |  3077 | `			/* Variable not found,load NULL */` |
|      624 |  3078 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3079 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3080 | `			}else{` |
|      624 |  3081 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3082 | `			}` |
|      624 |  3083 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1326697 |  3084 | `			break;` |
|      ! 0 |  3085 | `		}else{` |
|        - |  3086 | `			/* Fatal error */` |
|      ! 0 |  3087 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3088 | `			goto Abort;` |
|        - |  3089 | `		}` |
|        - |  3090 | `	}` |
|        - |  3091 | `	/* Load variable contents */` |
|  2652368 |  3092 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2652368 |  3093 | `	pTos->nIdx = pObj->nIdx;` |
|  2652368 |  3094 | `	break;` |
|        - |  3095 | `				   }` |
|        - |  3096 | `/*` |
|        - |  3097 | ` * LOAD_MAP P1 * *` |
|        - |  3098 | ` *` |
|        - |  3099 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3100 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3101 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3102 | ` */` |
|    17874 |  3103 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3104 | `	ph7_hashmap *pMap;` |
|        - |  3105 | `	/* Allocate a new hashmap instance */` |
|    35750 |  3106 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    35750 |  3107 | `	if( pMap == 0 ){` |
|      ! 0 |  3108 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3109 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3110 | `		goto Abort;` |
|        - |  3111 | `	}` |
|    35750 |  3112 | `	if( pInstr->iP1 > 0 ){` |
|     2180 |  3113 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3114 | `		/* Perform the insertion */` |
|     6638 |  3115 | `		while( pEntry < pTos ){` |
|     4460 |  3116 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3117 | `				/* Insertion by reference */` |
|      142 |  3118 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3119 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3120 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3121 | `					);` |
|       48 |  3122 | `			}else{` |
|        - |  3123 | `				/* Standard insertion */` |
|     6548 |  3124 | `				PH7_HashmapInsert(pMap,` |
|     4364 |  3125 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2182 |  3126 | `					&pEntry[1]` |
|        - |  3127 | `				);` |
|        - |  3128 | `			}` |
|        - |  3129 | `			/* Next pair on the stack */` |
|     4460 |  3130 | `			pEntry += 2;` |
|        2 |  3131 | `		}` |
|        - |  3132 | `		/* Pop P1 elements */` |
|     2180 |  3133 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1089 |  3134 | `	}` |
|        - |  3135 | `	/* Push the hashmap */` |
|    35750 |  3136 | `	pTos++;` |
|    35750 |  3137 | `	pTos->nIdx = SXU32_HIGH;` |
|    35750 |  3138 | `	pTos->x.pOther = pMap;` |
|    35750 |  3139 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    35750 |  3140 | `	break;` |
|        - |  3141 | `					  }` |
|        - |  3142 | `/*` |
|        - |  3143 | ` * LOAD_LIST: P1 * *` |
|        - |  3144 | ` *` |
|        - |  3145 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3146 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3147 | ` * Caveats:` |
|        - |  3148 | ` *  This implementation support only a single nesting level.` |
|        - |  3149 | ` */` |
|       26 |  3150 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3151 | `	ph7_value *pEntry;` |
|       54 |  3152 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3153 | `		/* Empty list,break immediately */` |
|      ! 0 |  3154 | `		break;` |
|        - |  3155 | `	}` |
|       54 |  3156 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3157 | `#ifdef UNTRUST` |
|        - |  3158 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3159 | `		goto Abort;` |
|        - |  3160 | `	}` |
|        - |  3161 | `#endif` |
|       54 |  3162 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       50 |  3163 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3164 | `		ph7_hashmap_node *pNode;` |
|        - |  3165 | `		ph7_value sKey,*pObj;` |
|        - |  3166 | `		/* Start Copying */` |
|       50 |  3167 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      154 |  3168 | `		while( pEntry <= pTos ){` |
|      106 |  3169 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       98 |  3170 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       98 |  3171 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       98 |  3172 | `					if( rc == SXRET_OK ){` |
|        - |  3173 | `						/* Store node value */` |
|       98 |  3174 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       50 |  3175 | `					}else{` |
|        - |  3176 | `						/* Nullify the variable */` |
|      ! 0 |  3177 | `						PH7_MemObjRelease(pObj);` |
|        - |  3178 | `					}` |
|       48 |  3179 | `				}` |
|       48 |  3180 | `			}` |
|      106 |  3181 | `			sKey.x.iVal++; /* Next numeric index */` |
|      106 |  3182 | `			pEntry++;` |
|        2 |  3183 | `		}` |
|       24 |  3184 | `	}` |
|       54 |  3185 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       54 |  3186 | `	break;` |
|        - |  3187 | `					   }` |
|        - |  3188 | `/*` |
|        - |  3189 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3190 | ` *` |
|        - |  3191 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3192 | ` * from the stack.` |
|        - |  3193 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3194 | ` * instead.` |
|        - |  3195 | ` */` |
|   214771 |  3196 | `case PH7_OP_LOAD_IDX: {` |
|   429588 |  3197 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   429588 |  3198 | `	ph7_hashmap *pMap = 0;` |
|        - |  3199 | `	ph7_value *pIdx;` |
|   429588 |  3200 | `	pIdx = 0;` |
|   429588 |  3201 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3202 | `		if( !pInstr->iP2){` |
|        - |  3203 | `			/* No available index,load NULL */` |
|      ! 0 |  3204 | `			if( pTos >= pStack ){` |
|      ! 0 |  3205 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3206 | `			}else{` |
|        - |  3207 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3208 | `				pTos++;` |
|      ! 0 |  3209 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3210 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3211 | `			}` |
|        - |  3212 | `			/* Emit a notice */` |
|      ! 0 |  3213 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3214 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3215 | `			break;` |
|        - |  3216 | `		}` |
|      ! 0 |  3217 | `	}else{` |
|   429588 |  3218 | `		pIdx = pTos;` |
|   429588 |  3219 | `		pTos--;` |
|        - |  3220 | `	}` |
|   429588 |  3221 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3222 | `		/* String access */` |
|   340922 |  3223 | `		if( pIdx ){` |
|        - |  3224 | `			sxu32 nOfft;` |
|   340922 |  3225 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3226 | `				/* Force an int cast */` |
|      ! 0 |  3227 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3228 | `			}` |
|   340922 |  3229 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340922 |  3230 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3231 | `				/* Invalid offset,load null */` |
|      ! 0 |  3232 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3233 | `			}else{` |
|   340922 |  3234 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340922 |  3235 | `				int c = zData[nOfft];` |
|   340922 |  3236 | `				PH7_MemObjRelease(pTos);` |
|   340922 |  3237 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340922 |  3238 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3239 | `			}` |
|   170484 |  3240 | `		}else{` |
|        - |  3241 | `			/* No available index,load NULL */` |
|      ! 0 |  3242 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3243 | `		}` |
|   340922 |  3244 | `		break;` |
|        - |  3245 | `	}` |
|    88668 |  3246 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3247 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3248 | `			ph7_value *pObj;` |
|      ! 0 |  3249 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3250 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3251 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3252 | `			}` |
|      ! 0 |  3253 | `		}` |
|      ! 0 |  3254 | `	}` |
|    88668 |  3255 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    88668 |  3256 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3257 | `		/* Point to the hashmap */` |
|    88668 |  3258 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    88668 |  3259 | `		if( pIdx ){` |
|        - |  3260 | `			/* Load the desired entry */` |
|    88668 |  3261 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    44333 |  3262 | `		}` |
|    88668 |  3263 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3264 | `			/* Create a new empty entry */` |
|      ! 0 |  3265 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3266 | `			if( rc == SXRET_OK ){` |
|        - |  3267 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3268 | `				pNode = pMap->pLast;` |
|      ! 0 |  3269 | `			}` |
|      ! 0 |  3270 | `		}` |
|    44333 |  3271 | `	}` |
|    88668 |  3272 | `	if( pIdx ){` |
|    88668 |  3273 | `		PH7_MemObjRelease(pIdx);` |
|    44333 |  3274 | `	}` |
|    88668 |  3275 | `	if( rc == SXRET_OK ){` |
|        - |  3276 | `		/* Load entry contents */` |
|    40376 |  3277 | `		if( pMap->iRef < 2 ){` |
|        - |  3278 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3279 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3280 | `			 */` |
|       24 |  3281 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3282 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3283 | `		}else{` |
|    40354 |  3284 | `			pTos->nIdx = pNode->nValIdx;` |
|    40354 |  3285 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    40354 |  3286 | `			PH7_HashmapUnref(pMap);` |
|        - |  3287 | `		}` |
|    20189 |  3288 | `	}else{` |
|        - |  3289 | `		/* No such entry,load NULL */` |
|    48294 |  3290 | `		PH7_MemObjRelease(pTos);` |
|    48294 |  3291 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3292 | `	}` |
|    88668 |  3293 | `	break;` |
|        - |  3294 | `					  }` |
|        - |  3295 | `/*` |
|        - |  3296 | ` * LOAD_CLOSURE * * P3` |
|        - |  3297 | ` *` |
|        - |  3298 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3299 | ` * name in the stack.` |
|        - |  3300 | ` */` |
|        2 |  3301 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3302 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3303 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3304 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3305 | `		ph7_vm_func *pClosure;` |
|        - |  3306 | `		char *zName;` |
|        - |  3307 | `		sxu32 mLen;` |
|        - |  3308 | `		sxu32 n;` |
|        - |  3309 | `		/* Create a new VM function */` |
|        5 |  3310 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3311 | `		/* Generate an unique closure name */` |
|        5 |  3312 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3313 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3314 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3315 | `			goto Abort;` |
|        - |  3316 | `		}` |
|        5 |  3317 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3318 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3319 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3320 | `		}` |
|        - |  3321 | `		/* Zero the stucture */` |
|        5 |  3322 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3323 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3324 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3325 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3326 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3327 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3328 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3329 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3330 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3331 | `		/* Register the closure */` |
|        5 |  3332 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3333 | `		/* Set up closure environment */` |
|        5 |  3334 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3335 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3336 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3337 | `			ph7_value *pValue;` |
|        9 |  3338 | `			pEnv = &aEnv[n];` |
|        9 |  3339 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3340 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3341 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3342 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3343 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3344 | `				/* Pass by reference */` |
|      ! 0 |  3345 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3346 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3347 | `					);` |
|      ! 0 |  3348 | `			}` |
|        - |  3349 | `			/* Standard pass by value */` |
|        9 |  3350 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3351 | `			if( pValue ){` |
|        - |  3352 | `				/* Copy imported value */` |
|        5 |  3353 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3354 | `			}` |
|        - |  3355 | `			/* Insert the imported variable */` |
|        9 |  3356 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3357 | `		}` |
|        - |  3358 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3359 | `		pTos++;` |
|        5 |  3360 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3361 | `	}` |
|        5 |  3362 | `	break;` |
|        - |  3363 | `						 }` |
|        - |  3364 | `/*` |
|        - |  3365 | ` * STORE * P2 P3` |
|        - |  3366 | ` *` |
|        - |  3367 | ` * Perform a store (Assignment) operation.` |
|        - |  3368 | ` */` |
|   110273 |  3369 | `case PH7_OP_STORE: {` |
|        - |  3370 | `	ph7_value *pObj;` |
|        - |  3371 | `	SyString sName;` |
|        - |  3372 | `#ifdef UNTRUST` |
|        - |  3373 | `	if( pTos < pStack ){` |
|        - |  3374 | `		goto Abort;` |
|        - |  3375 | `	}` |
|        - |  3376 | `#endif` |
|   220548 |  3377 | `	if( pInstr->iP2 ){` |
|        - |  3378 | `		sxu32 nIdx;` |
|        - |  3379 | `		/* Member store operation */` |
|     2922 |  3380 | `		nIdx = pTos->nIdx;` |
|     2922 |  3381 | `		VmPopOperand(&pTos,1);` |
|     2922 |  3382 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3383 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3384 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3385 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3386 | `		}else{` |
|        - |  3387 | `			/* Point to the desired memory object */` |
|     2918 |  3388 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2918 |  3389 | `			if( pObj ){` |
|        - |  3390 | `				/* Perform the store operation */` |
|     2918 |  3391 | `				PH7_MemObjStore(pTos,pObj);` |
|     1458 |  3392 | `			}` |
|        - |  3393 | `		}` |
|   111735 |  3394 | `		break;` |
|   217628 |  3395 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3396 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3397 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3398 | `			/* Force a string cast */` |
|      ! 0 |  3399 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3400 | `		}` |
|        7 |  3401 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3402 | `		pTos--;` |
|        - |  3403 | `#ifdef UNTRUST` |
|        - |  3404 | `		if( pTos < pStack  ){` |
|        - |  3405 | `			goto Abort;` |
|        - |  3406 | `		}` |
|        - |  3407 | `#endif` |
|        4 |  3408 | `	}else{` |
|   217622 |  3409 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3410 | `	}` |
|        - |  3411 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   217628 |  3412 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   217628 |  3413 | `	if( pObj == 0 ){` |
|      ! 0 |  3414 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3415 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3416 | `		goto Abort;` |
|        - |  3417 | `	}` |
|   217628 |  3418 | `	if( !pInstr->p3 ){` |
|        7 |  3419 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3420 | `	}` |
|        - |  3421 | `	/* Perform the store operation */` |
|   217628 |  3422 | `	PH7_MemObjStore(pTos,pObj);` |
|   217628 |  3423 | `	break;` |
|        - |  3424 | `				   }` |
|        - |  3425 | `/*` |
|        - |  3426 | ` * STORE_IDX:   P1 * P3` |
|        - |  3427 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3428 | ` *` |
|        - |  3429 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3430 | ` */` |
|    79940 |  3431 | `case PH7_OP_STORE_IDX:` |
|        - |  3432 | `case PH7_OP_STORE_IDX_REF: {` |
|   159882 |  3433 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3434 | `	ph7_value *pKey;` |
|        - |  3435 | `	sxu32 nIdx;` |
|   159882 |  3436 | `	if( pInstr->iP1 ){` |
|        - |  3437 | `		/* Key is next on stack */` |
|    56772 |  3438 | `		pKey = pTos;` |
|    56772 |  3439 | `		pTos--;` |
|    28387 |  3440 | `	}else{` |
|   103112 |  3441 | `		pKey = 0;` |
|        - |  3442 | `	}` |
|   159882 |  3443 | `	nIdx = pTos->nIdx;` |
|   159882 |  3444 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3445 | `		/* Hashmap already loaded */` |
|   159830 |  3446 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   159830 |  3447 | `		if( pMap->iRef < 2 ){` |
|        - |  3448 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3449 | `			pMap->iRef = 2;` |
|      ! 0 |  3450 | `		}` |
|    79916 |  3451 | `	}else{` |
|        - |  3452 | `		ph7_value *pObj;` |
|       53 |  3453 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3454 | `		if( pObj == 0 ){` |
|      ! 0 |  3455 | `			if( pKey ){` |
|      ! 0 |  3456 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3457 | `			}` |
|      ! 0 |  3458 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3459 | `			break;` |
|        - |  3460 | `		}` |
|        - |  3461 | `		/* Phase#1: Load the array */` |
|       53 |  3462 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3463 | `			VmPopOperand(&pTos,1);` |
|       53 |  3464 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3465 | `				/* Force a string cast */` |
|      ! 0 |  3466 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3467 | `			}` |
|       53 |  3468 | `			if( pKey == 0 ){` |
|        - |  3469 | `				/* Append string */` |
|        3 |  3470 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3471 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3472 | `				}` |
|        2 |  3473 | `			}else{` |
|        - |  3474 | `				sxu32 nOfft;` |
|       51 |  3475 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3476 | `					/* Force an int cast */` |
|       51 |  3477 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3478 | `				}` |
|       51 |  3479 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3480 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3481 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3482 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3483 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3484 | `				}else{` |
|      ! 0 |  3485 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3486 | `						/* Perform an append operation */` |
|      ! 0 |  3487 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3488 | `					}` |
|        - |  3489 | `				}` |
|        - |  3490 | `			}` |
|       53 |  3491 | `			if( pKey ){` |
|       51 |  3492 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3493 | `			}` |
|       53 |  3494 | `			break;` |
|      ! 0 |  3495 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3496 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3497 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3498 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3499 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3500 | `				goto Abort;` |
|        - |  3501 | `			}` |
|      ! 0 |  3502 | `		}` |
|      ! 0 |  3503 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3504 | `	}` |
|   159830 |  3505 | `	VmPopOperand(&pTos,1);` |
|        - |  3506 | `	/* Phase#2: Perform the insertion */` |
|   159830 |  3507 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3508 | `		/* Insertion by reference */` |
|       15 |  3509 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3510 | `	}else{` |
|   159816 |  3511 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3512 | `	}` |
|   159830 |  3513 | `	if( pKey ){` |
|    56722 |  3514 | `		PH7_MemObjRelease(pKey);` |
|    28360 |  3515 | `	}` |
|   159830 |  3516 | `	break;` |
|        - |  3517 | `					   }` |
|        - |  3518 | `/*` |
|        - |  3519 | ` * INCR: P1 * *` |
|        - |  3520 | ` *` |
|        - |  3521 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3522 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3523 | ` * the stack and increment after that.` |
|        - |  3524 | ` */` |
|   151251 |  3525 | `case PH7_OP_INCR:` |
|        - |  3526 | `#ifdef UNTRUST` |
|        - |  3527 | `	if( pTos < pStack ){` |
|        - |  3528 | `		goto Abort;` |
|        - |  3529 | `	}` |
|        - |  3530 | `#endif` |
|   302548 |  3531 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302548 |  3532 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3533 | `			ph7_value *pObj;` |
|   302548 |  3534 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3535 | `				/* Force a numeric cast */` |
|   302548 |  3536 | `				PH7_MemObjToNumeric(pObj);` |
|   302548 |  3537 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3538 | `					pObj->rVal++;` |
|        - |  3539 | `					/* Try to get an integer representation */` |
|      ! 0 |  3540 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3541 | `				}else{` |
|   302548 |  3542 | `					pObj->x.iVal++;` |
|   302548 |  3543 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3544 | `				}` |
|   302548 |  3545 | `				if( pInstr->iP1 ){` |
|        - |  3546 | `					/* Pre-icrement */` |
|       71 |  3547 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3548 | `				}` |
|   151295 |  3549 | `			}` |
|   151297 |  3550 | `		}else{` |
|      ! 0 |  3551 | `			if( pInstr->iP1 ){` |
|        - |  3552 | `				/* Force a numeric cast */` |
|      ! 0 |  3553 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3554 | `				/* Pre-increment */` |
|      ! 0 |  3555 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3556 | `					pTos->rVal++;` |
|        - |  3557 | `					/* Try to get an integer representation */` |
|      ! 0 |  3558 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3559 | `				}else{` |
|      ! 0 |  3560 | `					pTos->x.iVal++;` |
|      ! 0 |  3561 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3562 | `				}` |
|      ! 0 |  3563 | `			}` |
|        - |  3564 | `		}` |
|   151295 |  3565 | `	}` |
|   302548 |  3566 | `	break;` |
|        - |  3567 | `/*` |
|        - |  3568 | ` * DECR: P1 * *` |
|        - |  3569 | ` *` |
|        - |  3570 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3571 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3572 | ` * and decrement after that.` |
|        - |  3573 | ` */` |
|        2 |  3574 | `case PH7_OP_DECR:` |
|        - |  3575 | `#ifdef UNTRUST` |
|        - |  3576 | `	if( pTos < pStack ){` |
|        - |  3577 | `		goto Abort;` |
|        - |  3578 | `	}` |
|        - |  3579 | `#endif` |
|        5 |  3580 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3581 | `		/* Force a numeric cast */` |
|        5 |  3582 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3583 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3584 | `			ph7_value *pObj;` |
|        5 |  3585 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3586 | `				/* Force a numeric cast */` |
|        5 |  3587 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3588 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3589 | `					pObj->rVal--;` |
|        - |  3590 | `					/* Try to get an integer representation */` |
|      ! 0 |  3591 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3592 | `				}else{` |
|        5 |  3593 | `					pObj->x.iVal--;` |
|        5 |  3594 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3595 | `				}` |
|        5 |  3596 | `				if( pInstr->iP1 ){` |
|        - |  3597 | `					/* Pre-icrement */` |
|      ! 0 |  3598 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3599 | `				}` |
|        2 |  3600 | `			}` |
|        3 |  3601 | `		}else{` |
|      ! 0 |  3602 | `			if( pInstr->iP1 ){` |
|        - |  3603 | `				/* Pre-increment */` |
|      ! 0 |  3604 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3605 | `					pTos->rVal--;` |
|        - |  3606 | `					/* Try to get an integer representation */` |
|      ! 0 |  3607 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3608 | `				}else{` |
|      ! 0 |  3609 | `					pTos->x.iVal--;` |
|      ! 0 |  3610 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3611 | `				}` |
|      ! 0 |  3612 | `			}` |
|        - |  3613 | `		}` |
|        2 |  3614 | `	}` |
|        5 |  3615 | `	break;` |
|        - |  3616 | `/*` |
|        - |  3617 | ` * UMINUS: * * *` |
|        - |  3618 | ` *` |
|        - |  3619 | ` * Perform a unary minus operation.` |
|        - |  3620 | ` */` |
|    23111 |  3621 | `case PH7_OP_UMINUS:` |
|        - |  3622 | `#ifdef UNTRUST` |
|        - |  3623 | `	if( pTos < pStack ){` |
|        - |  3624 | `		goto Abort;` |
|        - |  3625 | `	}` |
|        - |  3626 | `#endif` |
|        - |  3627 | `	/* Force a numeric (integer,real or both) cast */` |
|    46224 |  3628 | `	PH7_MemObjToNumeric(pTos);` |
|    46224 |  3629 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3630 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3631 | `	}` |
|    46224 |  3632 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    46194 |  3633 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23096 |  3634 | `	}` |
|    46224 |  3635 | `	break;` |
|        - |  3636 | `/*` |
|        - |  3637 | ` * UPLUS: * * *` |
|        - |  3638 | ` *` |
|        - |  3639 | ` * Perform a unary plus operation.` |
|        - |  3640 | ` */` |
|       16 |  3641 | `case PH7_OP_UPLUS:` |
|        - |  3642 | `#ifdef UNTRUST` |
|        - |  3643 | `	if( pTos < pStack ){` |
|        - |  3644 | `		goto Abort;` |
|        - |  3645 | `	}` |
|        - |  3646 | `#endif` |
|        - |  3647 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3648 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3649 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3650 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3651 | `	}` |
|       33 |  3652 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3653 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3654 | `	}` |
|       33 |  3655 | `	break;` |
|        - |  3656 | `/*` |
|        - |  3657 | ` * OP_LNOT: * * *` |
|        - |  3658 | ` *` |
|        - |  3659 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3660 | ` * with its complement.` |
|        - |  3661 | ` */` |
|    39557 |  3662 | `case PH7_OP_LNOT:` |
|        - |  3663 | `#ifdef UNTRUST` |
|        - |  3664 | `	if( pTos < pStack ){` |
|        - |  3665 | `		goto Abort;` |
|        - |  3666 | `	}` |
|        - |  3667 | `#endif` |
|        - |  3668 | `	/* Force a boolean cast */` |
|    79160 |  3669 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3670 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3671 | `	}` |
|    79160 |  3672 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    79160 |  3673 | `	break;` |
|        - |  3674 | `/*` |
|        - |  3675 | ` * OP_BITNOT: * * *` |
|        - |  3676 | ` *` |
|        - |  3677 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3678 | ` * with its ones-complement.` |
|        - |  3679 | ` */` |
|       14 |  3680 | `case PH7_OP_BITNOT:` |
|        - |  3681 | `#ifdef UNTRUST` |
|        - |  3682 | `	if( pTos < pStack ){` |
|        - |  3683 | `		goto Abort;` |
|        - |  3684 | `	}` |
|        - |  3685 | `#endif` |
|        - |  3686 | `	/* Force an integer cast */` |
|       30 |  3687 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3688 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3689 | `	}` |
|       30 |  3690 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3691 | `	break;` |
|        - |  3692 | `/* OP_MUL * * *` |
|        - |  3693 | ` * OP_MUL_STORE * * *` |
|        - |  3694 | ` *` |
|        - |  3695 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3696 | ` * and push the result back onto the stack.` |
|        - |  3697 | ` */` |
|     1240 |  3698 | `case PH7_OP_MUL:` |
|        - |  3699 | `case PH7_OP_MUL_STORE: {` |
|     2482 |  3700 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3701 | `	/* Force the operand to be numeric */` |
|        - |  3702 | `#ifdef UNTRUST` |
|        - |  3703 | `	if( pNos < pStack ){` |
|        - |  3704 | `		goto Abort;` |
|        - |  3705 | `	}` |
|        - |  3706 | `#endif` |
|     2482 |  3707 | `	PH7_MemObjToNumeric(pTos);` |
|     2482 |  3708 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3709 | `	/* Perform the requested operation */` |
|     2482 |  3710 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3711 | `		/* Floating point arithemic */` |
|        - |  3712 | `		ph7_real a,b,r;` |
|       17 |  3713 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3714 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3715 | `		}` |
|       17 |  3716 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3717 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3718 | `		}` |
|       17 |  3719 | `		a = pNos->rVal;` |
|       17 |  3720 | `		b = pTos->rVal;` |
|       17 |  3721 | `		r = a * b;` |
|        - |  3722 | `		/* Push the result */` |
|       17 |  3723 | `		pNos->rVal = r;` |
|       17 |  3724 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3725 | `		/* Try to get an integer representation */` |
|       17 |  3726 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3727 | `	}else{` |
|        - |  3728 | `		/* Integer arithmetic */` |
|        - |  3729 | `		sxi64 a,b,r;` |
|     2466 |  3730 | `		a = pNos->x.iVal;` |
|     2466 |  3731 | `		b = pTos->x.iVal;` |
|     2466 |  3732 | `		r = a * b;` |
|        - |  3733 | `		/* Push the result */` |
|     2466 |  3734 | `		pNos->x.iVal = r;` |
|     2466 |  3735 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3736 | `	}` |
|     2482 |  3737 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3738 | `		ph7_value *pObj;` |
|       19 |  3739 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3740 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3741 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3742 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3743 | `		}` |
|        9 |  3744 | `	}` |
|     2482 |  3745 | `	VmPopOperand(&pTos,1);` |
|     2482 |  3746 | `	break;` |
|        - |  3747 | `				 }` |
|        - |  3748 | `/* OP_ADD * * *` |
|        - |  3749 | ` *` |
|        - |  3750 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3751 | ` * and push the result back onto the stack.` |
|        - |  3752 | ` */` |
|      429 |  3753 | `case PH7_OP_ADD:{` |
|      860 |  3754 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3755 | `#ifdef UNTRUST` |
|        - |  3756 | `	if( pNos < pStack ){` |
|        - |  3757 | `		goto Abort;` |
|        - |  3758 | `	}` |
|        - |  3759 | `#endif` |
|        - |  3760 | `	/* Perform the addition */` |
|      860 |  3761 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      860 |  3762 | `	VmPopOperand(&pTos,1);` |
|      860 |  3763 | `	break;` |
|        - |  3764 | `				}` |
|        - |  3765 | `/*` |
|        - |  3766 | ` * OP_ADD_STORE * * *` |
|        - |  3767 | ` *` |
|        - |  3768 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3769 | ` * and push the result back onto the stack.` |
|        - |  3770 | ` */` |
|      482 |  3771 | `case PH7_OP_ADD_STORE:{` |
|      966 |  3772 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3773 | `	ph7_value *pObj;` |
|        - |  3774 | `	sxu32 nIdx;` |
|        - |  3775 | `#ifdef UNTRUST` |
|        - |  3776 | `	if( pNos < pStack ){` |
|        - |  3777 | `		goto Abort;` |
|        - |  3778 | `	}` |
|        - |  3779 | `#endif` |
|        - |  3780 | `	/* Perform the addition */` |
|      966 |  3781 | `	nIdx = pTos->nIdx;` |
|      966 |  3782 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3783 | `	/* Peform the store operation */` |
|      966 |  3784 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3785 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      966 |  3786 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      966 |  3787 | `		PH7_MemObjStore(pTos,pObj);` |
|      482 |  3788 | `	}` |
|        - |  3789 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      966 |  3790 | `	PH7_MemObjStore(pTos,pNos);` |
|      966 |  3791 | `	VmPopOperand(&pTos,1);` |
|      966 |  3792 | `	break;` |
|        - |  3793 | `				}` |
|        - |  3794 | `/* OP_SUB * * *` |
|        - |  3795 | ` *` |
|        - |  3796 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3797 | ` * first (what was next on the stack) from the second (the` |
|        - |  3798 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3799 | ` */` |
|      299 |  3800 | `case PH7_OP_SUB: {` |
|      600 |  3801 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3802 | `#ifdef UNTRUST` |
|        - |  3803 | `	if( pNos < pStack ){` |
|        - |  3804 | `		goto Abort;` |
|        - |  3805 | `	}` |
|        - |  3806 | `#endif` |
|      600 |  3807 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3808 | `		/* Floating point arithemic */` |
|        - |  3809 | `		ph7_real a,b,r;` |
|       95 |  3810 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3811 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3812 | `		}` |
|       95 |  3813 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3814 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3815 | `		}` |
|       95 |  3816 | `		a = pNos->rVal;` |
|       95 |  3817 | `		b = pTos->rVal;` |
|       95 |  3818 | `		r = a - b;` |
|        - |  3819 | `		/* Push the result */` |
|       95 |  3820 | `		pNos->rVal = r;` |
|       95 |  3821 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3822 | `		/* Try to get an integer representation */` |
|       95 |  3823 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3824 | `	}else{` |
|        - |  3825 | `		/* Integer arithmetic */` |
|        - |  3826 | `		sxi64 a,b,r;` |
|      506 |  3827 | `		a = pNos->x.iVal;` |
|      506 |  3828 | `		b = pTos->x.iVal;` |
|      506 |  3829 | `		r = a - b;` |
|        - |  3830 | `		/* Push the result */` |
|      506 |  3831 | `		pNos->x.iVal = r;` |
|      506 |  3832 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3833 | `	}` |
|      600 |  3834 | `	VmPopOperand(&pTos,1);` |
|      600 |  3835 | `	break;` |
|        - |  3836 | `				 }` |
|        - |  3837 | `/* OP_SUB_STORE * * *` |
|        - |  3838 | ` *` |
|        - |  3839 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3840 | ` * first (what was next on the stack) from the second (the` |
|        - |  3841 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3842 | ` */` |
|        1 |  3843 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3844 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3845 | `	ph7_value *pObj;` |
|        - |  3846 | `#ifdef UNTRUST` |
|        - |  3847 | `	if( pNos < pStack ){` |
|        - |  3848 | `		goto Abort;` |
|        - |  3849 | `	}` |
|        - |  3850 | `#endif` |
|        3 |  3851 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3852 | `		/* Floating point arithemic */` |
|        - |  3853 | `		ph7_real a,b,r;` |
|      ! 0 |  3854 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3855 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3856 | `		}` |
|      ! 0 |  3857 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3858 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3859 | `		}` |
|      ! 0 |  3860 | `		a = pTos->rVal;` |
|      ! 0 |  3861 | `		b = pNos->rVal;` |
|      ! 0 |  3862 | `		r = a - b;` |
|        - |  3863 | `		/* Push the result */` |
|      ! 0 |  3864 | `		pNos->rVal = r;` |
|      ! 0 |  3865 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3866 | `		/* Try to get an integer representation */` |
|      ! 0 |  3867 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3868 | `	}else{` |
|        - |  3869 | `		/* Integer arithmetic */` |
|        - |  3870 | `		sxi64 a,b,r;` |
|        3 |  3871 | `		a = pTos->x.iVal;` |
|        3 |  3872 | `		b = pNos->x.iVal;` |
|        3 |  3873 | `		r = a - b;` |
|        - |  3874 | `		/* Push the result */` |
|        3 |  3875 | `		pNos->x.iVal = r;` |
|        3 |  3876 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3877 | `	}` |
|        3 |  3878 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3879 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3880 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3881 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3882 | `	}` |
|        3 |  3883 | `	VmPopOperand(&pTos,1);` |
|        3 |  3884 | `	break;` |
|        - |  3885 | `				 }` |
|        - |  3886 |  |
|        - |  3887 | `/*` |
|        - |  3888 | ` * OP_MOD * * *` |
|        - |  3889 | ` *` |
|        - |  3890 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3891 | ` * first (what was next on the stack) from the second (the` |
|        - |  3892 | ` * top of the stack) and push the remainder after division` |
|        - |  3893 | ` * onto the stack.` |
|        - |  3894 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3895 | ` */` |
|      296 |  3896 | `case PH7_OP_MOD:{` |
|      594 |  3897 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3898 | `	sxi64 a,b,r;` |
|        - |  3899 | `#ifdef UNTRUST` |
|        - |  3900 | `	if( pNos < pStack ){` |
|        - |  3901 | `		goto Abort;` |
|        - |  3902 | `	}` |
|        - |  3903 | `#endif` |
|        - |  3904 | `	/* Force the operands to be integer */` |
|      594 |  3905 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3906 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3907 | `	}` |
|      594 |  3908 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3909 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3910 | `	}` |
|        - |  3911 | `	/* Perform the requested operation */` |
|      594 |  3912 | `	a = pNos->x.iVal;` |
|      594 |  3913 | `	b = pTos->x.iVal;` |
|      594 |  3914 | `	if( b == 0 ){` |
|        3 |  3915 | `		r = 0;` |
|        3 |  3916 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3917 | `		/* goto Abort; */` |
|        2 |  3918 | `	}else{` |
|      591 |  3919 | `		r = a%b;` |
|        - |  3920 | `	}` |
|        - |  3921 | `	/* Push the result */` |
|      594 |  3922 | `	pNos->x.iVal = r;` |
|      594 |  3923 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3924 | `	VmPopOperand(&pTos,1);` |
|      594 |  3925 | `	break;` |
|        - |  3926 | `				}` |
|        - |  3927 | `/*` |
|        - |  3928 | ` * OP_MOD_STORE * * *` |
|        - |  3929 | ` *` |
|        - |  3930 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3931 | ` * first (what was next on the stack) from the second (the` |
|        - |  3932 | ` * top of the stack) and push the remainder after division` |
|        - |  3933 | ` * onto the stack.` |
|        - |  3934 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3935 | ` */` |
|        1 |  3936 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3937 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3938 | `	ph7_value *pObj;` |
|        - |  3939 | `	sxi64 a,b,r;` |
|        - |  3940 | `#ifdef UNTRUST` |
|        - |  3941 | `	if( pNos < pStack ){` |
|        - |  3942 | `		goto Abort;` |
|        - |  3943 | `	}` |
|        - |  3944 | `#endif` |
|        - |  3945 | `	/* Force the operands to be integer */` |
|        3 |  3946 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3947 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3948 | `	}` |
|        3 |  3949 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3950 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3951 | `	}` |
|        - |  3952 | `	/* Perform the requested operation */` |
|        3 |  3953 | `	a = pTos->x.iVal;` |
|        3 |  3954 | `	b = pNos->x.iVal;` |
|        3 |  3955 | `	if( b == 0 ){` |
|      ! 0 |  3956 | `		r = 0;` |
|      ! 0 |  3957 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3958 | `		/* goto Abort; */` |
|      ! 0 |  3959 | `	}else{` |
|        3 |  3960 | `		r = a%b;` |
|        - |  3961 | `	}` |
|        - |  3962 | `	/* Push the result */` |
|        3 |  3963 | `	pNos->x.iVal = r;` |
|        3 |  3964 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3965 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3966 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3967 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3968 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3969 | `	}` |
|        3 |  3970 | `	VmPopOperand(&pTos,1);` |
|        3 |  3971 | `	break;` |
|        - |  3972 | `				}` |
|        - |  3973 | `/*` |
|        - |  3974 | ` * OP_DIV * * *` |
|        - |  3975 | ` *` |
|        - |  3976 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3977 | ` * first (what was next on the stack) from the second (the` |
|        - |  3978 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3979 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3980 | ` */` |
|       28 |  3981 | `case PH7_OP_DIV:{` |
|       58 |  3982 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3983 | `	ph7_real a,b,r;` |
|        - |  3984 | `#ifdef UNTRUST` |
|        - |  3985 | `	if( pNos < pStack ){` |
|        - |  3986 | `		goto Abort;` |
|        - |  3987 | `	}` |
|        - |  3988 | `#endif` |
|        - |  3989 | `	/* Force the operands to be real */` |
|       58 |  3990 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3991 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3992 | `	}` |
|       58 |  3993 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3994 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3995 | `	}` |
|        - |  3996 | `	/* Perform the requested operation */` |
|       58 |  3997 | `	a = pNos->rVal;` |
|       58 |  3998 | `	b = pTos->rVal;` |
|       58 |  3999 | `	if( b == 0 ){` |
|        - |  4000 | `		/* Division by zero */` |
|        3 |  4001 | `		pNos->rVal = 0;` |
|        3 |  4002 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4003 | `		/* goto Abort; */` |
|        2 |  4004 | `	}else{` |
|       55 |  4005 | `		r = a/b;` |
|        - |  4006 | `		/* Push the result */` |
|       55 |  4007 | `		pNos->rVal = r;` |
|       55 |  4008 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4009 | `		/* Try to get an integer representation */` |
|       55 |  4010 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4011 | `	}` |
|       58 |  4012 | `	VmPopOperand(&pTos,1);` |
|       58 |  4013 | `	break;` |
|        - |  4014 | `				}` |
|        - |  4015 | `/*` |
|        - |  4016 | ` * OP_DIV_STORE * * *` |
|        - |  4017 | ` *` |
|        - |  4018 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4019 | ` * first (what was next on the stack) from the second (the` |
|        - |  4020 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4021 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4022 | ` */` |
|        1 |  4023 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4024 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4025 | `	ph7_value *pObj;` |
|        - |  4026 | `	ph7_real a,b,r;` |
|        - |  4027 | `#ifdef UNTRUST` |
|        - |  4028 | `	if( pNos < pStack ){` |
|        - |  4029 | `		goto Abort;` |
|        - |  4030 | `	}` |
|        - |  4031 | `#endif` |
|        - |  4032 | `	/* Force the operands to be real */` |
|        3 |  4033 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4034 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4035 | `	}` |
|        3 |  4036 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4037 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4038 | `	}` |
|        - |  4039 | `	/* Perform the requested operation */` |
|        3 |  4040 | `	a = pTos->rVal;` |
|        3 |  4041 | `	b = pNos->rVal;` |
|        3 |  4042 | `	if( b == 0 ){` |
|        - |  4043 | `		/* Division by zero */` |
|      ! 0 |  4044 | `		r = 0;` |
|      ! 0 |  4045 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4046 | `		/* goto Abort; */` |
|      ! 0 |  4047 | `	}else{` |
|        3 |  4048 | `		r = a/b;` |
|        - |  4049 | `		/* Push the result */` |
|        3 |  4050 | `		pNos->rVal = r;` |
|        3 |  4051 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4052 | `		/* Try to get an integer representation */` |
|        3 |  4053 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4054 | `	}` |
|        3 |  4055 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4056 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4057 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4058 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4059 | `	}` |
|        3 |  4060 | `	VmPopOperand(&pTos,1);` |
|        3 |  4061 | `	break;` |
|        - |  4062 | `				}` |
|        - |  4063 | `/* OP_BAND * * *` |
|        - |  4064 | ` *` |
|        - |  4065 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4066 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4067 | ` * two elements.` |
|        - |  4068 | `*/` |
|        - |  4069 | `/* OP_BOR * * *` |
|        - |  4070 | ` *` |
|        - |  4071 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4072 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4073 | ` * two elements.` |
|        - |  4074 | ` */` |
|        - |  4075 | `/* OP_BXOR * * *` |
|        - |  4076 | ` *` |
|        - |  4077 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4078 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4079 | ` * two elements.` |
|        - |  4080 | ` */` |
|       30 |  4081 | `case PH7_OP_BAND:` |
|        - |  4082 | `case PH7_OP_BOR:` |
|        - |  4083 | `case PH7_OP_BXOR:{` |
|       62 |  4084 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4085 | `	sxi64 a,b,r;` |
|        - |  4086 | `#ifdef UNTRUST` |
|        - |  4087 | `	if( pNos < pStack ){` |
|        - |  4088 | `		goto Abort;` |
|        - |  4089 | `	}` |
|        - |  4090 | `#endif` |
|        - |  4091 | `	/* Force the operands to be integer */` |
|       62 |  4092 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4093 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4094 | `	}` |
|       62 |  4095 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4096 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4097 | `	}` |
|        - |  4098 | `	/* Perform the requested operation */` |
|       62 |  4099 | `	a = pNos->x.iVal;` |
|       62 |  4100 | `	b = pTos->x.iVal;` |
|       62 |  4101 | `	switch(pInstr->iOp){` |
|        6 |  4102 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4103 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4104 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4105 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4106 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4107 | `	case PH7_OP_BAND:` |
|       38 |  4108 | `	default:          r = a&b; break;` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Push the result */` |
|       62 |  4111 | `	pNos->x.iVal = r;` |
|       62 |  4112 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4113 | `	VmPopOperand(&pTos,1);` |
|       62 |  4114 | `	break;` |
|        - |  4115 | `				 }` |
|        - |  4116 | `/* OP_BAND_STORE * * *` |
|        - |  4117 | ` *` |
|        - |  4118 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4119 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4120 | ` * two elements.` |
|        - |  4121 | `*/` |
|        - |  4122 | `/* OP_BOR_STORE * * *` |
|        - |  4123 | ` *` |
|        - |  4124 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4125 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4126 | ` * two elements.` |
|        - |  4127 | ` */` |
|        - |  4128 | `/* OP_BXOR_STORE * * *` |
|        - |  4129 | ` *` |
|        - |  4130 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4131 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4132 | ` * two elements.` |
|        - |  4133 | ` */` |
|        7 |  4134 | `case PH7_OP_BAND_STORE:` |
|        - |  4135 | `case PH7_OP_BOR_STORE:` |
|        - |  4136 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4137 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4138 | `	ph7_value *pObj;` |
|        - |  4139 | `	sxi64 a,b,r;` |
|        - |  4140 | `#ifdef UNTRUST` |
|        - |  4141 | `	if( pNos < pStack ){` |
|        - |  4142 | `		goto Abort;` |
|        - |  4143 | `	}` |
|        - |  4144 | `#endif` |
|        - |  4145 | `	/* Force the operands to be integer */` |
|       15 |  4146 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4147 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4148 | `	}` |
|       15 |  4149 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4150 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4151 | `	}` |
|        - |  4152 | `	/* Perform the requested operation */` |
|       15 |  4153 | `	a = pTos->x.iVal;` |
|       15 |  4154 | `	b = pNos->x.iVal;` |
|       15 |  4155 | `	switch(pInstr->iOp){` |
|        2 |  4156 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4157 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4158 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4159 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4160 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4161 | `	case PH7_OP_BAND:` |
|        5 |  4162 | `	default:          r = a&b; break;` |
|        - |  4163 | `	}` |
|        - |  4164 | `	/* Push the result */` |
|       15 |  4165 | `	pNos->x.iVal = r;` |
|       15 |  4166 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4167 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4168 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4169 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4170 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4171 | `	}` |
|       15 |  4172 | `	VmPopOperand(&pTos,1);` |
|       15 |  4173 | `	break;` |
|        - |  4174 | `				 }` |
|        - |  4175 | `/* OP_SHL * * *` |
|        - |  4176 | ` *` |
|        - |  4177 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4178 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4179 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4180 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4181 | ` */` |
|        - |  4182 | `/* OP_SHR * * *` |
|        - |  4183 | ` *` |
|        - |  4184 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4185 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4186 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4187 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4188 | ` */` |
|        9 |  4189 | `case PH7_OP_SHL:` |
|        - |  4190 | `case PH7_OP_SHR: {` |
|       19 |  4191 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4192 | `	sxi64 a,r;` |
|        - |  4193 | `	sxi32 b;` |
|        - |  4194 | `#ifdef UNTRUST` |
|        - |  4195 | `	if( pNos < pStack ){` |
|        - |  4196 | `		goto Abort;` |
|        - |  4197 | `	}` |
|        - |  4198 | `#endif` |
|        - |  4199 | `	/* Force the operands to be integer */` |
|       19 |  4200 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4201 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4202 | `	}` |
|       19 |  4203 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4204 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4205 | `	}` |
|        - |  4206 | `	/* Perform the requested operation */` |
|       19 |  4207 | `	a = pNos->x.iVal;` |
|       19 |  4208 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4209 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4210 | `		r = a << b;` |
|        6 |  4211 | `	}else{` |
|        9 |  4212 | `		r = a >> b;` |
|        - |  4213 | `	}` |
|        - |  4214 | `	/* Push the result */` |
|       19 |  4215 | `	pNos->x.iVal = r;` |
|       19 |  4216 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4217 | `	VmPopOperand(&pTos,1);` |
|       19 |  4218 | `	break;` |
|        - |  4219 | `				 }` |
|        - |  4220 | `/*  OP_SHL_STORE * * *` |
|        - |  4221 | ` *` |
|        - |  4222 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4223 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4224 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4225 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4226 | ` */` |
|        - |  4227 | `/* OP_SHR_STORE * * *` |
|        - |  4228 | ` *` |
|        - |  4229 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4230 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4231 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4232 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4233 | ` */` |
|        7 |  4234 | `case PH7_OP_SHL_STORE:` |
|        - |  4235 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4236 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4237 | `	ph7_value *pObj;` |
|        - |  4238 | `	sxi64 a,r;` |
|        - |  4239 | `	sxi32 b;` |
|        - |  4240 | `#ifdef UNTRUST` |
|        - |  4241 | `	if( pNos < pStack ){` |
|        - |  4242 | `		goto Abort;` |
|        - |  4243 | `	}` |
|        - |  4244 | `#endif` |
|        - |  4245 | `	/* Force the operands to be integer */` |
|       15 |  4246 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4247 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4248 | `	}` |
|       15 |  4249 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4250 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4251 | `	}` |
|        - |  4252 | `	/* Perform the requested operation */` |
|       15 |  4253 | `	a = pTos->x.iVal;` |
|       15 |  4254 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4255 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4256 | `		r = a << b;` |
|        4 |  4257 | `	}else{` |
|        9 |  4258 | `		r = a >> b;` |
|        - |  4259 | `	}` |
|        - |  4260 | `	/* Push the result */` |
|       15 |  4261 | `	pNos->x.iVal = r;` |
|       15 |  4262 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4263 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4264 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4265 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4266 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4267 | `	}` |
|       15 |  4268 | `	VmPopOperand(&pTos,1);` |
|       15 |  4269 | `	break;` |
|        - |  4270 | `				 }` |
|        - |  4271 | `/* CAT:  P1 * *` |
|        - |  4272 | ` *` |
|        - |  4273 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4274 | ` * back.` |
|        - |  4275 | ` */` |
|    61552 |  4276 | `case PH7_OP_CAT:{` |
|        - |  4277 | `	ph7_value *pNos,*pCur;` |
|   123106 |  4278 | `	if( pInstr->iP1 < 1 ){` |
|    96148 |  4279 | `		pNos = &pTos[-1];` |
|    48075 |  4280 | `	}else{` |
|    26960 |  4281 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4282 | `	}` |
|        - |  4283 | `#ifdef UNTRUST` |
|        - |  4284 | `	if( pNos < pStack ){` |
|        - |  4285 | `		goto Abort;` |
|        - |  4286 | `	}` |
|        - |  4287 | `#endif` |
|        - |  4288 | `	/* Force a string cast */` |
|   123106 |  4289 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1018 |  4290 | `		PH7_MemObjToString(pNos);` |
|      508 |  4291 | `	}` |
|   123106 |  4292 | `	pCur = &pNos[1];` |
|   248174 |  4293 | `	while( pCur <= pTos ){` |
|   125070 |  4294 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50520 |  4295 | `			PH7_MemObjToString(pCur);` |
|    25259 |  4296 | `		}` |
|        - |  4297 | `		/* Perform the concatenation */` |
|   125070 |  4298 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   125032 |  4299 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    62515 |  4300 | `		}` |
|   125070 |  4301 | `		SyBlobRelease(&pCur->sBlob);` |
|   125070 |  4302 | `		pCur++;` |
|        2 |  4303 | `	}` |
|   123106 |  4304 | `	pTos = pNos;` |
|   123106 |  4305 | `	break;` |
|        - |  4306 | `				}` |
|        - |  4307 | `/*  CAT_STORE: * * *` |
|        - |  4308 | ` *` |
|        - |  4309 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4310 | ` * back.` |
|        - |  4311 | ` */` |
|     3357 |  4312 | `case PH7_OP_CAT_STORE:{` |
|     6716 |  4313 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4314 | `	ph7_value *pObj;` |
|        - |  4315 | `#ifdef UNTRUST` |
|        - |  4316 | `	if( pNos < pStack ){` |
|        - |  4317 | `		goto Abort;` |
|        - |  4318 | `	}` |
|        - |  4319 | `#endif` |
|     6716 |  4320 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4321 | `		/* Force a string cast */` |
|      ! 0 |  4322 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4323 | `	}` |
|     6716 |  4324 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4325 | `		/* Force a string cast */` |
|      ! 0 |  4326 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4327 | `	}` |
|        - |  4328 | `	/* Perform the concatenation (Reverse order) */` |
|     6716 |  4329 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6716 |  4330 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3357 |  4331 | `	}` |
|        - |  4332 | `	/* Perform the store operation */` |
|     6716 |  4333 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4334 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6716 |  4335 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6716 |  4336 | `		PH7_MemObjStore(pTos,pObj);` |
|     3357 |  4337 | `	}` |
|     6716 |  4338 | `	PH7_MemObjStore(pTos,pNos);` |
|     6716 |  4339 | `	VmPopOperand(&pTos,1);` |
|     6716 |  4340 | `	break;` |
|        - |  4341 | `				}` |
|        - |  4342 | `/* OP_AND: * * *` |
|        - |  4343 | ` *` |
|        - |  4344 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4345 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4346 | ` * stack.` |
|        - |  4347 | ` */` |
|        - |  4348 | `/* OP_OR: * * *` |
|        - |  4349 | ` *` |
|        - |  4350 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4351 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4352 | ` * stack.` |
|        - |  4353 | ` */` |
|    93989 |  4354 | `case PH7_OP_LAND:` |
|        - |  4355 | `case PH7_OP_LOR: {` |
|   188024 |  4356 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4357 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4358 | `#ifdef UNTRUST` |
|        - |  4359 | `	if( pNos < pStack ){` |
|        - |  4360 | `		goto Abort;` |
|        - |  4361 | `	}` |
|        - |  4362 | `#endif` |
|        - |  4363 | `	/* Force a boolean cast */` |
|   188024 |  4364 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4365 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4366 | `	}` |
|   188024 |  4367 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4368 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4369 | `	}` |
|   188024 |  4370 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   188024 |  4371 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   188024 |  4372 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4373 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    85706 |  4374 | `		v1 = and_logic[v1*3+v2];` |
|    42876 |  4375 | `	}else{` |
|        - |  4376 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102320 |  4377 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4378 | `	}` |
|   188024 |  4379 | `	if( v1 == 2 ){` |
|      ! 0 |  4380 | `		v1 = 1;` |
|      ! 0 |  4381 | `	}` |
|   188024 |  4382 | `	VmPopOperand(&pTos,1);` |
|   188024 |  4383 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   188024 |  4384 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   188024 |  4385 | `	break;` |
|        - |  4386 | `				 }` |
|        - |  4387 | `/* OP_LXOR: * * *` |
|        - |  4388 | ` *` |
|        - |  4389 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4390 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4391 | ` * stack.` |
|        - |  4392 | ` * According to the PHP language reference manual:` |
|        - |  4393 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4394 | ` *  TRUE,but not both.` |
|        - |  4395 | ` */` |
|        5 |  4396 | `case PH7_OP_LXOR:{` |
|       11 |  4397 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4398 | `	sxi32 v = 0;` |
|        - |  4399 | `#ifdef UNTRUST` |
|        - |  4400 | `	if( pNos < pStack ){` |
|        - |  4401 | `		goto Abort;` |
|        - |  4402 | `	}` |
|        - |  4403 | `#endif` |
|        - |  4404 | `	/* Force a boolean cast */` |
|       11 |  4405 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4406 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4407 | `	}` |
|       11 |  4408 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4409 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4410 | `	}` |
|       11 |  4411 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4412 | `		v = 1;` |
|        3 |  4413 | `	}` |
|       11 |  4414 | `	VmPopOperand(&pTos,1);` |
|       11 |  4415 | `	pTos->x.iVal = v;` |
|       11 |  4416 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4417 | `	break;` |
|        - |  4418 | `				 }` |
|        - |  4419 | `/* OP_EQ P1 P2 P3` |
|        - |  4420 | ` *` |
|        - |  4421 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4422 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4423 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4424 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4425 | ` */` |
|        - |  4426 | `/* OP_NEQ P1 P2 P3` |
|        - |  4427 | ` *` |
|        - |  4428 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4429 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4430 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4431 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4432 | ` */` |
|     3819 |  4433 | `case PH7_OP_EQ:` |
|        - |  4434 | `case PH7_OP_NEQ: {` |
|     7640 |  4435 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4436 | `	/* Perform the comparison and act accordingly */` |
|        - |  4437 | `#ifdef UNTRUST` |
|        - |  4438 | `	if( pNos < pStack ){` |
|        - |  4439 | `		goto Abort;` |
|        - |  4440 | `	}` |
|        - |  4441 | `#endif` |
|     7640 |  4442 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7640 |  4443 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4444 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7631 |  4445 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7596 |  4446 | `		rc = rc == 0;` |
|     3799 |  4447 | `	}else{` |
|       28 |  4448 | `		rc = rc != 0;` |
|        - |  4449 | `	}` |
|     7640 |  4450 | `	VmPopOperand(&pTos,1);` |
|     7640 |  4451 | `	if( !pInstr->iP2 ){` |
|        - |  4452 | `		/* Push comparison result without taking the jump */` |
|     7640 |  4453 | `		PH7_MemObjRelease(pTos);` |
|     7640 |  4454 | `		pTos->x.iVal = rc;` |
|        - |  4455 | `		/* Invalidate any prior representation */` |
|     7640 |  4456 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3821 |  4457 | `	}else{` |
|      ! 0 |  4458 | `		if( rc ){` |
|        - |  4459 | `			/* Jump to the desired location */` |
|      ! 0 |  4460 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4461 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4462 | `		}` |
|        - |  4463 | `	}` |
|     7640 |  4464 | `	break;` |
|        - |  4465 | `				 }` |
|        - |  4466 | `/* OP_TEQ P1 P2 *` |
|        - |  4467 | ` *` |
|        - |  4468 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4469 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4470 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4471 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4472 | ` */` |
|   129112 |  4473 | `case PH7_OP_TEQ: {` |
|   258226 |  4474 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4475 | `	/* Perform the comparison and act accordingly */` |
|        - |  4476 | `#ifdef UNTRUST` |
|        - |  4477 | `	if( pNos < pStack ){` |
|        - |  4478 | `		goto Abort;` |
|        - |  4479 | `	}` |
|        - |  4480 | `#endif` |
|   258226 |  4481 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   258226 |  4482 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4483 | `		rc = 0;` |
|        2 |  4484 | `	}else{` |
|   258224 |  4485 | `		rc = rc == 0;` |
|        - |  4486 | `	}` |
|   258226 |  4487 | `	VmPopOperand(&pTos,1);` |
|   258226 |  4488 | `	if( !pInstr->iP2 ){` |
|        - |  4489 | `		/* Push comparison result without taking the jump */` |
|   258226 |  4490 | `		PH7_MemObjRelease(pTos);` |
|   258226 |  4491 | `		pTos->x.iVal = rc;` |
|        - |  4492 | `		/* Invalidate any prior representation */` |
|   258226 |  4493 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   129114 |  4494 | `	}else{` |
|      ! 0 |  4495 | `		if( rc ){` |
|        - |  4496 | `			/* Jump to the desired location */` |
|      ! 0 |  4497 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4498 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4499 | `		}` |
|        - |  4500 | `	}` |
|   258226 |  4501 | `	break;` |
|        - |  4502 | `				 }` |
|        - |  4503 | `/* OP_TNE P1 P2 *` |
|        - |  4504 | ` *` |
|        - |  4505 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4506 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4507 | ` * instruction.` |
|        - |  4508 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4509 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4510 | ` *` |
|        - |  4511 | ` */` |
|   100788 |  4512 | `case PH7_OP_TNE: {` |
|   201578 |  4513 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4514 | `	/* Perform the comparison and act accordingly */` |
|        - |  4515 | `#ifdef UNTRUST` |
|        - |  4516 | `	if( pNos < pStack ){` |
|        - |  4517 | `		goto Abort;` |
|        - |  4518 | `	}` |
|        - |  4519 | `#endif` |
|   201578 |  4520 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   201578 |  4521 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4522 | `		rc = 1;` |
|        2 |  4523 | `	}else{` |
|   201576 |  4524 | `		rc = rc != 0;` |
|        - |  4525 | `	}` |
|   201578 |  4526 | `	VmPopOperand(&pTos,1);` |
|   201578 |  4527 | `	if( !pInstr->iP2 ){` |
|        - |  4528 | `		/* Push comparison result without taking the jump */` |
|   201578 |  4529 | `		PH7_MemObjRelease(pTos);` |
|   201578 |  4530 | `		pTos->x.iVal = rc;` |
|        - |  4531 | `		/* Invalidate any prior representation */` |
|   201578 |  4532 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100790 |  4533 | `	}else{` |
|      ! 0 |  4534 | `		if( rc ){` |
|        - |  4535 | `			/* Jump to the desired location */` |
|      ! 0 |  4536 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4537 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4538 | `		}` |
|        - |  4539 | `	}` |
|   201578 |  4540 | `	break;` |
|        - |  4541 | `				 }` |
|        - |  4542 | `/* OP_LT P1 P2 P3` |
|        - |  4543 | ` *` |
|        - |  4544 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4545 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4546 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4547 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4548 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4549 | ` *` |
|        - |  4550 | ` */` |
|        - |  4551 | `/* OP_LE P1 P2 P3` |
|        - |  4552 | ` *` |
|        - |  4553 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4554 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4555 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4556 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4557 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4558 | ` *` |
|        - |  4559 | ` */` |
|   102469 |  4560 | `case PH7_OP_LT:` |
|        - |  4561 | `case PH7_OP_LE: {` |
|   204984 |  4562 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4563 | `	/* Perform the comparison and act accordingly */` |
|        - |  4564 | `#ifdef UNTRUST` |
|        - |  4565 | `	if( pNos < pStack ){` |
|        - |  4566 | `		goto Abort;` |
|        - |  4567 | `	}` |
|        - |  4568 | `#endif` |
|   204984 |  4569 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204984 |  4570 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4571 | `		rc = 0;` |
|   204980 |  4572 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4573 | `		rc = rc < 1;` |
|      205 |  4574 | `	}else{` |
|   204570 |  4575 | `		rc = rc < 0;` |
|        - |  4576 | `	}` |
|   204984 |  4577 | `	VmPopOperand(&pTos,1);` |
|   204984 |  4578 | `	if( !pInstr->iP2 ){` |
|        - |  4579 | `		/* Push comparison result without taking the jump */` |
|   204984 |  4580 | `		PH7_MemObjRelease(pTos);` |
|   204984 |  4581 | `		pTos->x.iVal = rc;` |
|        - |  4582 | `		/* Invalidate any prior representation */` |
|   204984 |  4583 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102515 |  4584 | `	}else{` |
|      ! 0 |  4585 | `		if( rc ){` |
|        - |  4586 | `			/* Jump to the desired location */` |
|      ! 0 |  4587 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4588 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4589 | `		}` |
|        - |  4590 | `	}` |
|   204984 |  4591 | `	break;` |
|        - |  4592 | `				}` |
|        - |  4593 | `/* OP_GT P1 P2 P3` |
|        - |  4594 | ` *` |
|        - |  4595 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4596 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4597 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4598 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4599 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4600 | ` *` |
|        - |  4601 | ` */` |
|        - |  4602 | `/* OP_GE P1 P2 P3` |
|        - |  4603 | ` *` |
|        - |  4604 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4605 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4606 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4607 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4608 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4609 | ` *` |
|        - |  4610 | ` */` |
|    48811 |  4611 | `case PH7_OP_GT:` |
|        - |  4612 | `case PH7_OP_GE: {` |
|    97624 |  4613 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4614 | `	/* Perform the comparison and act accordingly */` |
|        - |  4615 | `#ifdef UNTRUST` |
|        - |  4616 | `	if( pNos < pStack ){` |
|        - |  4617 | `		goto Abort;` |
|        - |  4618 | `	}` |
|        - |  4619 | `#endif` |
|    97624 |  4620 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97624 |  4621 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4622 | `		rc = 0;` |
|    97620 |  4623 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97468 |  4624 | `		rc = rc >= 0;` |
|    48735 |  4625 | `	}else{` |
|      150 |  4626 | `		rc = rc > 0;` |
|        - |  4627 | `	}` |
|    97624 |  4628 | `	VmPopOperand(&pTos,1);` |
|    97624 |  4629 | `	if( !pInstr->iP2 ){` |
|        - |  4630 | `		/* Push comparison result without taking the jump */` |
|    97624 |  4631 | `		PH7_MemObjRelease(pTos);` |
|    97624 |  4632 | `		pTos->x.iVal = rc;` |
|        - |  4633 | `		/* Invalidate any prior representation */` |
|    97624 |  4634 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48813 |  4635 | `	}else{` |
|      ! 0 |  4636 | `		if( rc ){` |
|        - |  4637 | `			/* Jump to the desired location */` |
|      ! 0 |  4638 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4639 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4640 | `		}` |
|        - |  4641 | `	}` |
|    97624 |  4642 | `	break;` |
|        - |  4643 | `				}` |
|        - |  4644 | `/* OP_SEQ P1 P2 *` |
|        - |  4645 | ` * Strict string comparison.` |
|        - |  4646 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4647 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4648 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4649 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4650 | ` * use PH7_OP_EQ.` |
|        - |  4651 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4652 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4653 | ` */` |
|        - |  4654 | `/* OP_SNE P1 P2 *` |
|        - |  4655 | ` * Strict string comparison.` |
|        - |  4656 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4657 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4658 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4659 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4660 | ` * use PH7_OP_EQ.` |
|        - |  4661 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4662 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4663 | ` */` |
|       18 |  4664 | `case PH7_OP_SEQ:` |
|        - |  4665 | `case PH7_OP_SNE: {` |
|       38 |  4666 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4667 | `	SyString s1,s2;` |
|        - |  4668 | `	/* Perform the comparison and act accordingly */` |
|        - |  4669 | `#ifdef UNTRUST` |
|        - |  4670 | `	if( pNos < pStack ){` |
|        - |  4671 | `		goto Abort;` |
|        - |  4672 | `	}` |
|        - |  4673 | `#endif` |
|        - |  4674 | `	/* Force a string cast */` |
|       38 |  4675 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4676 | `		PH7_MemObjToString(pTos);` |
|        2 |  4677 | `	}` |
|       38 |  4678 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4679 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4680 | `	}` |
|       38 |  4681 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4682 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4683 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4684 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4685 | `		rc = rc != 0;` |
|      ! 0 |  4686 | `	}else{` |
|       38 |  4687 | `		rc = rc == 0;` |
|        - |  4688 | `	}` |
|       38 |  4689 | `	VmPopOperand(&pTos,1);` |
|       38 |  4690 | `	if( !pInstr->iP2 ){` |
|        - |  4691 | `		/* Push comparison result without taking the jump */` |
|       38 |  4692 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4693 | `		pTos->x.iVal = rc;` |
|        - |  4694 | `		/* Invalidate any prior representation */` |
|       38 |  4695 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4696 | `	}else{` |
|      ! 0 |  4697 | `		if( rc ){` |
|        - |  4698 | `			/* Jump to the desired location */` |
|      ! 0 |  4699 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4700 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4701 | `		}` |
|        - |  4702 | `	}` |
|       38 |  4703 | `	break;` |
|        - |  4704 | `				 }` |
|        - |  4705 | `/*` |
|        - |  4706 | ` * OP_LOAD_REF * * *` |
|        - |  4707 | ` * Push the index of a referenced object on the stack.` |
|        - |  4708 | ` */` |
|       57 |  4709 | `case PH7_OP_LOAD_REF: {` |
|        - |  4710 | `	sxu32 nIdx;` |
|        - |  4711 | `#ifdef UNTRUST` |
|        - |  4712 | `	if( pTos < pStack ){` |
|        - |  4713 | `		goto Abort;` |
|        - |  4714 | `	}` |
|        - |  4715 | `#endif` |
|        - |  4716 | `	/* Extract memory object index */` |
|      115 |  4717 | `	nIdx = pTos->nIdx;` |
|      115 |  4718 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4719 | `		/* Nullify the object */` |
|       95 |  4720 | `		PH7_MemObjRelease(pTos);` |
|        - |  4721 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4722 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4723 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4724 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4725 | `	}` |
|      115 |  4726 | `	break;` |
|        - |  4727 | `					  }` |
|        - |  4728 | `/*` |
|        - |  4729 | ` * OP_STORE_REF * * P3` |
|        - |  4730 | ` * Perform an assignment operation by reference.` |
|        - |  4731 | ` */` |
|       14 |  4732 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4733 | `	 SyString sName = { 0 , 0 };` |
|        - |  4734 | `	 VmFrame *pFrameLocal;` |
|        - |  4735 | `	SyHashEntry *pEntry;` |
|        - |  4736 | `	sxu32 nIdx;` |
|        - |  4737 | `#ifdef UNTRUST` |
|        - |  4738 | `	if( pTos < pStack ){` |
|        - |  4739 | `		goto Abort;` |
|        - |  4740 | `	}` |
|        - |  4741 | `#endif` |
|       30 |  4742 | `	if( pInstr->p3 == 0 ){` |
|        - |  4743 | `		char *zName;` |
|        - |  4744 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4745 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4746 | `			/* Force a string cast */` |
|      ! 0 |  4747 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4748 | `		}` |
|      ! 0 |  4749 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4750 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4751 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4752 | `			if( zName ){` |
|      ! 0 |  4753 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4754 | `			}` |
|      ! 0 |  4755 | `		}` |
|      ! 0 |  4756 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4757 | `		pTos--;` |
|      ! 0 |  4758 | `	}else{` |
|       30 |  4759 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4760 | `	}` |
|       30 |  4761 | `	nIdx = pTos->nIdx;` |
|       30 |  4762 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4763 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4764 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4765 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4766 | `		}else{` |
|        - |  4767 | `			ph7_value *pObj;` |
|        - |  4768 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4769 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4770 | `			if( pObj == 0 ){` |
|      ! 0 |  4771 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4772 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4773 | `				goto Abort;` |
|        - |  4774 | `			}` |
|        - |  4775 | `			/* Perform the store operation */` |
|      ! 0 |  4776 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4777 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4778 | `		}` |
|       30 |  4779 | `	}else if( sName.nByte > 0){` |
|       30 |  4780 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4781 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4782 | `		}else{` |
|       30 |  4783 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4784 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4785 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4786 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4787 | `			}` |
|        - |  4788 | `			/* Query the local frame */` |
|       30 |  4789 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4790 | `			if( pEntry ){` |
|      ! 0 |  4791 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4792 | `			}else{` |
|       30 |  4793 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4794 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4795 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4796 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4797 | `				}` |
|       30 |  4798 | `				if( rc == SXRET_OK ){` |
|       30 |  4799 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4800 | `				}` |
|        - |  4801 | `			}` |
|        - |  4802 | `		}` |
|       14 |  4803 | `	}` |
|       30 |  4804 | `	break;` |
|        - |  4805 | `				 }` |
|        - |  4806 | `/*` |
|        - |  4807 | ` * OP_UPLINK P1 * *` |
|        - |  4808 | ` * Link a variable to the top active VM frame.` |
|        - |  4809 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4810 | ` */` |
|       25 |  4811 | `case PH7_OP_UPLINK: {` |
|       52 |  4812 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4813 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4814 | `		SyString sName;` |
|        - |  4815 | `		/* Perform the link */` |
|      104 |  4816 | `		while( pLink <= pTos ){` |
|       54 |  4817 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4818 | `				/* Force a string cast */` |
|      ! 0 |  4819 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4820 | `			}` |
|       54 |  4821 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4822 | `			if( sName.nByte > 0 ){` |
|       54 |  4823 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4824 | `			}` |
|       54 |  4825 | `			pLink++;` |
|        2 |  4826 | `		}` |
|       25 |  4827 | `	}` |
|       52 |  4828 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4829 | `	break;` |
|        - |  4830 | `					}` |
|        - |  4831 | `/*` |
|        - |  4832 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4833 | ` * Push an exception in the corresponding container so that` |
|        - |  4834 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4835 | ` */` |
|       29 |  4836 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       60 |  4837 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4838 | `	VmFrame *pFrameLocal;` |
|        - |  4839 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       60 |  4840 | `	pException->iFinallyDone = 0;` |
|       60 |  4841 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4842 | `	/* Create the exception frame */` |
|       60 |  4843 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       60 |  4844 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4845 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4846 | `		goto Abort;` |
|        - |  4847 | `	}` |
|        - |  4848 | `	/* Mark the special frame */` |
|       60 |  4849 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       60 |  4850 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4851 | `	/* Point to the frame that trigger the exception */` |
|       60 |  4852 | `	pFrameLocal = pFrameLocal->pParent;` |
|       68 |  4853 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       10 |  4854 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4855 | `	}` |
|       60 |  4856 | `	pException->pFrame = pFrameLocal;` |
|       60 |  4857 | `	break;` |
|        - |  4858 | `							}` |
|        - |  4859 | `/*` |
|        - |  4860 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4861 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4862 | ` */` |
|       28 |  4863 | `case PH7_OP_POP_EXCEPTION: {` |
|       58 |  4864 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       58 |  4865 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4866 | `		ph7_exception **apException;` |
|        - |  4867 | `		/* Pop the loaded exception */` |
|       28 |  4868 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  4869 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  4870 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  4871 | `		}` |
|       13 |  4872 | `	}` |
|       58 |  4873 | `	pException->pFrame = 0;` |
|        - |  4874 | `	/* Leave the exception frame */` |
|       58 |  4875 | `	VmLeaveFrame(&(*pVm));` |
|        - |  4876 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       58 |  4877 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  4878 | `		sxi32 rcFinally;` |
|       19 |  4879 | `		pException->iFinallyDone = 1;` |
|       19 |  4880 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  4881 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  4882 | `			goto Abort;` |
|        - |  4883 | `		}` |
|        9 |  4884 | `	}` |
|       58 |  4885 | `	break;` |
|        - |  4886 | `							}` |
|        - |  4887 |  |
|        - |  4888 | `/*` |
|        - |  4889 | ` * OP_THROW * P2 *` |
|        - |  4890 | ` * Throw an user exception.` |
|        - |  4891 | ` */` |
|       17 |  4892 | `case PH7_OP_THROW: {` |
|       36 |  4893 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  4894 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4895 | `#ifdef UNTRUST` |
|        - |  4896 | `	if( pTos < pStack ){` |
|        - |  4897 | `		goto Abort;` |
|        - |  4898 | `	}` |
|        - |  4899 | `#endif` |
|       56 |  4900 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4901 | `		/* Safely ignore the exception frame */` |
|       22 |  4902 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4903 | `	}` |
|        - |  4904 | `	/* Tell the upper layer that an exception was thrown */` |
|       36 |  4905 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       36 |  4906 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       36 |  4907 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4908 | `		ph7_class *pException;` |
|        - |  4909 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4910 | `		 */` |
|       36 |  4911 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       36 |  4912 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4913 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4914 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4915 | `			if( rc == SXERR_ABORT ){` |
|        - |  4916 | `				/* Abort processing immediately */` |
|      ! 0 |  4917 | `				goto Abort;` |
|        - |  4918 | `			}` |
|      ! 0 |  4919 | `		}else{` |
|        - |  4920 | `			/* Throw the exception */` |
|       36 |  4921 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       36 |  4922 | `			if( rc == SXERR_ABORT ){` |
|        - |  4923 | `				/* Abort processing immediately */` |
|        9 |  4924 | `				goto Abort;` |
|        - |  4925 | `			}` |
|        - |  4926 | `		}` |
|       15 |  4927 | `	}else{` |
|        - |  4928 | `		/* Expecting a class instance */` |
|      ! 0 |  4929 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4930 | `		if( rc == SXERR_ABORT ){` |
|        - |  4931 | `			/* Abort processing immediately */` |
|      ! 0 |  4932 | `			goto Abort;` |
|        - |  4933 | `		}` |
|        - |  4934 | `	}` |
|        - |  4935 | `	/* Pop the top entry */` |
|       28 |  4936 | `	VmPopOperand(&pTos,1);` |
|        - |  4937 | `	/* Perform an unconditional jump */` |
|       28 |  4938 | `	pc = nJump - 1;` |
|       28 |  4939 | `	break;` |
|        - |  4940 | `				   }` |
|        - |  4941 | `/*` |
|        - |  4942 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4943 | ` * Prepare a foreach step.` |
|        - |  4944 | ` */` |
|     4805 |  4945 | `case PH7_OP_FOREACH_INIT: {` |
|     9612 |  4946 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4947 | `	void *pName;` |
|        - |  4948 | `#ifdef UNTRUST` |
|        - |  4949 | `	if( pTos < pStack ){` |
|        - |  4950 | `		goto Abort;` |
|        - |  4951 | `	}` |
|        - |  4952 | `#endif` |
|     9612 |  4953 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4954 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4955 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4956 | `			/* Force a string cast */` |
|      ! 0 |  4957 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4958 | `		}` |
|        - |  4959 | `		/* Duplicate name */` |
|      ! 0 |  4960 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4961 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4962 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4963 | `		}` |
|      ! 0 |  4964 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4965 | `	}` |
|     9612 |  4966 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4967 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4968 | `			/* Force a string cast */` |
|      ! 0 |  4969 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4970 | `		}` |
|        - |  4971 | `		/* Duplicate name */` |
|      ! 0 |  4972 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4973 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4974 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4975 | `		}` |
|      ! 0 |  4976 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4977 | `	}` |
|        - |  4978 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9612 |  4979 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4980 | `		/* Jump out of the loop */` |
|      ! 0 |  4981 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4982 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4983 | `		}` |
|      ! 0 |  4984 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4985 | `	}else{` |
|        - |  4986 | `		ph7_foreach_step *pStep;` |
|     9612 |  4987 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9612 |  4988 | `		if( pStep == 0 ){` |
|      ! 0 |  4989 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4990 | `			/* Jump out of the loop */` |
|      ! 0 |  4991 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4992 | `		}else{` |
|        - |  4993 | `			/* Zero the structure */` |
|     9612 |  4994 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4995 | `			/* Prepare the step */` |
|     9612 |  4996 | `			pStep->iFlags = pInfo->iFlags;` |
|     9612 |  4997 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9596 |  4998 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4999 | `				/* Reset the internal loop cursor */` |
|     9596 |  5000 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5001 | `				/* Mark the step */` |
|     9596 |  5002 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9596 |  5003 | `				pStep->xIter.pMap = pMap;` |
|     9596 |  5004 | `				pMap->iRef++;` |
|     4799 |  5005 | `			}else{` |
|       18 |  5006 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5007 | `				ph7_class *pIteratorClass;` |
|        - |  5008 | `				/* Check if the object implements Iterator */` |
|       18 |  5009 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       21 |  5010 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5011 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5012 | `					ph7_class_method *pRewind;` |
|        7 |  5013 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        7 |  5014 | `					pStep->xIter.pThis = pThis;` |
|        7 |  5015 | `					pThis->iRef++;` |
|        7 |  5016 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|        7 |  5017 | `					if( pRewind ){` |
|        7 |  5018 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        3 |  5019 | `					}` |
|        4 |  5020 | `				}else{` |
|        - |  5021 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5022 | `					ph7_class *pIterAggClass;` |
|       12 |  5023 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5024 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5025 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5026 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5027 | `						ph7_class_method *pGetIter;` |
|        3 |  5028 | `						int iterAggOk = 0;` |
|        3 |  5029 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5030 | `						if( pGetIter ){` |
|        - |  5031 | `							ph7_value sResult;` |
|        3 |  5032 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5033 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5034 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5035 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5036 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5037 | `									ph7_class_method *pRewind;` |
|        3 |  5038 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5039 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5040 | `									pIterObj->iRef++;` |
|        - |  5041 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5042 | `									pStep->pOwner = pThis;` |
|        3 |  5043 | `									pThis->iRef++;` |
|        3 |  5044 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5045 | `									if( pRewind ){` |
|        3 |  5046 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5047 | `									}` |
|        3 |  5048 | `									iterAggOk = 1;` |
|        1 |  5049 | `								}` |
|        1 |  5050 | `							}` |
|        3 |  5051 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5052 | `						}` |
|        3 |  5053 | `						if( !iterAggOk ){` |
|        - |  5054 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5055 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5056 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5057 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5058 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5059 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5060 | `						}` |
|        2 |  5061 | `					}else{` |
|        - |  5062 | `						/* Plain object iteration via hAttr */` |
|        9 |  5063 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5064 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5065 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5066 | `						pThis->iRef++;` |
|        - |  5067 | `					}` |
|        - |  5068 | `				}` |
|        - |  5069 | `			}` |
|        - |  5070 | `		}` |
|     9612 |  5071 | `		if( pStep ){` |
|     9612 |  5072 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5073 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5074 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5075 | `				/* Jump out of the loop */` |
|      ! 0 |  5076 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5077 | `			}` |
|     4805 |  5078 | `		}` |
|        - |  5079 | `	}` |
|     9612 |  5080 | `	VmPopOperand(&pTos,1);` |
|     9612 |  5081 | `	break;` |
|        - |  5082 | `						  }` |
|        - |  5083 | `/*` |
|        - |  5084 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5085 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5086 | ` */` |
|    77130 |  5087 | `case PH7_OP_FOREACH_STEP: {` |
|   154262 |  5088 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5089 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5090 | `	ph7_value *pValue;` |
|        - |  5091 | `	VmFrame *pFrameLocal;` |
|        - |  5092 | `	/* Peek the last step */` |
|   154262 |  5093 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   154262 |  5094 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   154262 |  5095 | `	pFrameLocal = pVm->pFrame;` |
|   154262 |  5096 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5097 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  5098 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5099 | `	}` |
|   154262 |  5100 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   154202 |  5101 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5102 | `		ph7_hashmap_node *pNode;` |
|        - |  5103 | `		/* Extract the current node value */` |
|   154202 |  5104 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   154202 |  5105 | `		if( pNode == 0 ){` |
|        - |  5106 | `			/* No more entry to process */` |
|     9594 |  5107 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9594 |  5108 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5109 | `				/* Break the reference with the last element */` |
|        5 |  5110 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5111 | `			}` |
|        - |  5112 | `			/* Automatically reset the loop cursor */` |
|     9594 |  5113 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5114 | `			/* Cleanup the mess left behind */` |
|     9594 |  5115 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9594 |  5116 | `			SySetPop(&pInfo->aStep);` |
|     9594 |  5117 | `			PH7_HashmapUnref(pMap);` |
|     4798 |  5118 | `		}else{` |
|   144610 |  5119 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5120 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5121 | `				if( pKey ){` |
|      416 |  5122 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5123 | `				}` |
|      207 |  5124 | `			}` |
|   144610 |  5125 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5126 | `				SyHashEntry *pEntry;` |
|        - |  5127 | `				/* Pass by reference */` |
|       18 |  5128 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       18 |  5129 | `				if( pEntry ){` |
|       16 |  5130 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        9 |  5131 | `				}else{` |
|        4 |  5132 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5133 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5134 | `				}` |
|       10 |  5135 | `			}else{` |
|        - |  5136 | `				/* Make a copy of the entry value */` |
|   144594 |  5137 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   144594 |  5138 | `				if( pValue ){` |
|   144594 |  5139 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    72296 |  5140 | `				}` |
|        - |  5141 | `			}` |
|        2 |  5142 | `		}` |
|    77162 |  5143 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5144 | `		/* Iterator-based iteration.` |
|        - |  5145 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5146 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5147 | `		 */` |
|       37 |  5148 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5149 | `		ph7_class_method *pMethod;` |
|        - |  5150 | `		ph7_value sResult;` |
|       37 |  5151 | `		int isValid = 0;` |
|        - |  5152 | `		/* Call next() to advance — but skip on the first iteration */` |
|       37 |  5153 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        9 |  5154 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        5 |  5155 | `		}else{` |
|       29 |  5156 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       29 |  5157 | `			if( pMethod ){` |
|       29 |  5158 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       14 |  5159 | `			}` |
|        - |  5160 | `		}` |
|        - |  5161 | `		/* Call valid() */` |
|       37 |  5162 | `		PH7_MemObjInit(pVm,&sResult);` |
|       37 |  5163 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       37 |  5164 | `		if( pMethod ){` |
|       37 |  5165 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       37 |  5166 | `			PH7_MemObjToBool(&sResult);` |
|       37 |  5167 | `			isValid = (sResult.x.iVal != 0);` |
|       18 |  5168 | `		}` |
|       37 |  5169 | `		PH7_MemObjRelease(&sResult);` |
|       37 |  5170 | `		if( !isValid ){` |
|        - |  5171 | `			/* Iterator exhausted */` |
|        7 |  5172 | `			pc = pInstr->iP2 - 1;` |
|        - |  5173 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|        7 |  5174 | `			if( pStep->pOwner ){` |
|        3 |  5175 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5176 | `			}` |
|        7 |  5177 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        7 |  5178 | `			SySetPop(&pInfo->aStep);` |
|        7 |  5179 | `			PH7_ClassInstanceUnref(pThis);` |
|        4 |  5180 | `		}else{` |
|        - |  5181 | `			/* Call current() to get value */` |
|       31 |  5182 | `			PH7_MemObjInit(pVm,&sResult);` |
|       31 |  5183 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       31 |  5184 | `			if( pMethod ){` |
|       31 |  5185 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       15 |  5186 | `			}` |
|       31 |  5187 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       31 |  5188 | `			if( pValue ){` |
|       31 |  5189 | `				PH7_MemObjStore(&sResult,pValue);` |
|       15 |  5190 | `			}` |
|       31 |  5191 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5192 | `			/* Call key() if needed */` |
|       31 |  5193 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5194 | `				ph7_value sKey;` |
|       23 |  5195 | `				PH7_MemObjInit(pVm,&sKey);` |
|       23 |  5196 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       23 |  5197 | `				if( pMethod ){` |
|       23 |  5198 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       11 |  5199 | `				}` |
|       23 |  5200 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       23 |  5201 | `				if( pValue ){` |
|       23 |  5202 | `					PH7_MemObjStore(&sKey,pValue);` |
|       11 |  5203 | `				}` |
|       23 |  5204 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  5205 | `			}` |
|        - |  5206 | `		}` |
|       19 |  5207 | `	}else{` |
|       25 |  5208 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5209 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5210 | `		SyHashEntry *pEntry;` |
|        - |  5211 | `		/* Point to the next attribute */` |
|       29 |  5212 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5213 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5214 | `			/* Check access permission */` |
|       31 |  5215 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5216 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5217 | `					break; /* Access is granted */` |
|        - |  5218 | `			}` |
|        1 |  5219 | `		}` |
|       25 |  5220 | `		if( pEntry == 0 ){` |
|        - |  5221 | `			/* Clean up the mess left behind */` |
|        9 |  5222 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5223 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5224 | `				/* Break the reference with the last element */` |
|        3 |  5225 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5226 | `			}` |
|        9 |  5227 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5228 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5229 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5230 | `		}else{` |
|       17 |  5231 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5232 | `			ph7_value *pAttrValue;` |
|       17 |  5233 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5234 | `				/* Fill with the current attribute name */` |
|       17 |  5235 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5236 | `				if( pKey ){` |
|       17 |  5237 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5238 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5239 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5240 | `				}` |
|        8 |  5241 | `			}` |
|        - |  5242 | `			/* Extract attribute value */` |
|       17 |  5243 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5244 | `			if( pAttrValue ){` |
|       17 |  5245 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5246 | `					/* Pass by reference */` |
|        3 |  5247 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5248 | `					if( pEntry ){` |
|        3 |  5249 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5250 | `					}else{` |
|      ! 0 |  5251 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5252 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5253 | `					}` |
|        2 |  5254 | `				}else{` |
|        - |  5255 | `					/* Make a copy of the attribute value */` |
|       15 |  5256 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5257 | `					if( pValue ){` |
|       15 |  5258 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5259 | `					}` |
|        - |  5260 | `				}` |
|        8 |  5261 | `			}` |
|        - |  5262 | `		}` |
|        - |  5263 | `	}` |
|   154262 |  5264 | `	break;` |
|        - |  5265 | `						  }` |
|        - |  5266 | `/*` |
|        - |  5267 | ` * OP_MEMBER P1 P2` |
|        - |  5268 | ` * Load class attribute/method on the stack.` |
|        - |  5269 | ` */` |
|     2076 |  5270 | `case PH7_OP_MEMBER: {` |
|        - |  5271 | `	ph7_class_instance *pThis;` |
|        - |  5272 | `	ph7_value *pNos;` |
|        - |  5273 | `	SyString sName;` |
|     4154 |  5274 | `	if( !pInstr->iP1 ){` |
|     4056 |  5275 | `		pNos = &pTos[-1];` |
|        - |  5276 | `#ifdef UNTRUST` |
|        - |  5277 | `		if( pNos < pStack ){` |
|        - |  5278 | `			goto Abort;` |
|        - |  5279 | `		}` |
|        - |  5280 | `#endif` |
|     4056 |  5281 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5282 | `			ph7_class *pClass;` |
|        - |  5283 | `			/* Class already instantiated */` |
|     4056 |  5284 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5285 | `			/* Point to the instantiated class */` |
|     4056 |  5286 | `			pClass = pThis->pClass;` |
|        - |  5287 | `			/* Extract attribute name first */` |
|     4056 |  5288 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4056 |  5289 | `			if( pInstr->iP2 ){` |
|        - |  5290 | `				/* Method call */` |
|      278 |  5291 | `				ph7_class_method *pMeth = 0;` |
|      278 |  5292 | `				if( sName.nByte > 0 ){` |
|        - |  5293 | `					/* Extract the target method */` |
|      278 |  5294 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      138 |  5295 | `				}` |
|      278 |  5296 | `				if( pMeth == 0 ){` |
|      ! 0 |  5297 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5298 | `						&pClass->sName,&sName` |
|        - |  5299 | `						);` |
|        - |  5300 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5301 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5302 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5303 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5304 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5305 | `				}else{` |
|        - |  5306 | `					/* Push method name on the stack */` |
|      278 |  5307 | `					PH7_MemObjRelease(pTos);` |
|      278 |  5308 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      278 |  5309 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5310 | `				}` |
|      278 |  5311 | `				pTos->nIdx = SXU32_HIGH;` |
|      140 |  5312 | `			}else{` |
|        - |  5313 | `				/* Attribute access */` |
|     3780 |  5314 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5315 | `				SyHashEntry *pEntry;` |
|        - |  5316 | `				/* Extract the target attribute */` |
|     3780 |  5317 | `				if( sName.nByte > 0 ){` |
|     3780 |  5318 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3780 |  5319 | `					if( pEntry ){` |
|        - |  5320 | `						/* Point to the attribute value */` |
|     3778 |  5321 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1888 |  5322 | `					}` |
|     1889 |  5323 | `				}` |
|     3780 |  5324 | `				if( pObjAttr == 0 ){` |
|        - |  5325 | `					/* No such attribute,load null */` |
|        4 |  5326 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5327 | `						&pClass->sName,&sName);` |
|        - |  5328 | `					/* Call the __get magic method if available */` |
|        3 |  5329 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5330 | `				}` |
|     3780 |  5331 | `				VmPopOperand(&pTos,1);` |
|        - |  5332 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5333 | `				 * This is due to the following case:` |
|        - |  5334 | `				 *     (new TestClass())->foo;` |
|        - |  5335 | `				 */` |
|     3780 |  5336 | `				pThis->iRef++;` |
|     3780 |  5337 | `				PH7_MemObjRelease(pTos);` |
|     3780 |  5338 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3780 |  5339 | `				if( pObjAttr ){` |
|     3778 |  5340 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5341 | `					/* Check attribute access */` |
|     3778 |  5342 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5343 | `						/* Load attribute */` |
|     3778 |  5344 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3778 |  5345 | `						if( pValue ){` |
|     3778 |  5346 | `							if( pThis->iRef < 2 ){` |
|        - |  5347 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5348 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5349 | `								 */` |
|        3 |  5350 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5351 | `							}else{` |
|        - |  5352 | `								/* Simple load */` |
|     3776 |  5353 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5354 | `							}` |
|     3778 |  5355 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3776 |  5356 | `								if( pThis->iRef > 1 ){` |
|        - |  5357 | `									/* Load attribute index */` |
|     3774 |  5358 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1886 |  5359 | `								}` |
|     1887 |  5360 | `							}` |
|     1888 |  5361 | `						}` |
|     1888 |  5362 | `					}` |
|     1888 |  5363 | `				}` |
|        - |  5364 | `				/* Safely unreference the object */` |
|     3780 |  5365 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5366 | `			}` |
|     2029 |  5367 | `		}else{` |
|      ! 0 |  5368 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5369 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5370 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5371 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5372 | `		}` |
|     2029 |  5373 | `	}else{` |
|        - |  5374 | `		/* Static member access using class name */` |
|      100 |  5375 | `		pNos = pTos;` |
|      100 |  5376 | `		pThis = 0;` |
|      100 |  5377 | `		if( !pInstr->p3 ){` |
|       88 |  5378 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       88 |  5379 | `			pNos--;` |
|        - |  5380 | `#ifdef UNTRUST` |
|        - |  5381 | `			if( pNos < pStack ){` |
|        - |  5382 | `				goto Abort;` |
|        - |  5383 | `			}` |
|        - |  5384 | `#endif` |
|       45 |  5385 | `		}else{` |
|        - |  5386 | `			/* Attribute name already computed */` |
|       14 |  5387 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5388 | `		}` |
|      100 |  5389 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      100 |  5390 | `			ph7_class *pClass = 0;` |
|      100 |  5391 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5392 | `				/* Class already instantiated */` |
|      ! 0 |  5393 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5394 | `				pClass = pThis->pClass;` |
|      ! 0 |  5395 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5396 | `			}else{` |
|        - |  5397 | `				/* Try to extract the target class */` |
|      100 |  5398 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      100 |  5399 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      100 |  5400 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5401 | `					/* Handle self/static/parent keywords */` |
|      100 |  5402 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5403 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5404 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5405 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5406 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5407 | `						}` |
|       86 |  5408 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5409 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5410 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5411 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5412 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5413 | `							pClass = pSelf->pBase;` |
|        6 |  5414 | `						}` |
|        8 |  5415 | `					}else{` |
|       46 |  5416 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5417 | `					}` |
|       49 |  5418 | `				}` |
|        - |  5419 | `			}` |
|      100 |  5420 | `			if( pClass == 0 ){` |
|        - |  5421 | `				/* Undefined class */` |
|      ! 0 |  5422 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5423 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5424 | `					);` |
|      ! 0 |  5425 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5426 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5427 | `				}` |
|      ! 0 |  5428 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5429 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5430 | `			}else{` |
|      100 |  5431 | `				if( pInstr->iP2 ){` |
|        - |  5432 | `					/* Method call */` |
|       30 |  5433 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5434 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5435 | `						/* Extract the target method */` |
|       30 |  5436 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5437 | `					}` |
|       30 |  5438 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5439 | `						if( pMeth ){` |
|      ! 0 |  5440 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5441 | `								&pClass->sName,&sName` |
|        - |  5442 | `								);` |
|      ! 0 |  5443 | `						}else{` |
|      ! 0 |  5444 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5445 | `								&pClass->sName,&sName` |
|        - |  5446 | `								);` |
|        - |  5447 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5448 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5449 | `						}` |
|        - |  5450 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5451 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5452 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5453 | `						}` |
|      ! 0 |  5454 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5455 | `					}else{` |
|        - |  5456 | `						/* Push method name on the stack */` |
|       30 |  5457 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5458 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5459 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5460 | `					}` |
|       30 |  5461 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5462 | `				}else{` |
|        - |  5463 | `					/* Attribute access */` |
|       72 |  5464 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5465 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5466 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5467 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5468 | `						/* ::class returns the fully qualified class name */` |
|        - |  5469 | `						/* Pop the attribute name from the stack */` |
|       54 |  5470 | `						if( !pInstr->p3 ){` |
|       54 |  5471 | `							VmPopOperand(&pTos,1);` |
|       26 |  5472 | `						}` |
|       54 |  5473 | `						PH7_MemObjRelease(pTos);` |
|        - |  5474 | `						/* Load the class name */` |
|       54 |  5475 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5476 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5477 | `					}else{` |
|        - |  5478 | `						/* Extract the target attribute */` |
|       20 |  5479 | `						if( sName.nByte > 0 ){` |
|       20 |  5480 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5481 | `						}` |
|       20 |  5482 | `						if( pAttr == 0 ){` |
|        - |  5483 | `							/* No such attribute,load null */` |
|      ! 0 |  5484 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5485 | `								&pClass->sName,&sName);` |
|        - |  5486 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5487 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5488 | `						}` |
|        - |  5489 | `						/* Pop the attribute name from the stack */` |
|       20 |  5490 | `						if( !pInstr->p3 ){` |
|        7 |  5491 | `							VmPopOperand(&pTos,1);` |
|        3 |  5492 | `						}` |
|       20 |  5493 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5494 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5495 | `						if( pAttr ){` |
|       20 |  5496 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5497 | `								/* Access to a non static attribute */` |
|      ! 0 |  5498 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5499 | `									&pClass->sName,&pAttr->sName` |
|        - |  5500 | `									);` |
|      ! 0 |  5501 | `							}else{` |
|        - |  5502 | `								ph7_value *pValue;` |
|        - |  5503 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5504 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5505 | `									/* Load the desired attribute */` |
|       20 |  5506 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5507 | `									if( pValue ){` |
|       20 |  5508 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5509 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5510 | `											/* Load index number */` |
|       14 |  5511 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5512 | `										}` |
|        9 |  5513 | `									}` |
|        9 |  5514 | `								}` |
|        - |  5515 | `							}` |
|        9 |  5516 | `						}` |
|        - |  5517 | `					}` |
|        - |  5518 | `				}` |
|      100 |  5519 | `				if( pThis ){` |
|        - |  5520 | `					/* Safely unreference the object */` |
|      ! 0 |  5521 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5522 | `				}` |
|        - |  5523 | `			}` |
|       51 |  5524 | `		}else{` |
|        - |  5525 | `			/* Pop operands */` |
|      ! 0 |  5526 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5527 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5528 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5529 | `			}` |
|      ! 0 |  5530 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5531 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5532 | `		}` |
|        - |  5533 | `	}` |
|     4154 |  5534 | `	break;` |
|        - |  5535 | `					}` |
|        - |  5536 | `/*` |
|        - |  5537 | ` * OP_NEW P1 * * *` |
|        - |  5538 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5539 | ` */` |
|      304 |  5540 | `case PH7_OP_NEW: {` |
|      610 |  5541 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      610 |  5542 | `	ph7_class *pClass = 0;` |
|        - |  5543 | `	ph7_class_instance *pNew;` |
|      610 |  5544 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5545 | `		/* Try to extract the desired class */` |
|      914 |  5546 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      608 |  5547 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      304 |  5548 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5549 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5550 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5551 | `	}` |
|      610 |  5552 | `	if( pClass == 0 ){` |
|        - |  5553 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5554 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5555 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5556 | `			);` |
|        - |  5557 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5558 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5559 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5560 | `			/* Pop given arguments */` |
|      ! 0 |  5561 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5562 | `		}` |
|      ! 0 |  5563 | `		goto Abort;` |
|      ! 0 |  5564 | `	}else{` |
|        - |  5565 | `		ph7_class_method *pCons;` |
|        - |  5566 | `		/* Create a new class instance */` |
|      610 |  5567 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      610 |  5568 | `		if( pNew == 0 ){` |
|      ! 0 |  5569 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5570 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5571 | `				&pClass->sName` |
|        - |  5572 | `			);` |
|      ! 0 |  5573 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5574 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5575 | `				/* Pop given arguments */` |
|      ! 0 |  5576 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5577 | `			}` |
|      ! 0 |  5578 | `			break;` |
|        - |  5579 | `		}` |
|        - |  5580 | `		/* Check if a constructor is available */` |
|      610 |  5581 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      610 |  5582 | `		if( pCons == 0 ){` |
|      526 |  5583 | `			SyString *pName = &pClass->sName;` |
|        - |  5584 | `			/* Check for a constructor with the same base class name */` |
|      526 |  5585 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      262 |  5586 | `		}` |
|      610 |  5587 | `		if( pCons ){` |
|        - |  5588 | `			/* Call the class constructor */` |
|       86 |  5589 | `			SySetReset(&aArg);` |
|      160 |  5590 | `			while( pArg < pTos ){` |
|       76 |  5591 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       76 |  5592 | `				pArg++;` |
|        2 |  5593 | `			}` |
|       86 |  5594 | `			if( pVm->bErrReport ){` |
|        - |  5595 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5596 | `				sxu32 n;` |
|       43 |  5597 | `				n = SySetUsed(&aArg);` |
|        - |  5598 | `				/* Emit a notice for missing arguments */` |
|       95 |  5599 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       53 |  5600 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       53 |  5601 | `					if( pFuncArg ){` |
|       53 |  5602 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5603 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5604 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5605 | `						}` |
|       26 |  5606 | `					}` |
|       53 |  5607 | `					n++;` |
|        1 |  5608 | `				}` |
|       21 |  5609 | `			}` |
|       86 |  5610 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5611 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       86 |  5612 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5613 | `				pNew->iRef = 1;` |
|      ! 0 |  5614 | `			}` |
|       42 |  5615 | `		}` |
|      610 |  5616 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5617 | `			/* Pop given arguments */` |
|       68 |  5618 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       33 |  5619 | `		}` |
|      610 |  5620 | `		PH7_MemObjRelease(pTos);` |
|      610 |  5621 | `		pTos->x.pOther = pNew;` |
|      610 |  5622 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5623 | `	}` |
|      610 |  5624 | `	break;` |
|        - |  5625 | `				 }` |
|        - |  5626 | `/*` |
|        - |  5627 | ` * OP_CLONE * * *` |
|        - |  5628 | ` * Perfome a clone operation.` |
|        - |  5629 | ` */` |
|       23 |  5630 | `case PH7_OP_CLONE: {` |
|        - |  5631 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5632 | `#ifdef UNTRUST` |
|        - |  5633 | `	if( pTos < pStack ){` |
|        - |  5634 | `		goto Abort;` |
|        - |  5635 | `	}` |
|        - |  5636 | `#endif` |
|        - |  5637 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5638 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5639 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5640 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5641 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5642 | `		break;` |
|        - |  5643 | `	}` |
|        - |  5644 | `	/* Point to the source */` |
|       44 |  5645 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5646 | `	/* Perform the clone operation */` |
|       44 |  5647 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5648 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5649 | `	if( pClone == 0 ){` |
|      ! 0 |  5650 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5651 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5652 | `	}else{` |
|        - |  5653 | `		/* Load the cloned object */` |
|       44 |  5654 | `		pTos->x.pOther = pClone;` |
|       44 |  5655 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5656 | `	}` |
|       44 |  5657 | `	break;` |
|        - |  5658 | `				   }` |
|        - |  5659 | `/*` |
|        - |  5660 | ` * OP_SWITCH * * P3` |
|        - |  5661 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5662 | ` */` |
|       18 |  5663 | `case PH7_OP_SWITCH: {` |
|       38 |  5664 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5665 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5666 | `	ph7_value sValue,sCaseValue;` |
|        - |  5667 | `	sxu32 n,nEntry;` |
|        - |  5668 | `#ifdef UNTRUST` |
|        - |  5669 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5670 | `		goto Abort;` |
|        - |  5671 | `	}` |
|        - |  5672 | `#endif` |
|        - |  5673 | `	/* Point to the case table  */` |
|       38 |  5674 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5675 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5676 | `	/* Select the appropriate case block to execute */` |
|       38 |  5677 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5678 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5679 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5680 | `		pCase = &aCase[n];` |
|       92 |  5681 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5682 | `		/* Execute the case expression first */` |
|       92 |  5683 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5684 | `		/* Compare the two expression */` |
|       92 |  5685 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5686 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5687 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5688 | `		if( rc == 0 ){` |
|        - |  5689 | `			/* Value match,jump to this block */` |
|       38 |  5690 | `			pc = pCase->nStart - 1;` |
|       38 |  5691 | `			break;` |
|        - |  5692 | `		}` |
|       29 |  5693 | `	}` |
|       38 |  5694 | `	VmPopOperand(&pTos,1);` |
|       38 |  5695 | `	if( n >= nEntry ){` |
|        - |  5696 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5697 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5698 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5699 | `		}else{` |
|        - |  5700 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5701 | `			pc = pSwitch->nOut - 1;` |
|        - |  5702 | `		}` |
|      ! 0 |  5703 | `	}` |
|       38 |  5704 | `	break;` |
|        - |  5705 | `					}` |
|        - |  5706 | `/*` |
|        - |  5707 | ` * OP_CALL P1 * *` |
|        - |  5708 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5709 | ` *  function on the stack.` |
|        - |  5710 | ` */` |
|   280665 |  5711 | `case PH7_OP_CALL: {` |
|   561376 |  5712 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5713 | `	SyHashEntry *pEntry;` |
|        - |  5714 | `	SyString sName;` |
|        - |  5715 | `	/* Extract function name */` |
|   561376 |  5716 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5717 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5718 | `			ph7_value sResult;` |
|      ! 0 |  5719 | `			SySetReset(&aArg);` |
|      ! 0 |  5720 | `			while( pArg < pTos ){` |
|      ! 0 |  5721 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5722 | `				pArg++;` |
|      ! 0 |  5723 | `			}` |
|      ! 0 |  5724 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5725 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5726 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5727 | `			SySetReset(&aArg);` |
|        - |  5728 | `			/* Pop given arguments */` |
|      ! 0 |  5729 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5730 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5731 | `			}` |
|        - |  5732 | `			/* Copy result */` |
|      ! 0 |  5733 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5734 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5735 | `		}else{` |
|        3 |  5736 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5737 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5738 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5739 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5740 | `			}else{` |
|        - |  5741 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5742 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5743 | `			}` |
|        - |  5744 | `			/* Pop given arguments */` |
|        3 |  5745 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5746 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5747 | `			}` |
|        - |  5748 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5749 | `			PH7_MemObjRelease(pTos);` |
|        - |  5750 | `		}` |
|   280432 |  5751 | `		break;` |
|        - |  5752 | `	}` |
|   561374 |  5753 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5754 | `	/* Check for a compiled function first.` |
|        - |  5755 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5756 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   561374 |  5757 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5758 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5759 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5760 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5761 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5762 | `	 * function calls inside namespaces. */` |
|   561374 |  5763 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5764 | `		const char *zFunc;` |
|        - |  5765 | `		const char *zEnd;` |
|        - |  5766 | `		const char *z;` |
|        - |  5767 | `		SyString sGlobal;` |
|       15 |  5768 | `		zFunc = sName.zString;` |
|       15 |  5769 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5770 | `		z = zEnd;` |
|        - |  5771 | `		/* Find last namespace separator */` |
|      133 |  5772 | `		while( z > zFunc ){` |
|      133 |  5773 | `			if( z[-1] == '\\' ){` |
|       15 |  5774 | `				break;` |
|        - |  5775 | `			}` |
|      119 |  5776 | `			z--;` |
|        1 |  5777 | `		}` |
|       15 |  5778 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5779 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5780 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5781 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5782 | `		}` |
|        7 |  5783 | `	}` |
|   561374 |  5784 | `	if( pEntry ){` |
|        - |  5785 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5786 | `		ph7_class_instance *pThis;` |
|        - |  5787 | `		ph7_value *pFrameStack;` |
|        - |  5788 | `		ph7_vm_func *pVmFunc;` |
|        - |  5789 | `		ph7_class *pSelf;` |
|        - |  5790 | `		VmFrame *pFrame;` |
|        - |  5791 | `		ph7_value *pObj;` |
|        - |  5792 | `		VmSlot sArg;` |
|        - |  5793 | `		sxu32 n;` |
|        - |  5794 | `		/* initialize fields */` |
|    12476 |  5795 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12476 |  5796 | `		pThis = 0;` |
|    12476 |  5797 | `		pSelf = 0;` |
|    12476 |  5798 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5799 | `			ph7_class_method *pMeth;` |
|        - |  5800 | `			/* Class method call */` |
|     1604 |  5801 | `			ph7_value *pTarget = &pTos[-1];` |
|     1604 |  5802 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5803 | `				/* Extract the 'this' pointer */` |
|     1604 |  5804 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5805 | `					/* Instance already loaded */` |
|     1570 |  5806 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1570 |  5807 | `					pThis->iRef++;` |
|     1570 |  5808 | `					pSelf = pThis->pClass;` |
|      784 |  5809 | `				}` |
|     1604 |  5810 | `				if( pSelf == 0 ){` |
|       36 |  5811 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5812 | `						/* "Late Static Binding" class name */` |
|       44 |  5813 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5814 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5815 | `					}` |
|       36 |  5816 | `					if( pSelf == 0 ){` |
|       13 |  5817 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5818 | `					}` |
|       17 |  5819 | `				}` |
|     1604 |  5820 | `				if( pThis == 0  ){` |
|       36 |  5821 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5822 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5823 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5824 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5825 | `					}` |
|       36 |  5826 | `					if( pFrameLocal->pParent ){` |
|        - |  5827 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5828 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5829 | `						if( pThis ){` |
|       13 |  5830 | `							pThis->iRef++;` |
|        6 |  5831 | `						}` |
|        9 |  5832 | `					}` |
|       17 |  5833 | `				}` |
|     1604 |  5834 | `				VmPopOperand(&pTos,1);` |
|     1604 |  5835 | `				PH7_MemObjRelease(pTos);` |
|        - |  5836 | `				/* Synchronize pointers */` |
|     1604 |  5837 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5838 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5839 | `				 * user have already computed the random generated unique class method name` |
|        - |  5840 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5841 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5842 | `				 */` |
|     1604 |  5843 | `				while( pArg < pStack ){` |
|      ! 0 |  5844 | `					pArg++;` |
|      ! 0 |  5845 | `				}` |
|     1604 |  5846 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5847 | `					/* Check if the call is allowed */` |
|     1604 |  5848 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1604 |  5849 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5850 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5851 | `							/* Pop given arguments */` |
|      ! 0 |  5852 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5853 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5854 | `							}` |
|        - |  5855 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5856 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5857 | `							break;` |
|        - |  5858 | `						}` |
|        3 |  5859 | `					}` |
|      801 |  5860 | `				}` |
|      801 |  5861 | `			}` |
|      801 |  5862 | `		}` |
|        - |  5863 | `		/* Check The recursion limit */` |
|    12476 |  5864 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5865 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5866 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5867 | `				&pVmFunc->sName);` |
|        - |  5868 | `			/* Pop given arguments */` |
|        3 |  5869 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5870 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5871 | `			}` |
|        - |  5872 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5873 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5874 | `			break;` |
|        - |  5875 | `		}` |
|    12474 |  5876 | `		if( pVmFunc->pNextName ){` |
|        - |  5877 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5878 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5879 | `		}` |
|        - |  5880 | `		/* Extract the formal argument set */` |
|    12474 |  5881 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5882 | `		/* Create a new VM frame  */` |
|    12474 |  5883 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12474 |  5884 | `		if( rc != SXRET_OK ){` |
|        - |  5885 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5886 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5887 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5888 | `				&pVmFunc->sName);` |
|        - |  5889 | `			/* Pop given arguments */` |
|      ! 0 |  5890 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5891 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5892 | `			}` |
|        - |  5893 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5894 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5895 | `			break;` |
|        - |  5896 | `		}` |
|    12474 |  5897 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5898 | `			/* Install the '$this' variable */` |
|        - |  5899 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1580 |  5900 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1580 |  5901 | `			if( pObj ){` |
|        - |  5902 | `				/* Reflect the change */` |
|     1580 |  5903 | `				pObj->x.pOther = pThis;` |
|     1580 |  5904 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      789 |  5905 | `			}` |
|      789 |  5906 | `		}` |
|    12474 |  5907 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5908 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5909 | `			/* Install static variables */` |
|      ! 0 |  5910 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5911 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5912 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5913 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5914 | `					/* Initialize the static variables */` |
|      ! 0 |  5915 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5916 | `					if( pObj ){` |
|        - |  5917 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5918 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5919 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5920 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5921 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5922 | `						}` |
|      ! 0 |  5923 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5924 | `					}else{` |
|      ! 0 |  5925 | `						continue;` |
|        - |  5926 | `					}` |
|      ! 0 |  5927 | `				}` |
|        - |  5928 | `				/* Install in the current frame */` |
|      ! 0 |  5929 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5930 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5931 | `			}` |
|      ! 0 |  5932 | `		}` |
|        - |  5933 | `		/* Push arguments in the local frame */` |
|    12474 |  5934 | `		n = 0;` |
|    34414 |  5935 | `		while( pArg < pTos ){` |
|    21942 |  5936 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21792 |  5937 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5938 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5939 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5940 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5941 | `						goto Abort;` |
|        - |  5942 | `					}` |
|      ! 0 |  5943 | `				}` |
|        - |  5944 | `				/* Make sure the given arguments are of the correct type */` |
|    21792 |  5945 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5946 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5947 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5948 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5949 | `						ph7_class *pClass;` |
|        - |  5950 | `						/* Try to extract the desired class */` |
|      ! 0 |  5951 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5952 | `						if( pClass ){` |
|      ! 0 |  5953 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5954 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5955 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5956 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5957 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5958 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5959 | `								}` |
|      ! 0 |  5960 | `							}else{` |
|        - |  5961 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5962 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5963 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5964 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5965 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5966 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5967 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5968 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5969 | `								}` |
|        - |  5970 | `							}` |
|      ! 0 |  5971 | `						}` |
|     1088 |  5972 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5973 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5974 | `						/* Cast to the desired type */` |
|      ! 0 |  5975 | `						xCast(pArg);` |
|      ! 0 |  5976 | `					}` |
|      543 |  5977 | `				}` |
|    21792 |  5978 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5979 | `					/* Pass by reference */` |
|       48 |  5980 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5981 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5982 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5983 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5984 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5985 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5986 | `						}` |
|        - |  5987 | `						/* Switch to pass by value */` |
|      ! 0 |  5988 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5989 | `					}else{` |
|        - |  5990 | `						SyHashEntry *pRefEntry;` |
|        - |  5991 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5992 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5993 | `						if( pRefEntry == 0 ){` |
|       71 |  5994 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5995 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5996 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5997 | `							sArg.pUserData = 0;` |
|       48 |  5998 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5999 | `						}` |
|       48 |  6000 | `						pObj = 0;` |
|        - |  6001 | `					}` |
|       25 |  6002 | `				}else{` |
|        - |  6003 | `					/* Pass by value,make a copy of the given argument */` |
|    21746 |  6004 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6005 | `				}` |
|    10897 |  6006 | `			}else{` |
|        - |  6007 | `				char zName[32];` |
|        - |  6008 | `				SyString sArgName;` |
|        - |  6009 | `				/* Set a dummy name */` |
|      152 |  6010 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  6011 | `				sArgName.zString = zName;` |
|        - |  6012 | `				/* Annonymous argument */` |
|      152 |  6013 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6014 | `			}` |
|    21942 |  6015 | `			if( pObj ){` |
|    21896 |  6016 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6017 | `				/* Insert argument index  */` |
|    21896 |  6018 | `				sArg.nIdx = pObj->nIdx;` |
|    21896 |  6019 | `				sArg.pUserData = 0;` |
|    21896 |  6020 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10947 |  6021 | `			}` |
|    21942 |  6022 | `			PH7_MemObjRelease(pArg);` |
|    21942 |  6023 | `			pArg++;` |
|    21942 |  6024 | `			++n;` |
|        2 |  6025 | `		}` |
|        - |  6026 | `		/* Set up closure environment */` |
|    12474 |  6027 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6028 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6029 | `			ph7_value *pValue;` |
|        - |  6030 | `			sxu32 iEnv;` |
|        9 |  6031 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  6032 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  6033 | `				pEnv = &aEnv[iEnv];` |
|       17 |  6034 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6035 | `					/* Do not install null value */` |
|        9 |  6036 | `					continue;` |
|        - |  6037 | `				}` |
|        9 |  6038 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  6039 | `				if( pValue == 0 ){` |
|      ! 0 |  6040 | `					continue;` |
|        - |  6041 | `				}` |
|        - |  6042 | `				/* Invalidate any prior representation */` |
|        9 |  6043 | `				PH7_MemObjRelease(pValue);` |
|        - |  6044 | `				/* Duplicate bound variable value */` |
|        9 |  6045 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  6046 | `			}` |
|        4 |  6047 | `		}` |
|        - |  6048 | `		/* Process default values */` |
|    14338 |  6049 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1866 |  6050 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1860 |  6051 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1860 |  6052 | `				if( pObj ){` |
|        - |  6053 | `					/* Evaluate the default value and extract it's result */` |
|     1860 |  6054 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1860 |  6055 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6056 | `						goto Abort;` |
|        - |  6057 | `					}` |
|        - |  6058 | `					/* Insert argument index */` |
|     1860 |  6059 | `					sArg.nIdx = pObj->nIdx;` |
|     1860 |  6060 | `					sArg.pUserData = 0;` |
|     1860 |  6061 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6062 | `					/* Make sure the default argument is of the correct type */` |
|     1860 |  6063 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6064 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6065 | `						/* Cast to the desired type */` |
|      ! 0 |  6066 | `						xCast(pObj);` |
|      ! 0 |  6067 | `					}` |
|      929 |  6068 | `				}` |
|      929 |  6069 | `			}` |
|     1866 |  6070 | `			++n;` |
|        2 |  6071 | `		}` |
|        - |  6072 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6073 | `		 * does not return anything.` |
|        - |  6074 | `		 */` |
|    12474 |  6075 | `		PH7_MemObjRelease(pTos);` |
|    12474 |  6076 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6077 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12474 |  6078 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12474 |  6079 | `		if( pFrameStack == 0 ){` |
|        - |  6080 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6081 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6082 | `				&pVmFunc->sName);` |
|      ! 0 |  6083 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6084 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6085 | `			}` |
|      ! 0 |  6086 | `			break;` |
|        - |  6087 | `		}` |
|    12474 |  6088 | `		if( pSelf ){` |
|        - |  6089 | `			/* Push class name */` |
|     1602 |  6090 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      800 |  6091 | `		}` |
|        - |  6092 | `		/* Increment nesting level */` |
|    12474 |  6093 | `		pVm->nRecursionDepth++;` |
|        - |  6094 | `		/* Execute function body */` |
|    12474 |  6095 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  6096 | `		/* Decrement nesting level */` |
|    12474 |  6097 | `		pVm->nRecursionDepth--;` |
|    12474 |  6098 | `		if( pSelf ){` |
|        - |  6099 | `			/* Pop class name */` |
|     1602 |  6100 | `			(void)SySetPop(&pVm->aSelf);` |
|      800 |  6101 | `		}` |
|        - |  6102 | `		/* Cleanup the mess left behind */` |
|    12474 |  6103 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6104 | `			/* Return by reference,reflect that */` |
|        9 |  6105 | `			if( n != SXU32_HIGH ){` |
|        9 |  6106 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6107 | `				sxu32 i;` |
|        - |  6108 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6109 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6110 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6111 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6112 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6113 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6114 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6115 | `								&pVmFunc->sName);` |
|      ! 0 |  6116 | `						}` |
|      ! 0 |  6117 | `						n = SXU32_HIGH;` |
|      ! 0 |  6118 | `						break;` |
|        - |  6119 | `					}` |
|        3 |  6120 | `				}` |
|        5 |  6121 | `			}else{` |
|      ! 0 |  6122 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6123 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6124 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6125 | `						&pVmFunc->sName);` |
|      ! 0 |  6126 | `				}` |
|        - |  6127 | `			}` |
|        9 |  6128 | `			pTos->nIdx = n;` |
|        4 |  6129 | `		}` |
|        - |  6130 | `		/* Cleanup the mess left behind */` |
|    12474 |  6131 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6132 | `			/* An exception was throw in this frame */` |
|        7 |  6133 | `			pFrame = pFrame->pParent;` |
|        7 |  6134 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6135 | `				/* Pop the resutlt */` |
|        5 |  6136 | `				VmPopOperand(&pTos,1);` |
|        - |  6137 | `				/* Jump to this destination */` |
|        5 |  6138 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  6139 | `				rc = PH7_OK;` |
|        3 |  6140 | `			}else{` |
|        3 |  6141 | `				if( pFrame->pParent ){` |
|        3 |  6142 | `					rc = PH7_EXCEPTION;` |
|        2 |  6143 | `				}else{` |
|        - |  6144 | `					/* Continue normal execution */` |
|      ! 0 |  6145 | `					rc = PH7_OK;` |
|        - |  6146 | `				}` |
|        - |  6147 | `			}` |
|        3 |  6148 | `		}` |
|        - |  6149 | `		/* Free the operand stack */` |
|    12474 |  6150 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6151 | `		/* Leave the frame */` |
|    12474 |  6152 | `		VmLeaveFrame(&(*pVm));` |
|    12474 |  6153 | `		if( rc == PH7_ABORT ){` |
|        - |  6154 | `			/* Abort processing immeditaley */` |
|        7 |  6155 | `			goto Abort;` |
|    12468 |  6156 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6157 | `			goto Exception;` |
|        - |  6158 | `		}` |
|     6234 |  6159 | `	}else{` |
|        - |  6160 | `		ph7_user_func *pFunc;` |
|        - |  6161 | `		ph7_context sCtx;` |
|        - |  6162 | `		ph7_value sRet;` |
|        - |  6163 | `		/* Look for an installed foreign function.` |
|        - |  6164 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6165 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6166 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6167 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6168 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6169 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   548900 |  6170 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   548900 |  6171 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6172 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6173 | `			const char *zShort = sName.zString;` |
|        - |  6174 | `			sxu32 i;` |
|      217 |  6175 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6176 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6177 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6178 | `				}` |
|      102 |  6179 | `			}` |
|       15 |  6180 | `			if( zShort != sName.zString ){` |
|       15 |  6181 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6182 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6183 | `			}` |
|        7 |  6184 | `		}` |
|   548900 |  6185 | `		if( pEntry == 0 ){` |
|        - |  6186 | `			/* Call to undefined function */` |
|        5 |  6187 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6188 | `			/* Pop given arguments */` |
|        5 |  6189 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6190 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6191 | `			}` |
|        - |  6192 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6193 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6194 | `			break;` |
|        - |  6195 | `		}` |
|   548896 |  6196 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6197 | `		/* Start collecting function arguments */` |
|   548896 |  6198 | `		SySetReset(&aArg);` |
|  1475656 |  6199 | `		while( pArg < pTos ){` |
|   926762 |  6200 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   926762 |  6201 | `			pArg++;` |
|        2 |  6202 | `		}` |
|        - |  6203 | `		/* Assume a null return value */` |
|   548896 |  6204 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6205 | `		/* Init the call context */` |
|   548896 |  6206 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6207 | `		/* Call the foreign function */` |
|   548896 |  6208 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6209 | `		/* Release the call context */` |
|   548896 |  6210 | `		VmReleaseCallContext(&sCtx);` |
|   548896 |  6211 | `		if( rc == PH7_ABORT ){` |
|      463 |  6212 | `			goto Abort;` |
|   548434 |  6213 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6214 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  6215 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  6216 | `				pFrm = pFrm->pParent;` |
|        1 |  6217 | `			}` |
|        7 |  6218 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6219 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6220 | `				goto Exception;` |
|        - |  6221 | `			}` |
|        - |  6222 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6223 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6224 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6225 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6226 | `			}` |
|        - |  6227 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6228 | `			VmPopOperand(&pTos,1);` |
|        - |  6229 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6230 | `			pFrm = pVm->pFrame;` |
|        7 |  6231 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6232 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6233 | `			}` |
|        7 |  6234 | `			break;` |
|        - |  6235 | `		}` |
|   548428 |  6236 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6237 | `			/* Pop function name and arguments */` |
|   531132 |  6238 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   265587 |  6239 | `		}` |
|        - |  6240 | `		/* Save foreign function return value */` |
|   548428 |  6241 | `		PH7_MemObjStore(&sRet,pTos);` |
|   548428 |  6242 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6243 | `	}` |
|   560892 |  6244 | `	break;` |
|        - |  6245 | `				  }` |
|        - |  6246 | `/*` |
|        - |  6247 | ` * OP_CONSUME: P1 * *` |
|        - |  6248 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6249 | ` */` |
|    10902 |  6250 | `case PH7_OP_CONSUME: {` |
|    21806 |  6251 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    21806 |  6252 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6253 |  |
|    21806 |  6254 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    21806 |  6255 | `	pCur = pOut;` |
|        - |  6256 | `	/* Start the consume process  */` |
|    43610 |  6257 | `	while( pOut <= pTos ){` |
|        - |  6258 | `		/* Force a string cast */` |
|    21806 |  6259 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6260 | `			PH7_MemObjToString(pOut);` |
|      149 |  6261 | `		}` |
|    21806 |  6262 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6263 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6264 | `			/* Invoke the output consumer callback */` |
|    11978 |  6265 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11978 |  6266 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6267 | `				/* Increment output length */` |
|     5532 |  6268 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2765 |  6269 | `			}` |
|    11978 |  6270 | `			SyBlobRelease(&pOut->sBlob);` |
|    11978 |  6271 | `			if( rc == SXERR_ABORT ){` |
|        - |  6272 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6273 | `				goto Abort;` |
|        - |  6274 | `			}` |
|     5988 |  6275 | `		}` |
|    21806 |  6276 | `		pOut++;` |
|        2 |  6277 | `	}` |
|    21806 |  6278 | `	pTos = &pCur[-1];` |
|    21804 |  6279 | `	break;` |
|        - |  6280 | `					 }` |
|        - |  6281 |  |
|        - |  6282 | `		} /* Switch() */` |
|  9684624 |  6283 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6284 | `	} /* For(;;) */` |
|    15298 |  6285 | `Done:` |
|    30598 |  6286 | `	SySetRelease(&aArg);` |
|    30598 |  6287 | `	return SXRET_OK;` |
|      238 |  6288 | `Abort:` |
|      477 |  6289 | `	SySetRelease(&aArg);` |
|     1661 |  6290 | `	while( pTos >= pStack ){` |
|     1185 |  6291 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6292 | `		pTos--;` |
|        1 |  6293 | `	}` |
|      477 |  6294 | `	return PH7_ABORT;` |
|        1 |  6295 | `Exception:` |
|        3 |  6296 | `	SySetRelease(&aArg);` |
|        5 |  6297 | `	while( pTos >= pStack ){` |
|        3 |  6298 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6299 | `		pTos--;` |
|        1 |  6300 | `	}` |
|        3 |  6301 | `	return PH7_EXCEPTION;` |
|    15539 |  6302 |  |
|        - |  6303 | `/*` |
|        - |  6304 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6305 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6306 | ` * See block-comment on that function for additional information.` |
|        - |  6307 | ` */` |
|    14578 |  6308 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6309 |  |
|        - |  6310 | `	ph7_value *pStack;` |
|        - |  6311 | `	sxi32 rc;` |
|        - |  6312 | `	/* Allocate a new operand stack */` |
|    14580 |  6313 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14580 |  6314 | `	if( pStack == 0 ){` |
|      ! 0 |  6315 | `		return SXERR_MEM;` |
|        - |  6316 | `	}` |
|        - |  6317 | `	/* Execute the program */` |
|    14580 |  6318 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6319 | `	/* Free the operand stack */` |
|    14580 |  6320 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6321 | `	/* Execution result */` |
|    14580 |  6322 | `	return rc;` |
|     7291 |  6323 |  |
|        - |  6324 | `/*` |
|        - |  6325 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6326 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6327 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6328 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6329 | ` * execution ends.` |
|        - |  6330 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6331 | ` * additional information.` |
|        - |  6332 | ` */` |
|     2280 |  6333 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6334 |  |
|        - |  6335 | `	VmShutdownCB *pEntry;` |
|        - |  6336 | `	ph7_value *apArg[10];` |
|        - |  6337 | `	sxu32 n,nEntry;` |
|        - |  6338 | `	int i;` |
|        - |  6339 | `	/* Point to the stack of registered callbacks */` |
|     2282 |  6340 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25082 |  6341 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    22802 |  6342 | `		apArg[i] = 0;` |
|    11402 |  6343 | `	}` |
|     2284 |  6344 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6345 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6346 | `		if( pEntry ){` |
|        - |  6347 | `			/* Prepare callback arguments if any */` |
|        3 |  6348 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6349 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6350 | `					break;` |
|        - |  6351 | `				}` |
|      ! 0 |  6352 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6353 | `			}` |
|        - |  6354 | `			/* Invoke the callback */` |
|        3 |  6355 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6356 | `			/*` |
|        - |  6357 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6358 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6359 | `			 */` |
|        3 |  6360 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6361 | `			if( pEntry ){` |
|        3 |  6362 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6363 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6364 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6365 | `				}` |
|        1 |  6366 | `			}` |
|        1 |  6367 | `		}` |
|        2 |  6368 | `	}` |
|     2282 |  6369 | `	SySetReset(&pVm->aShutdown);` |
|     2282 |  6370 |  |
|        - |  6371 | `/*` |
|        - |  6372 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6373 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6374 | ` * See block-comment on that function for additional information.` |
|        - |  6375 | ` */` |
|     2288 |  6376 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6377 |  |
|        - |  6378 | `	/* Make sure we are ready to execute this program */` |
|     2290 |  6379 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6380 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6381 | `	}` |
|        - |  6382 | `	/* Set the execution magic number  */` |
|     2290 |  6383 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6384 | `	/* Execute the program */` |
|     2290 |  6385 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6386 | `	/* Invoke any shutdown callbacks */` |
|     2286 |  6387 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6388 | `	/*` |
|        - |  6389 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6390 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6391 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6392 | `	 */` |
|     2286 |  6393 | `	return SXRET_OK;` |
|     1146 |  6394 |  |
|        - |  6395 | `/*` |
|        - |  6396 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6397 | ` * the desired message.` |
|        - |  6398 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6399 | ` * in 'api.c' for additional information.` |
|        - |  6400 | ` */` |
|      350 |  6401 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6402 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6403 | `	SyString *pString /* Message to output */` |
|        - |  6404 | `	)` |
|        2 |  6405 |  |
|      352 |  6406 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6407 | `	sxi32 rc = SXRET_OK;` |
|        - |  6408 | `	/* Call the output consumer */` |
|      352 |  6409 | `	if( pString->nByte > 0 ){` |
|      352 |  6410 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6411 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6412 | `			/* Increment output length */` |
|       17 |  6413 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6414 | `		}` |
|      175 |  6415 | `	}` |
|      352 |  6416 | `	return rc;` |
|        2 |  6417 |  |
|        - |  6418 | `/*` |
|        - |  6419 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6420 | ` * callback to consume the formatted message.` |
|        - |  6421 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6422 | ` * in 'api.c' for additional information.` |
|        - |  6423 | ` */` |
|        2 |  6424 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6425 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6426 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6427 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6428 | `	)` |
|        1 |  6429 |  |
|        3 |  6430 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6431 | `	sxi32 rc = SXRET_OK;` |
|        - |  6432 | `	SyBlob sWorker;` |
|        - |  6433 | `	/* Format the message and call the output consumer */` |
|        3 |  6434 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6435 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6436 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6437 | `		/* Consume the formatted message */` |
|        3 |  6438 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6439 | `	}` |
|        3 |  6440 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6441 | `		/* Increment output length */` |
|      ! 0 |  6442 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6443 | `	}` |
|        - |  6444 | `	/* Release the working buffer */` |
|        3 |  6445 | `	SyBlobRelease(&sWorker);` |
|        3 |  6446 | `	return rc;` |
|        1 |  6447 |  |
|        - |  6448 | `/*` |
|        - |  6449 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6450 | ` * This function never fail and always return a pointer` |
|        - |  6451 | ` * to a null terminated string.` |
|        - |  6452 | ` */` |
|       12 |  6453 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6454 |  |
|       13 |  6455 | `	const char *zOp = "Unknown     ";` |
|       13 |  6456 | `	switch(nOp){` |
|        3 |  6457 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6458 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6459 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6460 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6461 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6462 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6463 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6464 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6465 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6466 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6467 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6468 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6469 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6470 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6471 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6472 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6473 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6474 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6475 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6476 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6477 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6478 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6479 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6480 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6481 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6482 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6483 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6484 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6485 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6486 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6487 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6488 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6489 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6490 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6491 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6492 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6493 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6494 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6495 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6496 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6497 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6498 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6499 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6500 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6501 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6502 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6503 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6504 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6505 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6506 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6507 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6508 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6509 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6510 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6511 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6512 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6513 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6514 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6515 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6516 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6517 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6518 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6519 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6520 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6521 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6522 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6523 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6524 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6525 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6526 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6527 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6528 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6529 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6530 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6531 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6532 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6533 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6534 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6535 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6536 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6537 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6538 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6539 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6540 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6541 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6542 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6543 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6544 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6545 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6546 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6547 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6548 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6549 | `	default:` |
|      ! 0 |  6550 | `		break;` |
|        - |  6551 | `	}` |
|       13 |  6552 | `	return zOp;` |
|        1 |  6553 |  |
|        - |  6554 | `/*` |
|        - |  6555 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6556 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6557 | ` * is responsible of consuming the generated dump.` |
|        - |  6558 | ` */` |
|        2 |  6559 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6560 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6561 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6562 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6563 | `	)` |
|        1 |  6564 |  |
|        - |  6565 | `	sxi32 rc;` |
|        3 |  6566 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6567 | `	return rc;` |
|        1 |  6568 |  |
|        - |  6569 | `/*` |
|        - |  6570 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6571 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6572 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6573 | ` * in 'compile.c' for additional information.` |
|        - |  6574 | ` */` |
|        8 |  6575 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6576 |  |
|        9 |  6577 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6578 | `	/* Evaluate and expand constant value */` |
|        9 |  6579 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6580 |  |
|        - |  6581 | `/*` |
|        - |  6582 | ` * Section:` |
|        - |  6583 | ` *  Function handling functions.` |
|        - |  6584 | ` * Status:` |
|        - |  6585 | ` *    Stable.` |
|        - |  6586 | ` */` |
|        - |  6587 | `/*` |
|        - |  6588 | ` * int func_num_args(void)` |
|        - |  6589 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6590 | ` * Parameters` |
|        - |  6591 | ` *   None.` |
|        - |  6592 | ` * Return` |
|        - |  6593 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6594 | ` *  or -1 if called from the globe scope.` |
|        - |  6595 | ` */` |
|      906 |  6596 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6597 |  |
|        - |  6598 | `	VmFrame *pFrame;` |
|        - |  6599 | `	ph7_vm *pVm;` |
|        - |  6600 | `	/* Point to the target VM */` |
|      908 |  6601 | `	pVm = pCtx->pVm;` |
|        - |  6602 | `	/* Current frame */` |
|      908 |  6603 | `	pFrame = pVm->pFrame;` |
|      908 |  6604 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6605 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6606 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6607 | `	}` |
|      908 |  6608 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6609 | `		SXUNUSED(nArg);` |
|      ! 0 |  6610 | `		SXUNUSED(apArg);` |
|        - |  6611 | `		/* Global frame,return -1 */` |
|      ! 0 |  6612 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6613 | `		return SXRET_OK;` |
|        - |  6614 | `	}` |
|        - |  6615 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6616 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6617 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6618 | `	return SXRET_OK;` |
|      455 |  6619 |  |
|        - |  6620 | `/*` |
|        - |  6621 | ` * value func_get_arg(int $arg_num)` |
|        - |  6622 | ` *   Return an item from the argument list.` |
|        - |  6623 | ` * Parameters` |
|        - |  6624 | ` *  Argument number(index start from zero).` |
|        - |  6625 | ` * Return` |
|        - |  6626 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6627 | ` */` |
|       22 |  6628 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6629 |  |
|       24 |  6630 | `	ph7_value *pObj = 0;` |
|       24 |  6631 | `	VmSlot *pSlot = 0;` |
|        - |  6632 | `	VmFrame *pFrame;` |
|        - |  6633 | `	ph7_vm *pVm;` |
|        - |  6634 | `	/* Point to the target VM */` |
|       24 |  6635 | `	pVm = pCtx->pVm;` |
|        - |  6636 | `	/* Current frame */` |
|       24 |  6637 | `	pFrame = pVm->pFrame;` |
|       24 |  6638 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6639 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6640 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6641 | `	}` |
|       24 |  6642 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6643 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6644 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6645 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6646 | `		return SXRET_OK;` |
|        - |  6647 | `	}` |
|        - |  6648 | `	/* Extract the desired index */` |
|       21 |  6649 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6650 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6651 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6652 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6653 | `		return SXRET_OK;` |
|        - |  6654 | `	}` |
|        - |  6655 | `	/* Extract the desired argument */` |
|       21 |  6656 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6657 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6658 | `			/* Return the desired argument */` |
|       21 |  6659 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6660 | `		}else{` |
|        - |  6661 | `			/* No such argument,return false */` |
|      ! 0 |  6662 | `			ph7_result_bool(pCtx,0);` |
|        - |  6663 | `		}` |
|       11 |  6664 | `	}else{` |
|        - |  6665 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6666 | `		ph7_result_bool(pCtx,0);` |
|        - |  6667 | `	}` |
|       21 |  6668 | `	return SXRET_OK;` |
|       13 |  6669 |  |
|        - |  6670 | `/*` |
|        - |  6671 | ` * array func_get_args_byref(void)` |
|        - |  6672 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6673 | ` * Parameters` |
|        - |  6674 | ` *  None.` |
|        - |  6675 | ` * Return` |
|        - |  6676 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6677 | ` *  member of the current user-defined function's argument list.` |
|        - |  6678 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6679 | ` * NOTE:` |
|        - |  6680 | ` *  Arguments are returned to the array by reference.` |
|        - |  6681 | ` */` |
|        2 |  6682 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6683 |  |
|        - |  6684 | `	ph7_value *pArray;` |
|        - |  6685 | `	VmFrame *pFrame;` |
|        - |  6686 | `	VmSlot *aSlot;` |
|        - |  6687 | `	sxu32 n;` |
|        - |  6688 | `	/* Point to the current frame */` |
|        3 |  6689 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6690 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6691 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6692 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6693 | `	}` |
|        3 |  6694 | `	if( pFrame->pParent == 0 ){` |
|        - |  6695 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6696 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6697 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6698 | `		return SXRET_OK;` |
|        - |  6699 | `	}` |
|        - |  6700 | `	/* Create a new array */` |
|        3 |  6701 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6702 | `	if( pArray == 0 ){` |
|      ! 0 |  6703 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6704 | `		SXUNUSED(apArg);` |
|      ! 0 |  6705 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6706 | `		return SXRET_OK;` |
|        - |  6707 | `	}` |
|        - |  6708 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6709 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6710 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6711 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6712 | `	}` |
|        - |  6713 | `	/* Return the freshly created array */` |
|        3 |  6714 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6715 | `	return SXRET_OK;` |
|        2 |  6716 |  |
|        - |  6717 | `/*` |
|        - |  6718 | ` * array func_get_args(void)` |
|        - |  6719 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6720 | ` * Parameters` |
|        - |  6721 | ` *  None.` |
|        - |  6722 | ` * Return` |
|        - |  6723 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6724 | ` *  member of the current user-defined function's argument list.` |
|        - |  6725 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6726 | ` */` |
|       62 |  6727 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6728 |  |
|       64 |  6729 | `	ph7_value *pObj = 0;` |
|        - |  6730 | `	ph7_value *pArray;` |
|        - |  6731 | `	VmFrame *pFrame;` |
|        - |  6732 | `	VmSlot *aSlot;` |
|        - |  6733 | `	sxu32 n;` |
|        - |  6734 | `	/* Point to the current frame */` |
|       64 |  6735 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6736 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6737 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6738 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6739 | `	}` |
|       64 |  6740 | `	if( pFrame->pParent == 0 ){` |
|        - |  6741 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6742 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6743 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6744 | `		return SXRET_OK;` |
|        - |  6745 | `	}` |
|        - |  6746 | `	/* Create a new array */` |
|       64 |  6747 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6748 | `	if( pArray == 0 ){` |
|      ! 0 |  6749 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6750 | `		SXUNUSED(apArg);` |
|      ! 0 |  6751 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6752 | `		return SXRET_OK;` |
|        - |  6753 | `	}` |
|        - |  6754 | `	/* Start filling the array with the given arguments */` |
|       64 |  6755 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6756 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6757 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6758 | `		if( pObj ){` |
|      130 |  6759 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6760 | `		}` |
|       66 |  6761 | `	}` |
|        - |  6762 | `	/* Return the freshly created array */` |
|       64 |  6763 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6764 | `	return SXRET_OK;` |
|       33 |  6765 |  |
|        - |  6766 | `/*` |
|        - |  6767 | ` * bool function_exists(string $name)` |
|        - |  6768 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6769 | ` * Parameters` |
|        - |  6770 | ` *  The name of the desired function.` |
|        - |  6771 | ` * Return` |
|        - |  6772 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6773 | ` */` |
|     1646 |  6774 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6775 |  |
|        - |  6776 | `	const char *zName;` |
|        - |  6777 | `	ph7_vm *pVm;` |
|        - |  6778 | `	int nLen;` |
|        - |  6779 | `	int res;` |
|     1648 |  6780 | `	if( nArg < 1 ){` |
|        - |  6781 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6782 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6783 | `		return SXRET_OK;` |
|        - |  6784 | `	}` |
|        - |  6785 | `	/* Point to the target VM */` |
|     1648 |  6786 | `	pVm = pCtx->pVm;` |
|        - |  6787 | `	/* Extract the function name */` |
|     1648 |  6788 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6789 | `	/* Assume the function is not defined */` |
|     1648 |  6790 | `	res = 0;` |
|        - |  6791 | `	/* Perform the lookup */` |
|     2469 |  6792 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1642 |  6793 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6794 | `			/* Function is defined */` |
|      206 |  6795 | `			res = 1;` |
|      102 |  6796 | `	}` |
|     1648 |  6797 | `	ph7_result_bool(pCtx,res);` |
|     1648 |  6798 | `	return SXRET_OK;` |
|      825 |  6799 |  |
|        - |  6800 | `/*` |
|        - |  6801 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6802 | ` * [i.e: Whether it is callable or not].` |
|        - |  6803 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6804 | ` */` |
|    16002 |  6805 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6806 |  |
|    16004 |  6807 | `	int res = 0;` |
|    16004 |  6808 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6809 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6810 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6811 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6812 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6813 | `		if( pMethod && CallInvoke ){` |
|        - |  6814 | `			ph7_value sResult;` |
|        - |  6815 | `			sxi32 rc;` |
|        - |  6816 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6817 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6818 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6819 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6820 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6821 | `			}` |
|      ! 0 |  6822 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6823 | `		}` |
|    16004 |  6824 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6825 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6826 | `		if( pMap->nEntry == 2 ){` |
|        - |  6827 | `			ph7_class *pClass;` |
|        - |  6828 | `			ph7_value *pV;` |
|        - |  6829 | `			/* Extract the target class */` |
|       12 |  6830 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6831 | `			if( pV ){` |
|       12 |  6832 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6833 | `				if( pClass ){` |
|        - |  6834 | `					ph7_class_method *pMethod;` |
|        - |  6835 | `					/* Extract the target method */` |
|       10 |  6836 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6837 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6838 | `						/* Perform the lookup */` |
|       10 |  6839 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6840 | `						if( pMethod ){` |
|        - |  6841 | `							/* Method is callable */` |
|        5 |  6842 | `							res = 1;` |
|        2 |  6843 | `						}` |
|        4 |  6844 | `					}` |
|        4 |  6845 | `				}` |
|        5 |  6846 | `			}` |
|        7 |  6847 | `		}` |
|    15991 |  6848 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6849 | `		const char *zName;` |
|        - |  6850 | `		int nLen;` |
|        - |  6851 | `		/* Extract the name */` |
|     4700 |  6852 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6853 | `		/* Perform the lookup */` |
|     4715 |  6854 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6855 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6856 | `				/* Function is callable */` |
|     4682 |  6857 | `				res = 1;` |
|     2340 |  6858 | `		}` |
|     2349 |  6859 | `	}` |
|    16004 |  6860 | `	return res;` |
|        2 |  6861 |  |
|        - |  6862 | `/*` |
|        - |  6863 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6864 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6865 | ` * Parameters` |
|        - |  6866 | ` * $name` |
|        - |  6867 | ` *    The callback function to check` |
|        - |  6868 | ` * $syntax_only` |
|        - |  6869 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6870 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6871 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6872 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6873 | ` *    a string.` |
|        - |  6874 | ` * Return` |
|        - |  6875 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6876 | ` */` |
|       14 |  6877 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6878 |  |
|        - |  6879 | `	ph7_vm *pVm;` |
|        - |  6880 | `	int res;` |
|       15 |  6881 | `	if( nArg < 1 ){` |
|        - |  6882 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6883 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6884 | `		return SXRET_OK;` |
|        - |  6885 | `	}` |
|        - |  6886 | `	/* Point to the target VM */` |
|       15 |  6887 | `	pVm = pCtx->pVm;` |
|        - |  6888 | `	/* Perform the requested operation */` |
|       15 |  6889 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6890 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6891 | `	return SXRET_OK;` |
|        8 |  6892 |  |
|        - |  6893 | `/*` |
|        - |  6894 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6895 | ` * defined below.` |
|        - |  6896 | ` */` |
|     1082 |  6897 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6898 |  |
|     1083 |  6899 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6900 | `	ph7_value sName;` |
|        - |  6901 | `	sxi32 rc;` |
|        - |  6902 | `	/* Prepare the function name for insertion */` |
|     1083 |  6903 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6904 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6905 | `	/* Perform the insertion */` |
|     1083 |  6906 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6907 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6908 | `	return rc;` |
|        1 |  6909 |  |
|        - |  6910 | `/*` |
|        - |  6911 | ` * array get_defined_functions(void)` |
|        - |  6912 | ` *  Returns an array of all defined functions.` |
|        - |  6913 | ` * Parameter` |
|        - |  6914 | ` *  None.` |
|        - |  6915 | ` * Return` |
|        - |  6916 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6917 | ` *  both built-in (internal) and user-defined.` |
|        - |  6918 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6919 | ` *  defined ones using $arr["user"].` |
|        - |  6920 | ` * Note:` |
|        - |  6921 | ` *  NULL is returned on failure.` |
|        - |  6922 | ` */` |
|        2 |  6923 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6924 |  |
|        - |  6925 | `	ph7_value *pArray,*pEntry;` |
|        - |  6926 | `	/* NOTE:` |
|        - |  6927 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6928 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6929 | `	 */` |
|        3 |  6930 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6931 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6932 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6933 | `		SXUNUSED(apArg);` |
|        - |  6934 | `		/* Return NULL */` |
|      ! 0 |  6935 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6936 | `		return SXRET_OK;` |
|        - |  6937 | `	}` |
|        3 |  6938 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6939 | `	if( pEntry == 0 ){` |
|        - |  6940 | `		/* Return NULL */` |
|      ! 0 |  6941 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6942 | `		return SXRET_OK;` |
|        - |  6943 | `	}` |
|        - |  6944 | `	/* Fill with the appropriate information */` |
|        3 |  6945 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6946 | `	/* Create the 'internal' index */` |
|        3 |  6947 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6948 | `	/* Create the user-func array */` |
|        3 |  6949 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6950 | `	if( pEntry == 0 ){` |
|        - |  6951 | `		/* Return NULL */` |
|      ! 0 |  6952 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6953 | `		return SXRET_OK;` |
|        - |  6954 | `	}` |
|        - |  6955 | `	/* Fill with the appropriate information */` |
|        3 |  6956 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6957 | `	/* Create the 'user' index */` |
|        3 |  6958 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6959 | `	/* Return the multi-dimensional array */` |
|        3 |  6960 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6961 | `	return SXRET_OK;` |
|        2 |  6962 |  |
|        - |  6963 | `/*` |
|        - |  6964 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6965 | ` *  Register a function for execution on shutdown.` |
|        - |  6966 | ` * Note` |
|        - |  6967 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6968 | ` *  be called in the same order as they were registered.` |
|        - |  6969 | ` * Parameters` |
|        - |  6970 | ` *  $callback` |
|        - |  6971 | ` *   The shutdown callback to register.` |
|        - |  6972 | ` * $param` |
|        - |  6973 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6974 | ` * Return` |
|        - |  6975 | ` *  Nothing.` |
|        - |  6976 | ` */` |
|        2 |  6977 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6978 |  |
|        - |  6979 | `	VmShutdownCB sEntry;` |
|        - |  6980 | `	int i,j;` |
|        3 |  6981 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6982 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6983 | `		return PH7_OK;` |
|        - |  6984 | `	}` |
|        - |  6985 | `	/* Zero the Entry */` |
|        3 |  6986 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6987 | `	/* Initialize fields */` |
|        3 |  6988 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6989 | `	/* Save the callback name for later invocation name */` |
|        3 |  6990 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6991 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6992 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6993 | `	}` |
|        - |  6994 | `	/* Copy arguments */` |
|        3 |  6995 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6996 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6997 | `			/* Limit reached */` |
|      ! 0 |  6998 | `			break;` |
|        - |  6999 | `		}` |
|      ! 0 |  7000 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  7001 | `	}` |
|        3 |  7002 | `	sEntry.nArg = j;` |
|        - |  7003 | `	/* Install the callback */` |
|        3 |  7004 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  7005 | `	return PH7_OK;` |
|        2 |  7006 |  |
|        - |  7007 | `/*` |
|        - |  7008 | ` * Section:` |
|        - |  7009 | ` *  Class handling functions.` |
|        - |  7010 | ` * Status:` |
|        - |  7011 | ` *    Stable.` |
|        - |  7012 | ` */` |
|        - |  7013 | `/*` |
|        - |  7014 | ` * Extract the top active class. NULL is returned` |
|        - |  7015 | ` * if the class stack is empty.` |
|        - |  7016 | ` */` |
|      550 |  7017 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  7018 |  |
|      552 |  7019 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  7020 | `	ph7_class **apClass;` |
|      552 |  7021 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  7022 | `		/* Empty stack,return NULL */` |
|       15 |  7023 | `		return 0;` |
|        - |  7024 | `	}` |
|        - |  7025 | `	/* Peek the last entry */` |
|      538 |  7026 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      538 |  7027 | `	return apClass[pSet->nUsed - 1];` |
|      277 |  7028 |  |
|        - |  7029 | `/*` |
|        - |  7030 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  7031 | ` *   Get the class that declared the currently executing method.` |
|        - |  7032 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  7033 | ` *` |
|        - |  7034 | ` * Parameters` |
|        - |  7035 | ` *   pVm: Target VM` |
|        - |  7036 | ` *` |
|        - |  7037 | ` * Return` |
|        - |  7038 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  7039 | ` *   - Not executing within a class method` |
|        - |  7040 | ` *` |
|        - |  7041 | ` * Note` |
|        - |  7042 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  7043 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  7044 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  7045 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  7046 | ` *   declaring class.` |
|        - |  7047 | ` */` |
|       52 |  7048 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  7049 |  |
|       54 |  7050 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  7051 | `	ph7_vm_func *pVmFunc;` |
|        - |  7052 |  |
|        - |  7053 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  7054 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  7055 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  7056 | `	}` |
|        - |  7057 |  |
|        - |  7058 | `	/* Check if we're in a method context */` |
|       54 |  7059 | `	if( pFrame->pParent ){` |
|       50 |  7060 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  7061 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  7062 | `			/* Return the declaring class */` |
|       50 |  7063 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  7064 | `		}` |
|      ! 0 |  7065 | `	}` |
|        - |  7066 |  |
|        5 |  7067 | `	return 0;` |
|       28 |  7068 |  |
|        - |  7069 |  |
|        - |  7070 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  7071 | `/*` |
|        - |  7072 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7073 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7074 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7075 | ` * return value indicates failure.` |
|        - |  7076 | ` */` |
|     1298 |  7077 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7078 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7079 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7080 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7081 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7082 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7083 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7084 | `	)` |
|        2 |  7085 |  |
|        - |  7086 | `	ph7_value *aStack;` |
|        - |  7087 | `	VmInstr aInstr[2];` |
|        - |  7088 | `	int iCursor;` |
|        - |  7089 | `	int i;` |
|        - |  7090 | `	/* Create a new operand stack */` |
|     1300 |  7091 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1300 |  7092 | `	if( aStack == 0 ){` |
|      ! 0 |  7093 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7094 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7095 | `		return SXERR_MEM;` |
|        - |  7096 | `	}` |
|        - |  7097 | `	/* Fill the operand stack with the given arguments */` |
|     1872 |  7098 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      574 |  7099 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7100 | `		/*` |
|        - |  7101 | `		 * Symisc eXtension:` |
|        - |  7102 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7103 | `		 */` |
|      574 |  7104 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      288 |  7105 | `	}` |
|     1300 |  7106 | `	iCursor = nArg + 1;` |
|     1300 |  7107 | `	if( pThis ){` |
|        - |  7108 | `		/*` |
|        - |  7109 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7110 | `		 */` |
|     1294 |  7111 | `		pThis->iRef++; /* Increment reference count */` |
|     1294 |  7112 | `		aStack[i].x.pOther = pThis;` |
|     1294 |  7113 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      646 |  7114 | `	}` |
|     1300 |  7115 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1300 |  7116 | `	i++;` |
|        - |  7117 | `	/* Push method name */` |
|     1300 |  7118 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1300 |  7119 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1300 |  7120 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1300 |  7121 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7122 | `	/* Emit the CALL istruction */` |
|     1300 |  7123 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1300 |  7124 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1300 |  7125 | `	aInstr[0].iP2 = 0;` |
|     1300 |  7126 | `	aInstr[0].p3  = 0;` |
|        - |  7127 | `	/* Emit the DONE instruction */` |
|     1300 |  7128 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1300 |  7129 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1300 |  7130 | `	aInstr[1].iP2 = 0;` |
|     1300 |  7131 | `	aInstr[1].p3  = 0;` |
|        - |  7132 | `	/* Execute the method body (if available) */` |
|     1300 |  7133 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7134 | `	/* Clean up the mess left behind */` |
|     1300 |  7135 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1300 |  7136 | `	return PH7_OK;` |
|      651 |  7137 |  |
|        - |  7138 | `/*` |
|        - |  7139 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7140 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7141 | ` * in the apArg[] array.` |
|        - |  7142 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7143 | ` * return value indicates failure.` |
|        - |  7144 | ` */` |
|      926 |  7145 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7146 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7147 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7148 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7149 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7150 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7151 | `	)` |
|        2 |  7152 |  |
|        - |  7153 | `	ph7_value *aStack;` |
|        - |  7154 | `	VmInstr aInstr[2];` |
|        - |  7155 | `	int i;` |
|      928 |  7156 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7157 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7158 | `		if( pResult ){` |
|        - |  7159 | `			/* Assume a null return value */` |
|      ! 0 |  7160 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7161 | `		}` |
|      471 |  7162 | `		return SXERR_INVALID;` |
|        - |  7163 | `	}` |
|      458 |  7164 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7165 | `		/* Class method */` |
|       11 |  7166 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7167 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7168 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7169 | `		ph7_class *pClass = 0;` |
|        - |  7170 | `		ph7_value *pValue;` |
|        - |  7171 | `		sxi32 rc;` |
|       11 |  7172 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7173 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7174 | `			if( pResult ){` |
|        - |  7175 | `				/* Assume a null return value */` |
|      ! 0 |  7176 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7177 | `			}` |
|      ! 0 |  7178 | `			return SXRET_OK;` |
|        - |  7179 | `		}` |
|        - |  7180 | `		/* Extract the class name or an instance of it */` |
|       11 |  7181 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7182 | `		if( pValue ){` |
|       11 |  7183 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7184 | `		}` |
|       11 |  7185 | `		if( pClass == 0 ){` |
|        - |  7186 | `			/* No such class,return NULL */` |
|      ! 0 |  7187 | `			if( pResult ){` |
|      ! 0 |  7188 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7189 | `			}` |
|      ! 0 |  7190 | `			return SXRET_OK;` |
|        - |  7191 | `		}` |
|       11 |  7192 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7193 | `			/* Point to the class instance */` |
|        5 |  7194 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7195 | `		}` |
|        - |  7196 | `		/* Try to extract the method */` |
|       11 |  7197 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7198 | `		if( pValue ){` |
|       11 |  7199 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7200 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7201 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7202 | `			}` |
|        5 |  7203 | `		}` |
|       11 |  7204 | `		if( pMethod == 0 ){` |
|        - |  7205 | `			/* No such method,return NULL */` |
|      ! 0 |  7206 | `			if( pResult ){` |
|      ! 0 |  7207 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7208 | `			}` |
|      ! 0 |  7209 | `			return SXRET_OK;` |
|        - |  7210 | `		}` |
|        - |  7211 | `		/* Call the class method */` |
|       11 |  7212 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7213 | `		return rc;` |
|        - |  7214 | `	}` |
|        - |  7215 | `	/* Create a new operand stack */` |
|      448 |  7216 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7217 | `	if( aStack == 0 ){` |
|      ! 0 |  7218 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7219 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7220 | `		if( pResult ){` |
|        - |  7221 | `			/* Assume a null return value */` |
|      ! 0 |  7222 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7223 | `		}` |
|      ! 0 |  7224 | `		return SXERR_MEM;` |
|        - |  7225 | `	}` |
|        - |  7226 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7227 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7228 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7229 | `		/*` |
|        - |  7230 | `		 * Symisc eXtension:` |
|        - |  7231 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7232 | `		 */` |
|     1024 |  7233 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7234 | `	}` |
|        - |  7235 | `	/* Push the function name */` |
|      448 |  7236 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7237 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7238 | `	/* Emit the CALL istruction */` |
|      448 |  7239 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7240 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7241 | `	aInstr[0].iP2 = 0;` |
|      448 |  7242 | `	aInstr[0].p3  = 0;` |
|        - |  7243 | `	/* Emit the DONE instruction */` |
|      448 |  7244 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7245 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7246 | `	aInstr[1].iP2 = 0;` |
|      448 |  7247 | `	aInstr[1].p3  = 0;` |
|        - |  7248 | `	/* Execute the function body (if available) */` |
|      448 |  7249 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7250 | `	/* Clean up the mess left behind */` |
|      448 |  7251 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7252 | `	return PH7_OK;` |
|      465 |  7253 |  |
|        - |  7254 | `/*` |
|        - |  7255 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7256 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7257 | ` * parameter.` |
|        - |  7258 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7259 | ` * return value indicates failure.` |
|        - |  7260 | ` */` |
|      236 |  7261 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7262 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7263 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7264 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7265 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7266 | `	)` |
|        1 |  7267 |  |
|        - |  7268 | `	ph7_value *pArg;` |
|        - |  7269 | `	SySet aArg;` |
|        - |  7270 | `	va_list ap;` |
|        - |  7271 | `	sxi32 rc;` |
|      237 |  7272 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7273 | `	/* Copy arguments one after one */` |
|      237 |  7274 | `	va_start(ap,pResult);` |
|      393 |  7275 | `	for(;;){` |
|      787 |  7276 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7277 | `		if( pArg == 0 ){` |
|      237 |  7278 | `			break;` |
|        - |  7279 | `		}` |
|      551 |  7280 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7281 | `	}` |
|        - |  7282 | `	/* Call the core routine */` |
|      237 |  7283 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7284 | `	/* Cleanup */` |
|      237 |  7285 | `	SySetRelease(&aArg);` |
|      237 |  7286 | `	return rc;` |
|        1 |  7287 |  |
|        - |  7288 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7289 | `/*` |
|        - |  7290 | ` * bool defined(string $name)` |
|        - |  7291 | ` *  Checks whether a given named constant exists.` |
|        - |  7292 | ` * Parameter:` |
|        - |  7293 | ` *  Name of the desired constant.` |
|        - |  7294 | ` * Return` |
|        - |  7295 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7296 | ` */` |
|       14 |  7297 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7298 |  |
|        - |  7299 | `	const char *zName;` |
|       16 |  7300 | `	int nLen = 0;` |
|       16 |  7301 | `	int res = 0;` |
|       16 |  7302 | `	if( nArg < 1 ){` |
|        - |  7303 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7304 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7305 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7306 | `		return SXRET_OK;` |
|        - |  7307 | `	}` |
|        - |  7308 | `	/* Extract constant name */` |
|       16 |  7309 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7310 | `	/* Perform the lookup */` |
|       16 |  7311 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7312 | `		/* Already defined */` |
|       10 |  7313 | `		res = 1;` |
|        4 |  7314 | `	}` |
|       16 |  7315 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7316 | `	return SXRET_OK;` |
|        9 |  7317 |  |
|        - |  7318 | `/*` |
|        - |  7319 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7320 | ` * below.` |
|        - |  7321 | ` */` |
|        8 |  7322 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7323 |  |
|       10 |  7324 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7325 | `	/* Expand constant value */` |
|       10 |  7326 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7327 |  |
|        - |  7328 | `/*` |
|        - |  7329 | ` * bool define(string $constant_name,expression value)` |
|        - |  7330 | ` *  Defines a named constant at runtime.` |
|        - |  7331 | ` * Parameter:` |
|        - |  7332 | ` *  $constant_name` |
|        - |  7333 | ` *   The name of the constant` |
|        - |  7334 | ` *  $value` |
|        - |  7335 | ` *   Constant value` |
|        - |  7336 | ` * Return:` |
|        - |  7337 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7338 | ` */` |
|       10 |  7339 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7340 |  |
|        - |  7341 | `	const char *zName;  /* Constant name */` |
|        - |  7342 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7343 | `	int nLen = 0;       /* Name length */` |
|        - |  7344 | `	sxi32 rc;` |
|       12 |  7345 | `	if( nArg < 2 ){` |
|        - |  7346 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7347 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7348 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7349 | `		return SXRET_OK;` |
|        - |  7350 | `	}` |
|       12 |  7351 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7352 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7353 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7354 | `		return SXRET_OK;` |
|        - |  7355 | `	}` |
|        - |  7356 | `	/* Extract constant name */` |
|       12 |  7357 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7358 | `	if( nLen < 1 ){` |
|      ! 0 |  7359 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7360 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7361 | `		return SXRET_OK;` |
|        - |  7362 | `	}` |
|        - |  7363 | `	/* Duplicate constant value */` |
|       12 |  7364 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7365 | `	if( pValue == 0 ){` |
|      ! 0 |  7366 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7367 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7368 | `		return SXRET_OK;` |
|        - |  7369 | `	}` |
|        - |  7370 | `	/* Initialize the memory object */` |
|       12 |  7371 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7372 | `	/* Register the constant */` |
|       12 |  7373 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7374 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7375 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7376 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7377 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7378 | `		return SXRET_OK;` |
|        - |  7379 | `	}` |
|        - |  7380 | `	/* Duplicate constant value */` |
|       12 |  7381 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7382 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7383 | `		/* Lower case the constant name */` |
|      ! 0 |  7384 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7385 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7386 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7387 | `				/* UTF-8 stream */` |
|      ! 0 |  7388 | `				zCur++;` |
|      ! 0 |  7389 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7390 | `					zCur++;` |
|      ! 0 |  7391 | `				}` |
|      ! 0 |  7392 | `				continue;` |
|        - |  7393 | `			}` |
|      ! 0 |  7394 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7395 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7396 | `				zCur[0] = (char)c;` |
|      ! 0 |  7397 | `			}` |
|      ! 0 |  7398 | `			zCur++;` |
|      ! 0 |  7399 | `		}` |
|        - |  7400 | `		/* Finally,register the constant */` |
|      ! 0 |  7401 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7402 | `	}` |
|        - |  7403 | `	/* All done,return TRUE */` |
|       12 |  7404 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7405 | `	return SXRET_OK;` |
|        7 |  7406 |  |
|        - |  7407 | `/*` |
|        - |  7408 | ` * value constant(string $name)` |
|        - |  7409 | ` *  Returns the value of a constant` |
|        - |  7410 | ` * Parameter` |
|        - |  7411 | ` *  $name` |
|        - |  7412 | ` *    Name of the constant.` |
|        - |  7413 | ` * Return` |
|        - |  7414 | ` *  Constant value or NULL if not defined.` |
|        - |  7415 | ` */` |
|        8 |  7416 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7417 |  |
|        - |  7418 | `	SyHashEntry *pEntry;` |
|        - |  7419 | `	ph7_constant *pCons;` |
|        - |  7420 | `	const char *zName; /* Constant name */` |
|        - |  7421 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7422 | `	int nLen;` |
|       10 |  7423 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7424 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7425 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7426 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7427 | `		return SXRET_OK;` |
|        - |  7428 | `	}` |
|        - |  7429 | `	/* Extract the constant name */` |
|       10 |  7430 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7431 | `	/* Perform the query */` |
|       10 |  7432 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7433 | `	if( pEntry == 0 ){` |
|        3 |  7434 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7435 | `		ph7_result_null(pCtx);` |
|        3 |  7436 | `		return SXRET_OK;` |
|        - |  7437 | `	}` |
|        8 |  7438 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7439 | `	/* Point to the structure that describe the constant */` |
|        8 |  7440 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7441 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7442 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7443 | `	/* Return that value */` |
|        8 |  7444 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7445 | `	/* Cleanup */` |
|        8 |  7446 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7447 | `	return SXRET_OK;` |
|        6 |  7448 |  |
|        - |  7449 | `/*` |
|        - |  7450 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7451 | ` * defined below.` |
|        - |  7452 | ` */` |
|      416 |  7453 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7454 |  |
|      417 |  7455 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7456 | `	ph7_value sName;` |
|        - |  7457 | `	sxi32 rc;` |
|        - |  7458 | `	/* Prepare the constant name for insertion */` |
|      417 |  7459 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7460 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7461 | `	/* Perform the insertion */` |
|      417 |  7462 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7463 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7464 | `	return rc;` |
|        1 |  7465 |  |
|        - |  7466 | `/*` |
|        - |  7467 | ` * array get_defined_constants(void)` |
|        - |  7468 | ` *  Returns an associative array with the names of all defined` |
|        - |  7469 | ` *  constants.` |
|        - |  7470 | ` * Parameters` |
|        - |  7471 | ` *  NONE.` |
|        - |  7472 | ` * Returns` |
|        - |  7473 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7474 | ` */` |
|        2 |  7475 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7476 |  |
|        - |  7477 | `	ph7_value *pArray;` |
|        - |  7478 | `	/* Create the array first*/` |
|        3 |  7479 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7480 | `	if( pArray == 0 ){` |
|      ! 0 |  7481 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7482 | `		SXUNUSED(apArg);` |
|        - |  7483 | `		/* Return NULL */` |
|      ! 0 |  7484 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7485 | `		return SXRET_OK;` |
|        - |  7486 | `	}` |
|        - |  7487 | `	/* Fill the array with the defined constants */` |
|        3 |  7488 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7489 | `	/* Return the created array */` |
|        3 |  7490 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7491 | `	return SXRET_OK;` |
|        2 |  7492 |  |
|        - |  7493 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7494 | `/*` |
|        - |  7495 | ` * Section:` |
|        - |  7496 | ` *  Random numbers/string generators.` |
|        - |  7497 | ` * Status:` |
|        - |  7498 | ` *    Stable.` |
|        - |  7499 | ` */` |
|        - |  7500 | `/*` |
|        - |  7501 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7502 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7503 | ` * used by te SQLite3 library.` |
|        - |  7504 | ` */` |
|     2362 |  7505 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7506 |  |
|        - |  7507 | `	sxu32 iNum;` |
|     2364 |  7508 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2364 |  7509 | `	return iNum;` |
|        2 |  7510 |  |
|        - |  7511 | `/*` |
|        - |  7512 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7513 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7514 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7515 | ` * by te SQLite3 library.` |
|        - |  7516 | ` */` |
|    74522 |  7517 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7518 |  |
|        - |  7519 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7520 | `	int i;` |
|        - |  7521 | `	/* Generate a binary string first */` |
|    74524 |  7522 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7523 | `	/* Turn the binary string into english based alphabet */` |
|   819912 |  7524 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   745390 |  7525 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   372696 |  7526 | `	 }` |
|    74524 |  7527 |  |
|        - |  7528 | `/*` |
|        - |  7529 | ` * int rand()` |
|        - |  7530 | ` * int mt_rand()` |
|        - |  7531 | ` * int rand(int $min,int $max)` |
|        - |  7532 | ` * int mt_rand(int $min,int $max)` |
|        - |  7533 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7534 | ` * Parameter` |
|        - |  7535 | ` *  $min` |
|        - |  7536 | ` *    The lowest value to return (default: 0)` |
|        - |  7537 | ` *  $max` |
|        - |  7538 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7539 | ` * Return` |
|        - |  7540 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7541 | ` * Note:` |
|        - |  7542 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7543 | ` *  by te SQLite3 library.` |
|        - |  7544 | ` */` |
|       20 |  7545 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7546 |  |
|        - |  7547 | `	sxu32 iNum;` |
|        - |  7548 | `	/* Generate the random number */` |
|       21 |  7549 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7550 | `	if( nArg > 1 ){` |
|        - |  7551 | `		sxu32 iMin,iMax;` |
|        3 |  7552 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7553 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7554 | `		if( iMin < iMax ){` |
|        3 |  7555 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7556 | `			if( iDiv > 0 ){` |
|        3 |  7557 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7558 | `			}` |
|        1 |  7559 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7560 | `			iNum %= iMax;` |
|      ! 0 |  7561 | `		}` |
|        1 |  7562 | `	}` |
|        - |  7563 | `	/* Return the number */` |
|       21 |  7564 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7565 | `	return SXRET_OK;` |
|        1 |  7566 |  |
|        - |  7567 | `/*` |
|        - |  7568 | ` * int getrandmax(void)` |
|        - |  7569 | ` * int mt_getrandmax(void)` |
|        - |  7570 | ` * int rc4_getrandmax(void)` |
|        - |  7571 | ` *   Show largest possible random value` |
|        - |  7572 | ` * Return` |
|        - |  7573 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7574 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7575 | ` * Note:` |
|        - |  7576 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7577 | ` *  by te SQLite3 library.` |
|        - |  7578 | ` */` |
|        4 |  7579 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7580 |  |
|        2 |  7581 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7582 | `	SXUNUSED(apArg);` |
|        5 |  7583 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7584 | `	return SXRET_OK;` |
|        1 |  7585 |  |
|        - |  7586 | `/*` |
|        - |  7587 | ` * string rand_str()` |
|        - |  7588 | ` * string rand_str(int $len)` |
|        - |  7589 | ` *  Generate a random string (English alphabet).` |
|        - |  7590 | ` * Parameter` |
|        - |  7591 | ` *  $len` |
|        - |  7592 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7593 | ` * Return` |
|        - |  7594 | ` *   A pseudo random string.` |
|        - |  7595 | ` * Note:` |
|        - |  7596 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7597 | ` *  by te SQLite3 library.` |
|        - |  7598 | ` *  This function is a symisc extension.` |
|        - |  7599 | ` */` |
|      120 |  7600 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7601 |  |
|        - |  7602 | `	char zString[1024];` |
|      122 |  7603 | `	int iLen = 0x10;` |
|      122 |  7604 | `	if( nArg > 0 ){` |
|        - |  7605 | `		/* Get the desired length */` |
|      122 |  7606 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7607 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7608 | `			/* Default length */` |
|        3 |  7609 | `			iLen = 0x10;` |
|        1 |  7610 | `		}` |
|       60 |  7611 | `	}` |
|        - |  7612 | `	/* Generate the random string */` |
|      122 |  7613 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7614 | `	/* Return the generated string */` |
|      122 |  7615 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7616 | `	return SXRET_OK;` |
|        2 |  7617 |  |
|        - |  7618 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7619 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7620 | `/* Unique ID private data */` |
|        - |  7621 | `struct unique_id_data` |
|        - |  7622 |  |
|        - |  7623 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7624 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7625 | `};` |
|        - |  7626 | `/*` |
|        - |  7627 | ` * Binary to hex consumer callback.` |
|        - |  7628 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7629 | ` * defined below.` |
|        - |  7630 | ` */` |
|      192 |  7631 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7632 |  |
|      193 |  7633 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7634 | `	sxu32 nBuflen;` |
|        - |  7635 | `	/* Extract result buffer length */` |
|      193 |  7636 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7637 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7638 | `			/*` |
|        - |  7639 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7640 | `			 * string will be 13 characters long` |
|        - |  7641 | `			 */` |
|       25 |  7642 | `		return SXERR_ABORT;` |
|        - |  7643 | `	}` |
|      169 |  7644 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7645 | `		return SXERR_ABORT;` |
|        - |  7646 | `	}` |
|        - |  7647 | `	/* Safely Consume the hex stream */` |
|      169 |  7648 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7649 | `	return SXRET_OK;` |
|       97 |  7650 |  |
|        - |  7651 | `/*` |
|        - |  7652 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7653 | ` *  Generate a unique ID` |
|        - |  7654 | ` * Parameter` |
|        - |  7655 | ` * $prefix` |
|        - |  7656 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7657 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7658 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7659 | ` * $more_entropy` |
|        - |  7660 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7661 | ` *  that the result will be unique.` |
|        - |  7662 | ` * Return` |
|        - |  7663 | ` *  Returns the unique identifier, as a string.` |
|        - |  7664 | ` */` |
|       24 |  7665 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7666 |  |
|        - |  7667 | `	struct unique_id_data sUniq;` |
|        - |  7668 | `	unsigned char zDigest[20];` |
|       25 |  7669 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7670 | `	const char *zPrefix;` |
|        - |  7671 | `	SHA1Context sCtx;` |
|        - |  7672 | `	char zRandom[7];` |
|        - |  7673 | `	int nPrefix;` |
|        - |  7674 | `	int entropy;` |
|        - |  7675 | `	/* Generate a random string first */` |
|       25 |  7676 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7677 | `	/* Initialize fields */` |
|       25 |  7678 | `	zPrefix = 0;` |
|       25 |  7679 | `	nPrefix = 0;` |
|       25 |  7680 | `	entropy = 0;` |
|       25 |  7681 | `	if( nArg > 0 ){` |
|        - |  7682 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7683 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7684 | `		if( nArg > 1 ){` |
|      ! 0 |  7685 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7686 | `		}` |
|      ! 0 |  7687 | `	}` |
|       25 |  7688 | `	SHA1Init(&sCtx);` |
|        - |  7689 | `	/* Generate the random ID */` |
|       25 |  7690 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7691 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7692 | `	}` |
|        - |  7693 | `	/* Append the random ID */` |
|       25 |  7694 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7695 | `	/* Append the random string */` |
|       25 |  7696 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7697 | `	/* Increment the number */` |
|       25 |  7698 | `	pVm->unique_id++;` |
|       25 |  7699 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7700 | `	/* Hexify the digest */` |
|       25 |  7701 | `	sUniq.pCtx = pCtx;` |
|       25 |  7702 | `	sUniq.entropy = entropy;` |
|       25 |  7703 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7704 | `	/* All done */` |
|       25 |  7705 | `	return PH7_OK;` |
|        1 |  7706 |  |
|        - |  7707 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7708 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7709 | `/*` |
|        - |  7710 | ` * Section:` |
|        - |  7711 | ` *  Language construct implementation as foreign functions.` |
|        - |  7712 | ` * Status:` |
|        - |  7713 | ` *    Stable.` |
|        - |  7714 | ` */` |
|        - |  7715 | `/*` |
|        - |  7716 | ` * void echo($string...)` |
|        - |  7717 | ` *  Output one or more messages.` |
|        - |  7718 | ` * Parameters` |
|        - |  7719 | ` *  $string` |
|        - |  7720 | ` *   Message to output.` |
|        - |  7721 | ` * Return` |
|        - |  7722 | ` *  NULL.` |
|        - |  7723 | ` */` |
|      ! 0 |  7724 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7725 |  |
|        - |  7726 | `	const char *zData;` |
|      ! 0 |  7727 | `	int nDataLen = 0;` |
|        - |  7728 | `	ph7_vm *pVm;` |
|        - |  7729 | `	int i,rc;` |
|        - |  7730 | `	/* Point to the target VM */` |
|      ! 0 |  7731 | `	pVm = pCtx->pVm;` |
|        - |  7732 | `	/* Output */` |
|      ! 0 |  7733 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7734 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7735 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7736 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7737 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7738 | `				/* Increment output length */` |
|      ! 0 |  7739 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7740 | `			}` |
|      ! 0 |  7741 | `			if( rc == SXERR_ABORT ){` |
|        - |  7742 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7743 | `				return PH7_ABORT;` |
|        - |  7744 | `			}` |
|      ! 0 |  7745 | `		}` |
|      ! 0 |  7746 | `	}` |
|      ! 0 |  7747 | `	return SXRET_OK;` |
|      ! 0 |  7748 |  |
|        - |  7749 | `/*` |
|        - |  7750 | ` * int print($string...)` |
|        - |  7751 | ` *  Output one or more messages.` |
|        - |  7752 | ` * Parameters` |
|        - |  7753 | ` *  $string` |
|        - |  7754 | ` *   Message to output.` |
|        - |  7755 | ` * Return` |
|        - |  7756 | ` *  1 always.` |
|        - |  7757 | ` */` |
|        2 |  7758 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7759 |  |
|        - |  7760 | `	const char *zData;` |
|        3 |  7761 | `	int nDataLen = 0;` |
|        - |  7762 | `	ph7_vm *pVm;` |
|        - |  7763 | `	int i,rc;` |
|        - |  7764 | `	/* Point to the target VM */` |
|        3 |  7765 | `	pVm = pCtx->pVm;` |
|        - |  7766 | `	/* Output */` |
|        5 |  7767 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7768 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7769 | `		if( nDataLen > 0 ){` |
|        3 |  7770 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7771 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7772 | `				/* Increment output length */` |
|        3 |  7773 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7774 | `			}` |
|        3 |  7775 | `			if( rc == SXERR_ABORT ){` |
|        - |  7776 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7777 | `				return PH7_ABORT;` |
|        - |  7778 | `			}` |
|        1 |  7779 | `		}` |
|        2 |  7780 | `	}` |
|        - |  7781 | `	/* Return 1 */` |
|        3 |  7782 | `	ph7_result_int(pCtx,1);` |
|        3 |  7783 | `	return SXRET_OK;` |
|        2 |  7784 |  |
|        - |  7785 | `/*` |
|        - |  7786 | ` * void exit(string $msg)` |
|        - |  7787 | ` * void exit(int $status)` |
|        - |  7788 | ` * void die(string $ms)` |
|        - |  7789 | ` * void die(int $status)` |
|        - |  7790 | ` *   Output a message and terminate program execution.` |
|        - |  7791 | ` * Parameter` |
|        - |  7792 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7793 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7794 | ` *  and not printed` |
|        - |  7795 | ` * Return` |
|        - |  7796 | ` *  NULL` |
|        - |  7797 | ` */` |
|      ! 0 |  7798 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7799 |  |
|      ! 0 |  7800 | `	if( nArg > 0 ){` |
|      ! 0 |  7801 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7802 | `			const char *zData;` |
|      ! 0 |  7803 | `			int iLen = 0;` |
|        - |  7804 | `			/* Print exit message */` |
|      ! 0 |  7805 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7806 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7807 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7808 | `			sxi32 iExitStatus;` |
|        - |  7809 | `			/* Record exit status code */` |
|      ! 0 |  7810 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7811 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7812 | `		}` |
|      ! 0 |  7813 | `	}` |
|        - |  7814 | `	/* Check if we are in an included file */` |
|      ! 0 |  7815 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7816 | `		/* Exit the entire process */` |
|      ! 0 |  7817 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7818 | `	}` |
|        - |  7819 | `	/* Abort processing immediately */` |
|      ! 0 |  7820 | `	return PH7_ABORT;` |
|      ! 0 |  7821 |  |
|        - |  7822 | `/*` |
|        - |  7823 | ` * bool isset($var,...)` |
|        - |  7824 | ` *  Finds out whether a variable is set.` |
|        - |  7825 | ` * Parameters` |
|        - |  7826 | ` *  One or more variable to check.` |
|        - |  7827 | ` * Return` |
|        - |  7828 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7829 | ` */` |
|    71378 |  7830 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7831 |  |
|        - |  7832 | `	ph7_value *pObj;` |
|    71380 |  7833 | `	int res = 0;` |
|        - |  7834 | `	int i;` |
|    71380 |  7835 | `	if( nArg < 1 ){` |
|        - |  7836 | `		/* Missing arguments,return false */` |
|      ! 0 |  7837 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7838 | `		return SXRET_OK;` |
|        - |  7839 | `	}` |
|        - |  7840 | `	/* Iterate over available arguments */` |
|    94232 |  7841 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    71380 |  7842 | `		pObj = apArg[i];` |
|    71380 |  7843 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    48020 |  7844 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7845 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7846 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7847 | `			}` |
|    24009 |  7848 | `		}` |
|    71380 |  7849 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    71380 |  7850 | `		if( !res ){` |
|        - |  7851 | `			/* Variable not set,return FALSE */` |
|    48528 |  7852 | `			ph7_result_bool(pCtx,0);` |
|    48528 |  7853 | `			return SXRET_OK;` |
|        - |  7854 | `		}` |
|    11428 |  7855 | `	}` |
|        - |  7856 | `	/* All given variable are set,return TRUE */` |
|    22854 |  7857 | `	ph7_result_bool(pCtx,1);` |
|    22854 |  7858 | `	return SXRET_OK;` |
|    35691 |  7859 |  |
|        - |  7860 | `/*` |
|        - |  7861 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7862 | ` * frame,the reference table and discard it's contents.` |
|        - |  7863 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7864 | ` */` |
|  2964008 |  7865 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7866 |  |
|        - |  7867 | `	ph7_value *pObj;` |
|        - |  7868 | `	VmRefObj *pRef;` |
|  2964010 |  7869 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2964010 |  7870 | `	if( pObj ){` |
|        - |  7871 | `		/* Release the object */` |
|  2964010 |  7872 | `		PH7_MemObjRelease(pObj);` |
|  1482004 |  7873 | `	}` |
|        - |  7874 | `	/* Remove old reference links */` |
|  2964010 |  7875 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2964010 |  7876 | `	if( pRef ){` |
|  2963990 |  7877 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7878 | `		/* Unlink from the reference table */` |
|  2963990 |  7879 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2963990 |  7880 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7881 | `			VmSlot sFree;` |
|        - |  7882 | `			/* Restore to the free list */` |
|  2963984 |  7883 | `			sFree.nIdx = nObjIdx;` |
|  2963984 |  7884 | `			sFree.pUserData = 0;` |
|  2963984 |  7885 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1481991 |  7886 | `		}` |
|  1481994 |  7887 | `	}` |
|  2964010 |  7888 | `	return SXRET_OK;` |
|        2 |  7889 |  |
|        - |  7890 | `/*` |
|        - |  7891 | ` * void unset($var,...)` |
|        - |  7892 | ` *   Unset one or more given variable.` |
|        - |  7893 | ` * Parameters` |
|        - |  7894 | ` *  One or more variable to unset.` |
|        - |  7895 | ` * Return` |
|        - |  7896 | ` *  Nothing.` |
|        - |  7897 | ` */` |
|     3260 |  7898 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7899 |  |
|        - |  7900 | `	ph7_value *pObj;` |
|        - |  7901 | `	ph7_vm *pVm;` |
|        - |  7902 | `	int i;` |
|        - |  7903 | `	/* Point to the target VM */` |
|     3262 |  7904 | `	pVm = pCtx->pVm;` |
|        - |  7905 | `	/* Iterate and unset */` |
|     9666 |  7906 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6406 |  7907 | `		pObj = apArg[i];` |
|     6406 |  7908 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7909 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7910 | `				/* Throw an error */` |
|      ! 0 |  7911 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7912 | `			}` |
|      435 |  7913 | `		}else{` |
|     5540 |  7914 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7915 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5540 |  7916 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5534 |  7917 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2766 |  7918 | `			}` |
|        - |  7919 | `		}` |
|     3204 |  7920 | `	}` |
|     3262 |  7921 | `	return SXRET_OK;` |
|        2 |  7922 |  |
|        - |  7923 | `/*` |
|        - |  7924 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7925 | ` */` |
|      110 |  7926 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7927 |  |
|      111 |  7928 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7929 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7930 | `	ph7_value *pObj;` |
|        - |  7931 | `	sxu32 nIdx;` |
|        - |  7932 | `	/* Extract the memory object */` |
|      111 |  7933 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7934 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7935 | `	if( pObj ){` |
|      111 |  7936 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7937 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7938 | `				SyString sName;` |
|        - |  7939 | `				ph7_value sKey;` |
|        - |  7940 | `				/* Perform the insertion */` |
|      109 |  7941 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7942 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7943 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7944 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7945 | `			}` |
|       54 |  7946 | `		}` |
|       55 |  7947 | `	}` |
|      111 |  7948 | `	return SXRET_OK;` |
|        1 |  7949 |  |
|        - |  7950 | `/*` |
|        - |  7951 | ` * array get_defined_vars(void)` |
|        - |  7952 | ` *  Returns an array of all defined variables.` |
|        - |  7953 | ` * Parameter` |
|        - |  7954 | ` *  None` |
|        - |  7955 | ` * Return` |
|        - |  7956 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7957 | ` */` |
|        2 |  7958 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7959 |  |
|        3 |  7960 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7961 | `	ph7_value *pArray;` |
|        - |  7962 | `	/* Create a new array */` |
|        3 |  7963 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7964 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7965 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7966 | `		SXUNUSED(apArg);` |
|        - |  7967 | `		/* Return NULL */` |
|      ! 0 |  7968 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7969 | `		return SXRET_OK;` |
|        - |  7970 | `	}` |
|        - |  7971 | `	/* Superglobals first */` |
|        3 |  7972 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7973 | `	/* Then variable defined in the current frame */` |
|        3 |  7974 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7975 | `	/* Finally,return the created array */` |
|        3 |  7976 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7977 | `	return SXRET_OK;` |
|        2 |  7978 |  |
|        - |  7979 | `/*` |
|        - |  7980 | ` * bool gettype($var)` |
|        - |  7981 | ` *  Get the type of a variable` |
|        - |  7982 | ` * Parameters` |
|        - |  7983 | ` *   $var` |
|        - |  7984 | ` *    The variable being type checked.` |
|        - |  7985 | ` * Return` |
|        - |  7986 | ` *   String representation of the given variable type.` |
|        - |  7987 | ` */` |
|       32 |  7988 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7989 |  |
|       34 |  7990 | `	const char *zType = "Empty";` |
|       34 |  7991 | `	if( nArg > 0 ){` |
|       34 |  7992 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7993 | `	}` |
|        - |  7994 | `	/* Return the variable type */` |
|       34 |  7995 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7996 | `	return SXRET_OK;` |
|        2 |  7997 |  |
|        - |  7998 | `/*` |
|        - |  7999 | ` * string get_resource_type(resource $handle)` |
|        - |  8000 | ` *  This function gets the type of the given resource.` |
|        - |  8001 | ` * Parameters` |
|        - |  8002 | ` *  $handle` |
|        - |  8003 | ` *  The evaluated resource handle.` |
|        - |  8004 | ` * Return` |
|        - |  8005 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  8006 | ` *  representing its type. If the type is not identified by this function` |
|        - |  8007 | ` *  the return value will be the string Unknown.` |
|        - |  8008 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  8009 | ` *  is not a resource.` |
|        - |  8010 | ` */` |
|        2 |  8011 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8012 |  |
|        3 |  8013 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  8014 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  8015 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8016 | `		return PH7_OK;` |
|        - |  8017 | `	}` |
|        3 |  8018 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  8019 | `	return SXRET_OK;` |
|        2 |  8020 |  |
|        - |  8021 | `/*` |
|        - |  8022 | ` * void var_dump(expression,....)` |
|        - |  8023 | ` *   var_dump � Dumps information about a variable` |
|        - |  8024 | ` * Parameters` |
|        - |  8025 | ` *   One or more expression to dump.` |
|        - |  8026 | ` * Returns` |
|        - |  8027 | ` *  Nothing.` |
|        - |  8028 | ` */` |
|      218 |  8029 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8030 |  |
|        - |  8031 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8032 | `	int i;` |
|      220 |  8033 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8034 | `	/* Dump one or more expressions */` |
|      444 |  8035 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  8036 | `		ph7_value *pObj = apArg[i];` |
|        - |  8037 | `		/* Reset the working buffer */` |
|      226 |  8038 | `		SyBlobReset(&sDump);` |
|        - |  8039 | `		/* Dump the given expression */` |
|      226 |  8040 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8041 | `		/* Output */` |
|      226 |  8042 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  8043 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  8044 | `		}` |
|      114 |  8045 | `	}` |
|        - |  8046 | `	/* Release the working buffer */` |
|      220 |  8047 | `	SyBlobRelease(&sDump);` |
|      220 |  8048 | `	return SXRET_OK;` |
|        2 |  8049 |  |
|        - |  8050 | `/*` |
|        - |  8051 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8052 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8053 | ` * Parameters` |
|        - |  8054 | ` *   expression: Expression to dump` |
|        - |  8055 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8056 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8057 | ` *            print_r() will return the information rather than print it.` |
|        - |  8058 | ` * Return` |
|        - |  8059 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8060 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8061 | ` */` |
|       16 |  8062 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8063 |  |
|       17 |  8064 | `	int ret_string = 0;` |
|        - |  8065 | `	SyBlob sDump;` |
|       17 |  8066 | `	if( nArg < 1 ){` |
|        - |  8067 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8068 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8069 | `		return SXRET_OK;` |
|        - |  8070 | `	}` |
|       17 |  8071 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8072 | `	if ( nArg > 1 ){` |
|        - |  8073 | `		/* Where to redirect output */` |
|       11 |  8074 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8075 | `	}` |
|        - |  8076 | `	/* Generate dump */` |
|       17 |  8077 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8078 | `	if( !ret_string ){` |
|        - |  8079 | `		/* Output dump */` |
|        7 |  8080 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8081 | `		/* Return true */` |
|        7 |  8082 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8083 | `	}else{` |
|        - |  8084 | `		/* Generated dump as return value */` |
|       11 |  8085 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8086 | `	}` |
|        - |  8087 | `	/* Release the working buffer */` |
|       17 |  8088 | `	SyBlobRelease(&sDump);` |
|       17 |  8089 | `	return SXRET_OK;` |
|        9 |  8090 |  |
|        - |  8091 | `/*` |
|        - |  8092 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8093 | ` * Same job as print_r. (see coment above)` |
|        - |  8094 | ` */` |
|        2 |  8095 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8096 |  |
|        3 |  8097 | `	int ret_string = 0;` |
|        - |  8098 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8099 | `	if( nArg < 1 ){` |
|        - |  8100 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8101 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8102 | `		return SXRET_OK;` |
|        - |  8103 | `	}` |
|        3 |  8104 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8105 | `	if ( nArg > 1 ){` |
|        - |  8106 | `		/* Where to redirect output */` |
|        3 |  8107 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8108 | `	}` |
|        - |  8109 | `	/* Generate dump */` |
|        3 |  8110 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8111 | `	if( !ret_string ){` |
|        - |  8112 | `		/* Output dump */` |
|      ! 0 |  8113 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8114 | `		/* Return NULL */` |
|      ! 0 |  8115 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8116 | `	}else{` |
|        - |  8117 | `		/* Generated dump as return value */` |
|        3 |  8118 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8119 | `	}` |
|        - |  8120 | `	/* Release the working buffer */` |
|        3 |  8121 | `	SyBlobRelease(&sDump);` |
|        3 |  8122 | `	return SXRET_OK;` |
|        2 |  8123 |  |
|        - |  8124 | `/*` |
|        - |  8125 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8126 | ` *  Set/get the various assert flags.` |
|        - |  8127 | ` * Parameter` |
|        - |  8128 | ` * $what` |
|        - |  8129 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8130 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  8131 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8132 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  8133 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8134 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  8135 | ` * $value` |
|        - |  8136 | ` *   An optional new value for the option.` |
|        - |  8137 | ` * Return` |
|        - |  8138 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8139 | ` */` |
|       30 |  8140 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8141 |  |
|       32 |  8142 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8143 | `	int iOption;` |
|        - |  8144 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  8145 | `	if( nArg < 1 ){` |
|        3 |  8146 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8147 | `			"ArgumentCountError",` |
|        - |  8148 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  8149 | `			);` |
|        - |  8150 | `	}` |
|        - |  8151 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8152 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8153 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8154 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8155 | `			"TypeError",` |
|        - |  8156 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8157 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8158 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8159 | `			);` |
|        - |  8160 | `	}` |
|       30 |  8161 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8162 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8163 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8164 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8165 | `	switch( iOption ){` |
|        6 |  8166 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8167 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8168 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8169 | `		if( nArg > 1 ){` |
|        5 |  8170 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8171 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8172 | `			}else{` |
|        3 |  8173 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8174 | `			}` |
|        2 |  8175 | `		}` |
|       14 |  8176 | `		break;` |
|        1 |  8177 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8178 | `		/* Return old callback or null */` |
|        3 |  8179 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8180 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8181 | `		}else{` |
|        3 |  8182 | `			ph7_result_null(pCtx);` |
|        - |  8183 | `		}` |
|        3 |  8184 | `		if( nArg > 1 ){` |
|      ! 0 |  8185 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8186 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8187 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8188 | `			}else{` |
|      ! 0 |  8189 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8190 | `			}` |
|      ! 0 |  8191 | `		}` |
|        3 |  8192 | `		break;` |
|        5 |  8193 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8194 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8195 | `		if( nArg > 1 ){` |
|        5 |  8196 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8197 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8198 | `			}else{` |
|        3 |  8199 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8200 | `			}` |
|        2 |  8201 | `		}` |
|       11 |  8202 | `		break;` |
|      ! 0 |  8203 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8204 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8205 | `		break;` |
|        1 |  8206 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8207 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8208 | `		break;` |
|      ! 0 |  8209 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8210 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8211 | `		break;` |
|        1 |  8212 | `	default:` |
|        - |  8213 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8214 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8215 | `			"ValueError",` |
|        - |  8216 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8217 | `			);` |
|        - |  8218 | `	}` |
|       28 |  8219 | `	return PH7_OK;` |
|       17 |  8220 |  |
|        - |  8221 | `/*` |
|        - |  8222 | ` * bool assert(mixed $assertion)` |
|        - |  8223 | ` *  Checks if assertion is FALSE.` |
|        - |  8224 | ` * Parameter` |
|        - |  8225 | ` *  $assertion` |
|        - |  8226 | ` *    The assertion to test.` |
|        - |  8227 | ` * Return` |
|        - |  8228 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8229 | ` */` |
|       26 |  8230 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8231 |  |
|       28 |  8232 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8233 | `	int iFlags,iResult;` |
|        - |  8234 | `	const char *zDesc;` |
|        - |  8235 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8236 | `	if( nArg < 1 ){` |
|        3 |  8237 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8238 | `			"ArgumentCountError",` |
|        - |  8239 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8240 | `			);` |
|        - |  8241 | `	}` |
|       26 |  8242 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8243 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8244 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8245 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8246 | `		return PH7_OK;` |
|        - |  8247 | `	}` |
|        - |  8248 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8249 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8250 | `	if( !iResult ){` |
|        - |  8251 | `		/* Assertion failed */` |
|        - |  8252 | `		/* Extract optional description */` |
|       13 |  8253 | `		zDesc = 0;` |
|       13 |  8254 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8255 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8256 | `		}` |
|       13 |  8257 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8258 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8259 | `			ph7_value sFile,sLine;` |
|        - |  8260 | `			ph7_value *apCbArg[3];` |
|        - |  8261 | `			SyString *pFile;` |
|        - |  8262 | `			/* Extract the processed script */` |
|      ! 0 |  8263 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8264 | `			if( pFile == 0 ){` |
|      ! 0 |  8265 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8266 | `			}` |
|        - |  8267 | `			/* Invoke the callback */` |
|      ! 0 |  8268 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8269 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8270 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8271 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8272 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8273 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8274 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8275 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8276 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8277 | `		}` |
|       13 |  8278 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8279 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8280 | `			return PH7_ABORT;` |
|        - |  8281 | `		}` |
|        - |  8282 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8283 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8284 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8285 | `				"AssertionError",` |
|        - |  8286 | `				"%s",` |
|        1 |  8287 | `				zDesc` |
|        - |  8288 | `				);` |
|      ! 0 |  8289 | `		}else{` |
|       11 |  8290 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8291 | `				"AssertionError",` |
|        - |  8292 | `				"assert(false)"` |
|        - |  8293 | `				);` |
|        - |  8294 | `		}` |
|        - |  8295 | `	}` |
|        - |  8296 | `	/* Assertion passed */` |
|       14 |  8297 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8298 | `	return PH7_OK;` |
|       15 |  8299 |  |
|        - |  8300 | `/*` |
|        - |  8301 | ` * Section:` |
|        - |  8302 | ` *  Error reporting functions.` |
|        - |  8303 | ` * Status:` |
|        - |  8304 | ` *    Stable.` |
|        - |  8305 | ` */` |
|        - |  8306 | `/*` |
|        - |  8307 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8308 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8309 | ` * Parameters` |
|        - |  8310 | ` *  $error_msg` |
|        - |  8311 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8312 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8313 | ` * $error_type` |
|        - |  8314 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8315 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8316 | ` * Return` |
|        - |  8317 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8318 | ` */` |
|       12 |  8319 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8320 |  |
|       14 |  8321 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8322 | `	int rc = PH7_OK;` |
|       14 |  8323 | `	if( nArg > 0 ){` |
|        - |  8324 | `		const char *zErr;` |
|        - |  8325 | `		int nLen;` |
|        - |  8326 | `		/* Extract the error message */` |
|       12 |  8327 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8328 | `		if( nArg > 1 ){` |
|        - |  8329 | `			/* Extract the error type */` |
|       12 |  8330 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8331 | `			switch( nErr ){` |
|        1 |  8332 | `			case 1:   /* E_ERROR */` |
|        - |  8333 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8334 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8335 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8336 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8337 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8338 | `				break;` |
|        1 |  8339 | `			case 2:   /* E_WARNING */` |
|        - |  8340 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8341 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8342 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8343 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8344 | `				break;` |
|        3 |  8345 | `			default:` |
|        8 |  8346 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8347 | `				break;` |
|        - |  8348 | `			}` |
|        5 |  8349 | `		}` |
|        - |  8350 | `		/* Report error */` |
|       12 |  8351 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8352 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8353 | `			return rc;` |
|        - |  8354 | `		}` |
|        - |  8355 | `		/* Return true */` |
|       12 |  8356 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8357 | `	}else{` |
|        - |  8358 | `		/* Missing arguments,return FALSE */` |
|        3 |  8359 | `		ph7_result_bool(pCtx,0);` |
|        - |  8360 | `	}` |
|       14 |  8361 | `	return rc;` |
|        8 |  8362 |  |
|        - |  8363 | `/*` |
|        - |  8364 | ` * int error_reporting([int $level])` |
|        - |  8365 | ` *  Sets which PHP errors are reported.` |
|        - |  8366 | ` * Parameters` |
|        - |  8367 | ` *  $level` |
|        - |  8368 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8369 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8370 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8371 | ` *   levels will not always behave as expected.` |
|        - |  8372 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8373 | ` *   in the predefined constants.` |
|        - |  8374 | ` * Return` |
|        - |  8375 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8376 | ` *   parameter is given.` |
|        - |  8377 | ` */` |
|       40 |  8378 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8379 |  |
|       42 |  8380 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8381 | `	int nOld;` |
|        - |  8382 | `	/* Extract the old reporting level */` |
|       42 |  8383 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8384 | `	if( nArg > 0 ){` |
|        - |  8385 | `		int nNew;` |
|        - |  8386 | `		/* Extract the desired error reporting level */` |
|       34 |  8387 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8388 | `		if( !nNew ){` |
|        - |  8389 | `			/* Do not report errors at all */` |
|        5 |  8390 | `			pVm->bErrReport = 0;` |
|        3 |  8391 | `		}else{` |
|        - |  8392 | `			/* Report all errors */` |
|       30 |  8393 | `			pVm->bErrReport = 1;` |
|        - |  8394 | `		}` |
|       16 |  8395 | `	}` |
|        - |  8396 | `	/* Return the old level */` |
|       42 |  8397 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8398 | `	return PH7_OK;` |
|        2 |  8399 |  |
|        - |  8400 | `/*` |
|        - |  8401 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8402 | ` *  Send an error message somewhere.` |
|        - |  8403 | ` * Parameter` |
|        - |  8404 | ` *  $message` |
|        - |  8405 | ` *   The error message that should be logged.` |
|        - |  8406 | ` *  $message_type` |
|        - |  8407 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8408 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8409 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8410 | ` *       This is the default option.` |
|        - |  8411 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8412 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8413 | ` *    2  No longer an option.` |
|        - |  8414 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8415 | ` *       to the end of the message string.` |
|        - |  8416 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8417 | ` *  $destination` |
|        - |  8418 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8419 | ` *  $extra_headers` |
|        - |  8420 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8421 | ` * Return` |
|        - |  8422 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8423 | ` * NOTE:` |
|        - |  8424 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8425 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8426 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8427 | ` *  Otherwise this function is no-op.` |
|        - |  8428 | ` */` |
|        4 |  8429 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8430 |  |
|        - |  8431 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8432 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8433 | `	int iType = 0;` |
|        5 |  8434 | `	if( nArg < 1 ){` |
|        - |  8435 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8436 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8437 | `		return PH7_OK;` |
|        - |  8438 | `	}` |
|        5 |  8439 | `	if( pVm->xErrLog  ){` |
|        - |  8440 | `		/* Invoke the user callback */` |
|      ! 0 |  8441 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8442 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8443 | `		if( nArg > 1 ){` |
|      ! 0 |  8444 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8445 | `			if( nArg > 2 ){` |
|      ! 0 |  8446 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8447 | `				if( nArg > 3 ){` |
|      ! 0 |  8448 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8449 | `				}` |
|      ! 0 |  8450 | `			}` |
|      ! 0 |  8451 | `		}` |
|      ! 0 |  8452 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8453 | `	}` |
|        - |  8454 | `	/* Retun TRUE */` |
|        5 |  8455 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8456 | `	return PH7_OK;` |
|        3 |  8457 |  |
|        - |  8458 | `/*` |
|        - |  8459 | ` * bool restore_exception_handler(void)` |
|        - |  8460 | ` *  Restores the previously defined exception handler function.` |
|        - |  8461 | ` * Parameter` |
|        - |  8462 | ` *  None` |
|        - |  8463 | ` * Return` |
|        - |  8464 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8465 | ` */` |
|        4 |  8466 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8467 |  |
|        5 |  8468 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8469 | `	ph7_value *pOld,*pNew;` |
|        - |  8470 | `	/* Point to the old and the new handler */` |
|        5 |  8471 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8472 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8473 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8474 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8475 | `		SXUNUSED(apArg);` |
|        - |  8476 | `		/* No installed handler,return FALSE */` |
|        5 |  8477 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8478 | `		return PH7_OK;` |
|        - |  8479 | `	}` |
|        - |  8480 | `	/* Copy the old handler */` |
|      ! 0 |  8481 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8482 | `	PH7_MemObjRelease(pOld);` |
|        - |  8483 | `	/* Return TRUE */` |
|      ! 0 |  8484 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8485 | `	return PH7_OK;` |
|        3 |  8486 |  |
|        - |  8487 | `/*` |
|        - |  8488 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8489 | ` *  Sets a user-defined exception handler function.` |
|        - |  8490 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8491 | ` * NOTE` |
|        - |  8492 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8493 | ` *  the satndard PHP engine.` |
|        - |  8494 | ` * Parameters` |
|        - |  8495 | ` *  $exception_handler` |
|        - |  8496 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8497 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8498 | ` *   that was thrown.` |
|        - |  8499 | ` *  Note:` |
|        - |  8500 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8501 | ` * Return` |
|        - |  8502 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8503 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8504 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8505 | ` */` |
|        4 |  8506 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8507 |  |
|        6 |  8508 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8509 | `	ph7_value *pOld,*pNew;` |
|        - |  8510 | `	/* Point to the old and the new handler */` |
|        6 |  8511 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8512 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8513 | `	/* Return the old handler */` |
|        6 |  8514 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8515 | `	if( nArg > 0 ){` |
|        6 |  8516 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8517 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8518 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8519 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8520 | `		}else{` |
|        6 |  8521 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8522 | `			/* Install the new handler */` |
|        6 |  8523 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8524 | `		}` |
|        2 |  8525 | `	}` |
|        6 |  8526 | `	return PH7_OK;` |
|        2 |  8527 |  |
|        - |  8528 | `/*` |
|        - |  8529 | ` * bool restore_error_handler(void)` |
|        - |  8530 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8531 | ` * Parameters:` |
|        - |  8532 | ` *  None.` |
|        - |  8533 | ` * Return` |
|        - |  8534 | ` *  Always TRUE.` |
|        - |  8535 | ` */` |
|        4 |  8536 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8537 |  |
|        5 |  8538 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8539 | `	ph7_value *pOld,*pNew;` |
|        - |  8540 | `	/* Point to the old and the new handler */` |
|        5 |  8541 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8542 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8543 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8544 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8545 | `		SXUNUSED(apArg);` |
|        - |  8546 | `		/* No installed callback,return FALSE */` |
|        5 |  8547 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8548 | `		return PH7_OK;` |
|        - |  8549 | `	}` |
|        - |  8550 | `	/* Copy the old callback */` |
|      ! 0 |  8551 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8552 | `	PH7_MemObjRelease(pOld);` |
|        - |  8553 | `	/* Return TRUE */` |
|      ! 0 |  8554 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8555 | `	return PH7_OK;` |
|        3 |  8556 |  |
|        - |  8557 | `/*` |
|        - |  8558 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8559 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8560 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8561 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8562 | ` *  Sets a user-defined error handler function.` |
|        - |  8563 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8564 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8565 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8566 | ` *  conditions (using trigger_error()).` |
|        - |  8567 | ` * Parameters` |
|        - |  8568 | ` *  $error_handler` |
|        - |  8569 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8570 | ` *   describing the error.` |
|        - |  8571 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8572 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8573 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8574 | ` *   The function can be shown as:` |
|        - |  8575 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8576 | ` *     errno` |
|        - |  8577 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8578 | ` *   errstr` |
|        - |  8579 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8580 | ` *   errfile` |
|        - |  8581 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8582 | ` *     was raised in, as a string.` |
|        - |  8583 | ` *  Note:` |
|        - |  8584 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8585 | ` * Return` |
|        - |  8586 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8587 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8588 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8589 | ` */` |
|     8722 |  8590 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8591 |  |
|     8724 |  8592 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8593 | `	ph7_value *pOld,*pNew;` |
|        - |  8594 | `	/* Point to the old and the new handler */` |
|     8724 |  8595 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8596 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8597 | `	/* Return the old handler */` |
|     8724 |  8598 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8599 | `	if( nArg > 0 ){` |
|     8724 |  8600 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8601 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8602 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8603 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8604 | `		}else{` |
|     4364 |  8605 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8606 | `			/* Install the new handler */` |
|     4364 |  8607 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8608 | `		}` |
|     4361 |  8609 | `	}` |
|     8724 |  8610 | `	return PH7_OK;` |
|        2 |  8611 |  |
|        - |  8612 | `/*` |
|        - |  8613 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8614 | ` *  Generates a backtrace.` |
|        - |  8615 | ` * Paramaeter` |
|        - |  8616 | ` *  $options` |
|        - |  8617 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8618 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8619 | ` *   all the function/method arguments, to save memory.` |
|        - |  8620 | ` * $limit` |
|        - |  8621 | ` *   (Not Used)` |
|        - |  8622 | ` * Return` |
|        - |  8623 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8624 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8625 | ` *          Name        Type      Description` |
|        - |  8626 | ` *          ------      ------     -----------` |
|        - |  8627 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8628 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8629 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8630 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8631 | ` *          object      object    The current object.` |
|        - |  8632 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8633 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8634 | ` */` |
|      504 |  8635 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8636 |  |
|      506 |  8637 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8638 | `	ph7_value *pArray;` |
|        - |  8639 | `	ph7_class *pClass;` |
|        - |  8640 | `	ph7_value *pValue;` |
|        - |  8641 | `	SyString *pFile;` |
|        - |  8642 | `	/* Create a new array */` |
|      506 |  8643 | `	pArray = ph7_context_new_array(pCtx);` |
|      506 |  8644 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      506 |  8645 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8646 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8647 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8648 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8649 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8650 | `		SXUNUSED(apArg);` |
|      ! 0 |  8651 | `		return PH7_OK;` |
|        - |  8652 | `	}` |
|        - |  8653 | `	/* Dump running function name and it's arguments  */` |
|      506 |  8654 | `	if( pVm->pFrame->pParent ){` |
|      506 |  8655 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8656 | `		ph7_vm_func *pFunc;` |
|        - |  8657 | `		ph7_value *pArg;` |
|      506 |  8658 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8659 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8660 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8661 | `		}` |
|      506 |  8662 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      506 |  8663 | `		if( pFrame->pParent && pFunc ){` |
|      506 |  8664 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      506 |  8665 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      506 |  8666 | `			ph7_value_reset_string_cursor(pValue);` |
|      252 |  8667 | `		}` |
|        - |  8668 | `		/* Function arguments */` |
|      506 |  8669 | `		pArg = ph7_context_new_array(pCtx);` |
|      506 |  8670 | `		if( pArg  ){` |
|        - |  8671 | `			ph7_value *pObj;` |
|        - |  8672 | `			VmSlot *aSlot;` |
|        - |  8673 | `			sxu32 n;` |
|        - |  8674 | `			/* Start filling the array with the given arguments */` |
|      506 |  8675 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2010 |  8676 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1506 |  8677 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1506 |  8678 | `				if( pObj ){` |
|     1506 |  8679 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      752 |  8680 | `				}` |
|      754 |  8681 | `			}` |
|        - |  8682 | `			/* Save the array */` |
|      506 |  8683 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      252 |  8684 | `		}` |
|      252 |  8685 | `	}` |
|      506 |  8686 | `	ph7_value_int(pValue,1);` |
|        - |  8687 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8688 | `	 * line numbers at run-time. )` |
|        - |  8689 | `	 */` |
|      506 |  8690 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8691 | `	/* Current processed script */` |
|      506 |  8692 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      506 |  8693 | `	if( pFile ){` |
|      506 |  8694 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      506 |  8695 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      506 |  8696 | `		ph7_value_reset_string_cursor(pValue);` |
|      252 |  8697 | `	}` |
|        - |  8698 | `	/* Top class */` |
|      506 |  8699 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      506 |  8700 | `	if( pClass ){` |
|      502 |  8701 | `		ph7_value_reset_string_cursor(pValue);` |
|      502 |  8702 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      502 |  8703 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      250 |  8704 | `	}` |
|        - |  8705 | `	/* Return the freshly created array */` |
|      506 |  8706 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8707 | `	/*` |
|        - |  8708 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8709 | `	 * as soon we return from this function.` |
|        - |  8710 | `	 */` |
|      506 |  8711 | `	return PH7_OK;` |
|      254 |  8712 |  |
|        - |  8713 | `/*` |
|        - |  8714 | ` * Generate a small backtrace.` |
|        - |  8715 | ` * Store the generated dump in the given BLOB` |
|        - |  8716 | ` */` |
|        4 |  8717 | `static int VmMiniBacktrace(` |
|        - |  8718 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8719 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8720 | `	)` |
|        1 |  8721 |  |
|        5 |  8722 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8723 | `	ph7_vm_func *pFunc;` |
|        - |  8724 | `	ph7_class *pClass;` |
|        - |  8725 | `	SyString *pFile;` |
|        - |  8726 | `	/* Called function */` |
|        5 |  8727 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8728 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8729 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8730 | `	}` |
|        5 |  8731 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8732 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8733 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8734 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8735 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8736 | `	}else{` |
|      ! 0 |  8737 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8738 | `	}` |
|        5 |  8739 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8740 | `	/* Current processed script */` |
|        5 |  8741 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8742 | `	if( pFile ){` |
|        5 |  8743 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8744 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8745 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8746 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8747 | `	}` |
|        - |  8748 | `	/* Top class */` |
|        5 |  8749 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8750 | `	if( pClass ){` |
|      ! 0 |  8751 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8752 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8753 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8754 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8755 | `	}` |
|        5 |  8756 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8757 | `	/* All done */` |
|        5 |  8758 | `	return SXRET_OK;` |
|        1 |  8759 |  |
|        - |  8760 | `/*` |
|        - |  8761 | ` * void debug_print_backtrace()` |
|        - |  8762 | ` *  Prints a backtrace` |
|        - |  8763 | ` * Parameters` |
|        - |  8764 | ` * None` |
|        - |  8765 | ` * Return` |
|        - |  8766 | ` * NULL` |
|        - |  8767 | ` */` |
|        2 |  8768 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8769 |  |
|        3 |  8770 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8771 | `	SyBlob sDump;` |
|        3 |  8772 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8773 | `	/* Generate the backtrace */` |
|        3 |  8774 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8775 | `	/* Output backtrace */` |
|        3 |  8776 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8777 | `	/* All done,cleanup */` |
|        3 |  8778 | `	SyBlobRelease(&sDump);` |
|        1 |  8779 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8780 | `	SXUNUSED(apArg);` |
|        3 |  8781 | `	return PH7_OK;` |
|        1 |  8782 |  |
|        - |  8783 | `/*` |
|        - |  8784 | ` * string debug_string_backtrace()` |
|        - |  8785 | ` *  Generate a backtrace` |
|        - |  8786 | ` * Parameters` |
|        - |  8787 | ` * None` |
|        - |  8788 | ` * Return` |
|        - |  8789 | ` *  A mini backtrace().` |
|        - |  8790 | ` * Note that this is a symisc extension.` |
|        - |  8791 | ` */` |
|        2 |  8792 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8793 |  |
|        3 |  8794 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8795 | `	SyBlob sDump;` |
|        3 |  8796 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8797 | `	/* Generate the backtrace */` |
|        3 |  8798 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8799 | `	/* Return the backtrace */` |
|        3 |  8800 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8801 | `	/* All done,cleanup */` |
|        3 |  8802 | `	SyBlobRelease(&sDump);` |
|        1 |  8803 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8804 | `	SXUNUSED(apArg);` |
|        3 |  8805 | `	return PH7_OK;` |
|        1 |  8806 |  |
|        - |  8807 | `/*` |
|        - |  8808 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8809 | ` * exception is triggered.` |
|        - |  8810 | ` */` |
|      472 |  8811 | `static sxi32 VmUncaughtException(` |
|        - |  8812 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8813 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8814 | `	)` |
|        1 |  8815 |  |
|        - |  8816 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8817 | `	int nArg = 1;` |
|        - |  8818 | `	sxi32 rc;` |
|      473 |  8819 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8820 | `		/* Nesting limit reached */` |
|      ! 0 |  8821 | `		return SXRET_OK;` |
|        - |  8822 | `	}` |
|        - |  8823 | `	/* Call any exception handler if available */` |
|      473 |  8824 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8825 | `	if( pThis ){` |
|        - |  8826 | `		/* Load the exception instance */` |
|      473 |  8827 | `		sArg.x.pOther = pThis;` |
|      473 |  8828 | `		pThis->iRef++;` |
|      473 |  8829 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8830 | `	}else{` |
|      ! 0 |  8831 | `		nArg = 0;` |
|        - |  8832 | `	}` |
|      473 |  8833 | `	apArg[0] = &sArg;` |
|        - |  8834 | `	/* Call the exception handler if available */` |
|      473 |  8835 | `	pVm->nExceptDepth++;` |
|      473 |  8836 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8837 | `	pVm->nExceptDepth--;` |
|      473 |  8838 | `	if( rc != SXRET_OK ){` |
|        - |  8839 | `		SyBlob sMsgBuf;` |
|      471 |  8840 | `		const char *zClass = "Exception";` |
|      471 |  8841 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8842 | `		const char *zMsg;` |
|        - |  8843 | `		sxu32 nMsg;` |
|        - |  8844 | `		const char *zFuncName;` |
|        - |  8845 | `		int nFuncLen;` |
|      471 |  8846 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8847 | `		if( pThis ){` |
|        - |  8848 | `			ph7_class_method *pGetMessage;` |
|        - |  8849 | `			ph7_value sMsg;` |
|        - |  8850 | `			const char *zTmp;` |
|        - |  8851 | `			int nTmp;` |
|      471 |  8852 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8853 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8854 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8855 | `			if( pGetMessage ){` |
|      471 |  8856 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8857 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8858 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8859 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8860 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8861 | `					}` |
|      235 |  8862 | `				}` |
|      471 |  8863 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8864 | `			}` |
|      235 |  8865 | `		}` |
|      471 |  8866 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8867 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8868 | `		}` |
|      471 |  8869 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8870 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8871 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8872 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8873 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8874 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8875 | `		rc = SXERR_ABORT;` |
|      235 |  8876 | `	}` |
|      473 |  8877 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8878 | `	return rc;` |
|      237 |  8879 |  |
|        - |  8880 | `/*` |
|        - |  8881 | ` * Throw an user exception.` |
|        - |  8882 | ` */` |
|      508 |  8883 | `static sxi32 VmThrowException(` |
|        - |  8884 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8885 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8886 | `	)` |
|        2 |  8887 |  |
|        - |  8888 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8889 | `	ph7_exception **apException;` |
|        - |  8890 | `	ph7_exception *pException;` |
|        - |  8891 | `	/* Point to the stack of loaded exceptions */` |
|      510 |  8892 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      510 |  8893 | `	pException = 0;` |
|      510 |  8894 | `	pCatch = 0;` |
|      510 |  8895 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8896 | `		ph7_exception_block *aCatch;` |
|        - |  8897 | `		ph7_class *pClass;` |
|        - |  8898 | `		sxu32 j;` |
|        - |  8899 | `		/* Locate the appropriate block to execute */` |
|       34 |  8900 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       34 |  8901 | `		(void)SySetPop(&pVm->aException);` |
|       34 |  8902 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       34 |  8903 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       32 |  8904 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8905 | `			/* Extract the target class */` |
|       32 |  8906 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       32 |  8907 | `			if( pClass == 0 ){` |
|        - |  8908 | `				/* No such class */` |
|      ! 0 |  8909 | `				continue;` |
|        - |  8910 | `			}` |
|       32 |  8911 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8912 | `				/* Catch block found,break immeditaley */` |
|       32 |  8913 | `				pCatch = &aCatch[j];` |
|       32 |  8914 | `				break;` |
|        - |  8915 | `			}` |
|      ! 0 |  8916 | `		}` |
|       16 |  8917 | `	}` |
|        - |  8918 | `	/* Execute the cached block if available */` |
|      510 |  8919 | `	if( pCatch == 0 ){` |
|        - |  8920 | `		sxi32 rc;` |
|        - |  8921 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  8922 | `		if( pException && pException->iHasFinally ){` |
|        3 |  8923 | `			pException->iFinallyDone = 1;` |
|        3 |  8924 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  8925 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8926 | `				return SXERR_ABORT;` |
|        - |  8927 | `			}` |
|        1 |  8928 | `		}` |
|        - |  8929 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  8930 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8931 | `			/* Re-throw to the outer handler */` |
|        3 |  8932 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  8933 | `		}` |
|        - |  8934 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  8935 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  8936 | `		 * exception instead of reporting it uncaught.` |
|        - |  8937 | `		 */` |
|      478 |  8938 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  8939 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  8940 | `			 * by looking for a catch frame on the stack.` |
|        - |  8941 | `			 */` |
|      478 |  8942 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  8943 | `			int inCatch = 0;` |
|      956 |  8944 | `			while( pF ){` |
|      484 |  8945 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  8946 | `					inCatch = 1;` |
|        6 |  8947 | `					break;` |
|        - |  8948 | `				}` |
|      479 |  8949 | `				pF = pF->pParent;` |
|        1 |  8950 | `			}` |
|      478 |  8951 | `			if( inCatch ){` |
|        - |  8952 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  8953 | `				pThis->iRef++;` |
|        6 |  8954 | `				pVm->pPendingException = pThis;` |
|        6 |  8955 | `				return SXRET_OK;` |
|        - |  8956 | `			}` |
|      236 |  8957 | `		}` |
|        - |  8958 | `		/* Truly uncaught */` |
|      473 |  8959 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8960 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8961 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8962 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  8963 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8964 | `			}` |
|      ! 0 |  8965 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  8966 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8967 | `			}` |
|      ! 0 |  8968 | `		}` |
|      473 |  8969 | `		return rc;` |
|      ! 0 |  8970 | `	}else{` |
|       32 |  8971 | `		VmFrame *pFrame = pVm->pFrame;` |
|       32 |  8972 | `		ph7_exception **apSaved = 0;` |
|        - |  8973 | `		sxu32 nSavedCount;` |
|        - |  8974 | `		sxi32 rc;` |
|       62 |  8975 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|       32 |  8976 | `			pFrame = pFrame->pParent;` |
|        2 |  8977 | `		}` |
|       32 |  8978 | `		if( pException->pFrame == pFrame ){` |
|       24 |  8979 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 |  8980 | `		}` |
|        - |  8981 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  8982 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  8983 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  8984 | `		 */` |
|       32 |  8985 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       32 |  8986 | `		if( nSavedCount > 0 ){` |
|       11 |  8987 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  8988 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8989 | `			if( apSaved ){` |
|       11 |  8990 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  8991 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8992 | `				SySetReset(&pVm->aException);` |
|        3 |  8993 | `			}` |
|        3 |  8994 | `		}` |
|        - |  8995 | `		/* Create a private frame first */` |
|       32 |  8996 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       32 |  8997 | `		if( rc == SXRET_OK ){` |
|       32 |  8998 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       32 |  8999 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       32 |  9000 | `			if( pObj ){` |
|       32 |  9001 | `				pThis->iRef++;` |
|       32 |  9002 | `				pObj->x.pOther = pThis;` |
|       32 |  9003 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       15 |  9004 | `			}` |
|        - |  9005 | `			/* Execute the catch block */` |
|       32 |  9006 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9007 | `			/* Leave the frame */` |
|       32 |  9008 | `			VmLeaveFrame(&(*pVm));` |
|       15 |  9009 | `		}` |
|        - |  9010 | `		/* Restore the outer exception handlers */` |
|       32 |  9011 | `		if( apSaved ){` |
|        - |  9012 | `			sxu32 k;` |
|        - |  9013 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  9014 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  9015 | `			 * Restore the original outer entries.` |
|        - |  9016 | `			 */` |
|        8 |  9017 | `			SySetReset(&pVm->aException);` |
|       14 |  9018 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  9019 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  9020 | `			}` |
|        8 |  9021 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  9022 | `		}` |
|        - |  9023 | `		/* Execute the finally block after catch */` |
|       32 |  9024 | `		if( pException->iHasFinally ){` |
|       11 |  9025 | `			pException->iFinallyDone = 1;` |
|        - |  9026 | `			{` |
|       11 |  9027 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 |  9028 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  9029 | `					return SXERR_ABORT;` |
|        - |  9030 | `				}` |
|        - |  9031 | `			}` |
|        5 |  9032 | `		}` |
|       32 |  9033 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9034 | `			return SXERR_ABORT;` |
|        - |  9035 | `		}` |
|        - |  9036 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  9037 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  9038 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  9039 | `		 */` |
|       32 |  9040 | `		if( pVm->pPendingException ){` |
|        6 |  9041 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  9042 | `			pVm->pPendingException = 0;` |
|        6 |  9043 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  9044 | `		}` |
|        - |  9045 | `	}` |
|        - |  9046 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9047 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9048 | `	 */` |
|       28 |  9049 | `	return SXRET_OK;` |
|      256 |  9050 |  |
|        - |  9051 | `/*` |
|        - |  9052 | ` * Section:` |
|        - |  9053 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9054 | ` * Status:` |
|        - |  9055 | ` *    Stable.` |
|        - |  9056 | ` */` |
|        - |  9057 | `/*` |
|        - |  9058 | ` * string ph7version(void)` |
|        - |  9059 | ` *  Returns the running version of the PH7 version.` |
|        - |  9060 | ` * Parameters` |
|        - |  9061 | ` *  None` |
|        - |  9062 | ` * Return` |
|        - |  9063 | ` * Current PH7 version.` |
|        - |  9064 | ` */` |
|        2 |  9065 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9066 |  |
|        1 |  9067 | `	SXUNUSED(nArg);` |
|        1 |  9068 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9069 | `	/* Current engine version */` |
|        3 |  9070 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9071 | `	return PH7_OK;` |
|        1 |  9072 |  |
|        - |  9073 | `/*` |
|        - |  9074 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9075 | ` */` |
|        - |  9076 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9077 | ` "<html><head>"\` |
|        - |  9078 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9079 | ` "<style type=\"text/css\">"\` |
|        - |  9080 | ` "div {"\` |
|        - |  9081 | `     "border: 1px solid #cccccc;"\` |
|        - |  9082 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9083 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9084 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9085 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9086 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9087 | `     "-o-border-radius: 10px;"\` |
|        - |  9088 | `     "border-radius: 10px;"\` |
|        - |  9089 | `     "padding-left: 2em;"\` |
|        - |  9090 | `     "background-color: white;"\` |
|        - |  9091 | `     "margin-left: auto;"\` |
|        - |  9092 | `     "font-family: verdana;"\` |
|        - |  9093 | `     "padding-right: 2em;"\` |
|        - |  9094 | `     "margin-right: auto;"\` |
|        - |  9095 | `     "}"\` |
|        - |  9096 | `     "body {"\` |
|        - |  9097 | `     "padding: 0.2em;"\` |
|        - |  9098 | `     "font-style: normal;"\` |
|        - |  9099 | `     "font-size: medium;"\` |
|        - |  9100 | `     "background-color: #f2f2f2;"\` |
|        - |  9101 | `     "}"\` |
|        - |  9102 | `     "hr {"\` |
|        - |  9103 | `     "border-style: solid none none;"\` |
|        - |  9104 | `     "border-width: 1px medium medium;"\` |
|        - |  9105 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9106 | `     "height: 1px;"\` |
|        - |  9107 | `     "}"\` |
|        - |  9108 | `     "a {"\` |
|        - |  9109 | `     "color: #3366cc;"\` |
|        - |  9110 | `     "text-decoration: none;"\` |
|        - |  9111 | `     "}"\` |
|        - |  9112 | `     "a:hover {"\` |
|        - |  9113 | `     "color: #999999;"\` |
|        - |  9114 | `     "}"\` |
|        - |  9115 | `     "a:active {"\` |
|        - |  9116 | `     "color: #663399;"\` |
|        - |  9117 | `     "}"\` |
|        - |  9118 | `     "h1 {"\` |
|        - |  9119 | `     "margin: 0;"\` |
|        - |  9120 | `     "padding: 0;"\` |
|        - |  9121 | `     "font-family: Verdana;"\` |
|        - |  9122 | `     "font-weight: bold;"\` |
|        - |  9123 | `     "font-style: normal;"\` |
|        - |  9124 | `     "font-size: medium;"\` |
|        - |  9125 | `     "text-transform: capitalize;"\` |
|        - |  9126 | `     "color: #0a328c;"\` |
|        - |  9127 | `     "}"\` |
|        - |  9128 | `     "p {"\` |
|        - |  9129 | `     "margin: 0 auto;"\` |
|        - |  9130 | `     "font-size: medium;"\` |
|        - |  9131 | `     "font-style: normal;"\` |
|        - |  9132 | `     "font-family: verdana;"\` |
|        - |  9133 | `     "}"\` |
|        - |  9134 | `"</style></head><body>"\` |
|        - |  9135 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9136 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9137 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9138 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9139 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9140 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9141 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9142 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9143 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9144 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9145 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9146 |  |
|        - |  9147 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9148 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9149 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9150 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9151 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9152 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9153 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9154 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9155 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9156 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9157 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9158 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9159 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9160 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9161 |  |
|        - |  9162 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9163 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9164 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9165 | `"&nbsp;*<br>"\` |
|        - |  9166 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9167 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9168 | `"&nbsp;* are met:<br>"\` |
|        - |  9169 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9170 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9171 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9172 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9173 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9174 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9175 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9176 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9177 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9178 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9179 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9180 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9181 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9182 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9183 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9184 | `"&nbsp;*<br>"\` |
|        - |  9185 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9186 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9187 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9188 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9189 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9190 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9191 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9192 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9193 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9194 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9195 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9196 | `"&nbsp;*/<br>"\` |
|        - |  9197 | `"</span></small></small></p>"\` |
|        - |  9198 | `"</div></body></html>"` |
|        - |  9199 | `/*` |
|        - |  9200 | ` * bool ph7credits(void)` |
|        - |  9201 | ` * bool ph7info(void)` |
|        - |  9202 | ` * bool ph7copyright(void)` |
|        - |  9203 | ` *  Prints out the credits for PH7 engine` |
|        - |  9204 | ` * Parameters` |
|        - |  9205 | ` *  None` |
|        - |  9206 | ` * Return` |
|        - |  9207 | ` *  Always TRUE` |
|        - |  9208 | ` */` |
|        2 |  9209 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9210 |  |
|        3 |  9211 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9212 | `	/* Expand the HTML page above*/` |
|        3 |  9213 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9214 | `	ph7_context_output_format(` |
|        1 |  9215 | `		pCtx,` |
|        - |  9216 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9217 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9218 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9219 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9220 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9221 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9222 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9223 | `#ifdef __WINNT__` |
|        - |  9224 | `		"Windows NT"` |
|        - |  9225 | `#elif defined(__UNIXES__)` |
|        - |  9226 | `		"UNIX-Like"` |
|        - |  9227 | `#else` |
|        - |  9228 | `		"Other OS"` |
|        - |  9229 | `#endif` |
|        - |  9230 | `		);` |
|        3 |  9231 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9232 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9233 | `	SXUNUSED(apArg);` |
|        - |  9234 | `	/* Return TRUE */` |
|        - |  9235 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9236 | `	return PH7_OK;` |
|        1 |  9237 |  |
|        - |  9238 | `/*` |
|        - |  9239 | ` * Section:` |
|        - |  9240 | ` *    URL related routines.` |
|        - |  9241 | ` * Status:` |
|        - |  9242 | ` *    Stable.` |
|        - |  9243 | ` */` |
|        - |  9244 | `/*` |
|        - |  9245 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9246 | ` *  Parse a URL and return its fields.` |
|        - |  9247 | ` * Parameters` |
|        - |  9248 | ` *  $url` |
|        - |  9249 | ` *   The URL to parse.` |
|        - |  9250 | ` * $component` |
|        - |  9251 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9252 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9253 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9254 | ` *  in which case the return value will be an integer).` |
|        - |  9255 | ` * Return` |
|        - |  9256 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9257 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9258 | ` *  this array are:` |
|        - |  9259 | ` *   scheme - e.g. http` |
|        - |  9260 | ` *   host` |
|        - |  9261 | ` *   port` |
|        - |  9262 | ` *   user` |
|        - |  9263 | ` *   pass` |
|        - |  9264 | ` *   path` |
|        - |  9265 | ` *   query - after the question mark ?` |
|        - |  9266 | ` *   fragment - after the hashmark #` |
|        - |  9267 | ` * Note:` |
|        - |  9268 | ` *  FALSE is returned on failure.` |
|        - |  9269 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9270 | ` *  with the standard PHP engine.` |
|        - |  9271 | ` */` |
|       28 |  9272 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9273 |  |
|        - |  9274 | `	const char *zStr; /* Input string */` |
|        - |  9275 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9276 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9277 | `	int nLen;` |
|        - |  9278 | `	sxi32 rc;` |
|       29 |  9279 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9280 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9281 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9282 | `		return PH7_OK;` |
|        - |  9283 | `	}` |
|        - |  9284 | `	/* Extract the given URI */` |
|       29 |  9285 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9286 | `	if( nLen < 1 ){` |
|        - |  9287 | `		/* Nothing to process,return FALSE */` |
|        3 |  9288 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9289 | `		return PH7_OK;` |
|        - |  9290 | `	}` |
|        - |  9291 | `	/* Get a parse */` |
|       27 |  9292 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9293 | `	if( rc != SXRET_OK ){` |
|        - |  9294 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9295 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9296 | `		return PH7_OK;` |
|        - |  9297 | `	}` |
|       27 |  9298 | `	if( nArg > 1 ){` |
|      ! 0 |  9299 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9300 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9301 | `		switch(nComponent){` |
|      ! 0 |  9302 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9303 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9304 | `			if( pComp->nByte < 1 ){` |
|        - |  9305 | `				/* No available value,return NULL */` |
|      ! 0 |  9306 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9307 | `			}else{` |
|      ! 0 |  9308 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9309 | `			}` |
|      ! 0 |  9310 | `			break;` |
|      ! 0 |  9311 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9312 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9313 | `			if( pComp->nByte < 1 ){` |
|        - |  9314 | `				/* No available value,return NULL */` |
|      ! 0 |  9315 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9316 | `			}else{` |
|      ! 0 |  9317 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9318 | `			}` |
|      ! 0 |  9319 | `			break;` |
|      ! 0 |  9320 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9321 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9322 | `			if( pComp->nByte < 1 ){` |
|        - |  9323 | `				/* No available value,return NULL */` |
|      ! 0 |  9324 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9325 | `			}else{` |
|      ! 0 |  9326 | `				int iPort = 0;` |
|        - |  9327 | `				/* Cast the value to integer */` |
|      ! 0 |  9328 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9329 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9330 | `			}` |
|      ! 0 |  9331 | `			break;` |
|      ! 0 |  9332 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9333 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9334 | `			if( pComp->nByte < 1 ){` |
|        - |  9335 | `				/* No available value,return NULL */` |
|      ! 0 |  9336 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9337 | `			}else{` |
|      ! 0 |  9338 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9339 | `			}` |
|      ! 0 |  9340 | `			break;` |
|      ! 0 |  9341 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9342 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9343 | `			if( pComp->nByte < 1 ){` |
|        - |  9344 | `				/* No available value,return NULL */` |
|      ! 0 |  9345 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9346 | `			}else{` |
|      ! 0 |  9347 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9348 | `			}` |
|      ! 0 |  9349 | `			break;` |
|      ! 0 |  9350 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9351 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9352 | `			if( pComp->nByte < 1 ){` |
|        - |  9353 | `				/* No available value,return NULL */` |
|      ! 0 |  9354 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9355 | `			}else{` |
|      ! 0 |  9356 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9357 | `			}` |
|      ! 0 |  9358 | `			break;` |
|      ! 0 |  9359 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9360 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9361 | `			if( pComp->nByte < 1 ){` |
|        - |  9362 | `				/* No available value,return NULL */` |
|      ! 0 |  9363 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9364 | `			}else{` |
|      ! 0 |  9365 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9366 | `			}` |
|      ! 0 |  9367 | `			break;` |
|      ! 0 |  9368 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9369 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9370 | `			if( pComp->nByte < 1 ){` |
|        - |  9371 | `				/* No available value,return NULL */` |
|      ! 0 |  9372 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9373 | `			}else{` |
|      ! 0 |  9374 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9375 | `			}` |
|      ! 0 |  9376 | `			break;` |
|      ! 0 |  9377 | `		default:` |
|        - |  9378 | `			/* No such entry,return NULL */` |
|      ! 0 |  9379 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9380 | `			break;` |
|        - |  9381 | `		}` |
|      ! 0 |  9382 | `	}else{` |
|        - |  9383 | `		ph7_value *pArray,*pValue;` |
|        - |  9384 | `		/* Return an associative array */` |
|       27 |  9385 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9386 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9387 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9388 | `			/* Out of memory */` |
|      ! 0 |  9389 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9390 | `			/* Return false */` |
|      ! 0 |  9391 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9392 | `			return PH7_OK;` |
|        - |  9393 | `		}` |
|        - |  9394 | `		/* Fill the array */` |
|       27 |  9395 | `		pComp = &sURI.sScheme;` |
|       27 |  9396 | `		if( pComp->nByte > 0 ){` |
|       19 |  9397 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9398 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9399 | `		}` |
|        - |  9400 | `		/* Reset the string cursor */` |
|       27 |  9401 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9402 | `		pComp = &sURI.sHost;` |
|       27 |  9403 | `		if( pComp->nByte > 0 ){` |
|       25 |  9404 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9405 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9406 | `		}` |
|        - |  9407 | `		/* Reset the string cursor */` |
|       27 |  9408 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9409 | `		pComp = &sURI.sPort;` |
|       27 |  9410 | `		if( pComp->nByte > 0 ){` |
|       11 |  9411 | `			int iPort = 0;/* cc warning */` |
|        - |  9412 | `			/* Convert to integer */` |
|       11 |  9413 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9414 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9415 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9416 | `		}` |
|        - |  9417 | `		/* Reset the string cursor */` |
|       27 |  9418 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9419 | `		pComp = &sURI.sUser;` |
|       27 |  9420 | `		if( pComp->nByte > 0 ){` |
|        7 |  9421 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9422 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9423 | `		}` |
|        - |  9424 | `		/* Reset the string cursor */` |
|       27 |  9425 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9426 | `		pComp = &sURI.sPass;` |
|       27 |  9427 | `		if( pComp->nByte > 0 ){` |
|        7 |  9428 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9429 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9430 | `		}` |
|        - |  9431 | `		/* Reset the string cursor */` |
|       27 |  9432 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9433 | `		pComp = &sURI.sPath;` |
|       27 |  9434 | `		if( pComp->nByte > 0 ){` |
|       17 |  9435 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9436 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9437 | `		}` |
|        - |  9438 | `		/* Reset the string cursor */` |
|       27 |  9439 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9440 | `		pComp = &sURI.sQuery;` |
|       27 |  9441 | `		if( pComp->nByte > 0 ){` |
|        5 |  9442 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9443 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9444 | `		}` |
|        - |  9445 | `		/* Reset the string cursor */` |
|       27 |  9446 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9447 | `		pComp = &sURI.sFragment;` |
|       27 |  9448 | `		if( pComp->nByte > 0 ){` |
|        5 |  9449 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9450 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9451 | `		}` |
|        - |  9452 | `		/* Return the created array */` |
|       27 |  9453 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9454 | `		/* NOTE:` |
|        - |  9455 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9456 | `		 * automatically as soon we return from this function.` |
|        - |  9457 | `		 */` |
|        - |  9458 | `	}` |
|        - |  9459 | `	/* All done */` |
|       27 |  9460 | `	return PH7_OK;` |
|       15 |  9461 |  |
|        - |  9462 | `/*` |
|        - |  9463 | ` * Section:` |
|        - |  9464 | ` *   Array related routines.` |
|        - |  9465 | ` * Status:` |
|        - |  9466 | ` *    Stable.` |
|        - |  9467 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9468 | ` *  Array related functions that need access to the underlying` |
|        - |  9469 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9470 | ` */` |
|        - |  9471 | `/*` |
|        - |  9472 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9473 | ` * of the following structure.` |
|        - |  9474 | ` */` |
|        - |  9475 | `struct compact_data` |
|        - |  9476 |  |
|        - |  9477 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9478 | `	int nRecCount;      /* Recursion count */` |
|        - |  9479 | `};` |
|        - |  9480 | `/*` |
|        - |  9481 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9482 | ` */` |
|      ! 0 |  9483 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9484 |  |
|      ! 0 |  9485 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9486 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9487 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9488 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9489 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9490 | `		SyString sVar;` |
|      ! 0 |  9491 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9492 | `		if( sVar.nByte > 0 ){` |
|        - |  9493 | `			/* Query the current frame */` |
|      ! 0 |  9494 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9495 | `			/* ^` |
|        - |  9496 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9497 | `			 */` |
|      ! 0 |  9498 | `			if( pKey ){` |
|        - |  9499 | `				/* Perform the insertion */` |
|      ! 0 |  9500 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9501 | `			}` |
|      ! 0 |  9502 | `		}` |
|      ! 0 |  9503 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9504 | `		int rc;` |
|        - |  9505 | `		/* Recursively traverse this array */` |
|      ! 0 |  9506 | `		pData->nRecCount++;` |
|      ! 0 |  9507 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9508 | `		pData->nRecCount--;` |
|      ! 0 |  9509 | `		return rc;` |
|        - |  9510 | `	}` |
|      ! 0 |  9511 | `	return SXRET_OK;` |
|      ! 0 |  9512 |  |
|        - |  9513 | `/*` |
|        - |  9514 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9515 | ` *  Create array containing variables and their values.` |
|        - |  9516 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9517 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9518 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9519 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9520 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9521 | ` * Parameters` |
|        - |  9522 | ` *  $varname` |
|        - |  9523 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9524 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9525 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9526 | ` *   it recursively.` |
|        - |  9527 | ` * Return` |
|        - |  9528 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9529 | ` */` |
|        2 |  9530 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9531 |  |
|        - |  9532 | `	ph7_value *pArray,*pObj;` |
|        3 |  9533 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9534 | `	const char *zName;` |
|        - |  9535 | `	SyString sVar;` |
|        - |  9536 | `	int i,nLen;` |
|        3 |  9537 | `	if( nArg < 1 ){` |
|        - |  9538 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9539 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9540 | `		return PH7_OK;` |
|        - |  9541 | `	}` |
|        - |  9542 | `	/* Create the array */` |
|        3 |  9543 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9544 | `	if( pArray == 0 ){` |
|        - |  9545 | `		/* Out of memory */` |
|      ! 0 |  9546 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9547 | `		/* Return NULL */` |
|      ! 0 |  9548 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9549 | `		return PH7_OK;` |
|        - |  9550 | `	}` |
|        - |  9551 | `	/* Perform the requested operation */` |
|        7 |  9552 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9553 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9554 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9555 | `				struct compact_data sData;` |
|      ! 0 |  9556 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9557 | `				/* Recursively walk the array */` |
|      ! 0 |  9558 | `				sData.nRecCount = 0;` |
|      ! 0 |  9559 | `				sData.pArray = pArray;` |
|      ! 0 |  9560 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9561 | `			}` |
|      ! 0 |  9562 | `		}else{` |
|        - |  9563 | `			/* Extract variable name */` |
|        5 |  9564 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9565 | `			if( nLen > 0 ){` |
|        5 |  9566 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9567 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9568 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9569 | `				if( pObj ){` |
|        5 |  9570 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9571 | `				}` |
|        2 |  9572 | `			}` |
|        - |  9573 | `		}` |
|        3 |  9574 | `	}` |
|        - |  9575 | `	/* Return the array */` |
|        3 |  9576 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9577 | `	return PH7_OK;` |
|        2 |  9578 |  |
|        - |  9579 | `/*` |
|        - |  9580 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9581 | ` * of the following structure.` |
|        - |  9582 | ` */` |
|        - |  9583 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9584 | `struct extract_aux_data` |
|        - |  9585 |  |
|        - |  9586 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9587 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9588 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9589 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9590 | `	int iFlags;           /* Control flags */` |
|        - |  9591 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9592 | `};` |
|        - |  9593 | `/* Forward declaration */` |
|        - |  9594 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9595 | `/*` |
|        - |  9596 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9597 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9598 | ` * Parameters` |
|        - |  9599 | ` * $var_array` |
|        - |  9600 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9601 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9602 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9603 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9604 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9605 | ` * $extract_type` |
|        - |  9606 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9607 | ` *  It can be one of the following values:` |
|        - |  9608 | ` *   EXTR_OVERWRITE` |
|        - |  9609 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9610 | ` *   EXTR_SKIP` |
|        - |  9611 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9612 | ` *   EXTR_PREFIX_SAME` |
|        - |  9613 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9614 | ` *   EXTR_PREFIX_ALL` |
|        - |  9615 | ` *       Prefix all variable names with prefix.` |
|        - |  9616 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9617 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9618 | ` *   EXTR_IF_EXISTS` |
|        - |  9619 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9620 | ` *       otherwise do nothing.` |
|        - |  9621 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9622 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9623 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9624 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9625 | ` *      the current symbol table.` |
|        - |  9626 | ` * $prefix` |
|        - |  9627 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9628 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9629 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9630 | ` *  underscore character.` |
|        - |  9631 | ` * Return` |
|        - |  9632 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9633 | ` */` |
|        4 |  9634 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9635 |  |
|        - |  9636 | `	extract_aux_data sAux;` |
|        - |  9637 | `	ph7_hashmap *pMap;` |
|        5 |  9638 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9639 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9640 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9641 | `		return PH7_OK;` |
|        - |  9642 | `	}` |
|        - |  9643 | `	/* Point to the target hashmap */` |
|        5 |  9644 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9645 | `	if( pMap->nEntry < 1 ){` |
|        - |  9646 | `		/* Empty map,return  0 */` |
|      ! 0 |  9647 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9648 | `		return PH7_OK;` |
|        - |  9649 | `	}` |
|        - |  9650 | `	/* Prepare the aux data */` |
|        5 |  9651 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9652 | `	if( nArg > 1 ){` |
|        3 |  9653 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9654 | `		if( nArg > 2 ){` |
|      ! 0 |  9655 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9656 | `		}` |
|        1 |  9657 | `	}` |
|        5 |  9658 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9659 | `	/* Invoke the worker callback */` |
|        5 |  9660 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9661 | `	/* Number of variables successfully imported */` |
|        5 |  9662 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9663 | `	return PH7_OK;` |
|        3 |  9664 |  |
|        - |  9665 | `/*` |
|        - |  9666 | ` * Worker callback for the [extract()] function defined` |
|        - |  9667 | ` * below.` |
|        - |  9668 | ` */` |
|        8 |  9669 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9670 |  |
|        9 |  9671 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9672 | `	int iFlags = pAux->iFlags;` |
|        9 |  9673 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9674 | `	ph7_value *pObj;` |
|        - |  9675 | `	SyString sVar;` |
|        9 |  9676 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9677 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9678 | `	}` |
|        - |  9679 | `	/* Perform a string cast */` |
|        9 |  9680 | `	PH7_MemObjToString(pKey);` |
|        9 |  9681 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9682 | `		/* Unavailable variable name */` |
|      ! 0 |  9683 | `		return SXRET_OK;` |
|        - |  9684 | `	}` |
|        9 |  9685 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9686 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9687 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9688 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9689 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9690 | `			);` |
|      ! 0 |  9691 | `	}else{` |
|       13 |  9692 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9693 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9694 | `	}` |
|        9 |  9695 | `	sVar.zString = pAux->zWorker;` |
|        - |  9696 | `	/* Try to extract the variable */` |
|        9 |  9697 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9698 | `	if( pObj ){` |
|        - |  9699 | `		/* Collision */` |
|        5 |  9700 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9701 | `			return SXRET_OK;` |
|        - |  9702 | `		}` |
|        5 |  9703 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9704 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9705 | `				/* Already prefixed */` |
|      ! 0 |  9706 | `				return SXRET_OK;` |
|        - |  9707 | `			}` |
|      ! 0 |  9708 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9709 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9710 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9711 | `				);` |
|      ! 0 |  9712 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9713 | `		}` |
|        3 |  9714 | `	}else{` |
|        - |  9715 | `		/* Create the variable */` |
|        5 |  9716 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9717 | `	}` |
|        9 |  9718 | `	if( pObj ){` |
|        - |  9719 | `		/* Overwrite the old value */` |
|        9 |  9720 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9721 | `		/* Increment counter */` |
|        9 |  9722 | `		pAux->iCount++;` |
|        4 |  9723 | `	}` |
|        9 |  9724 | `	return SXRET_OK;` |
|        5 |  9725 |  |
|        - |  9726 | `/*` |
|        - |  9727 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9728 | ` * defined below.` |
|        - |  9729 | ` */` |
|        2 |  9730 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9731 |  |
|        3 |  9732 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9733 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9734 | `	ph7_value *pObj;` |
|        - |  9735 | `	SyString sVar;` |
|        - |  9736 | `	/* Perform a string cast */` |
|        3 |  9737 | `	PH7_MemObjToString(pKey);` |
|        3 |  9738 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9739 | `		/* Unavailable variable name */` |
|      ! 0 |  9740 | `		return SXRET_OK;` |
|        - |  9741 | `	}` |
|        3 |  9742 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9743 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9744 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9745 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9746 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9747 | `			);` |
|        2 |  9748 | `	}else{` |
|      ! 0 |  9749 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9750 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9751 | `	}` |
|        3 |  9752 | `	sVar.zString = pAux->zWorker;` |
|        - |  9753 | `	/* Extract the variable */` |
|        3 |  9754 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9755 | `	if( pObj ){` |
|        3 |  9756 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9757 | `	}` |
|        3 |  9758 | `	return SXRET_OK;` |
|        2 |  9759 |  |
|        - |  9760 | `/*` |
|        - |  9761 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9762 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9763 | ` * Parameters` |
|        - |  9764 | ` * $types` |
|        - |  9765 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9766 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9767 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9768 | ` *  POST includes the POST uploaded file information.` |
|        - |  9769 | ` *  Note:` |
|        - |  9770 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9771 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9772 | ` * $prefix` |
|        - |  9773 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9774 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9775 | ` *  variable named $pref_userid.` |
|        - |  9776 | ` * Return` |
|        - |  9777 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9778 | ` */` |
|        2 |  9779 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9780 |  |
|        - |  9781 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9782 | `	extract_aux_data sAux;` |
|        - |  9783 | `	int nLen,nPrefixLen;` |
|        - |  9784 | `	ph7_value *pSuper;` |
|        - |  9785 | `	ph7_vm *pVm;` |
|        - |  9786 | `	/* By default import only $_GET variables  */` |
|        3 |  9787 | `	zImport = "G";` |
|        3 |  9788 | `	nLen = (int)sizeof(char);` |
|        3 |  9789 | `	zPrefix = 0;` |
|        3 |  9790 | `	nPrefixLen = 0;` |
|        3 |  9791 | `	if( nArg > 0 ){` |
|        3 |  9792 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9793 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9794 | `		}` |
|        3 |  9795 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9796 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9797 | `		}` |
|        1 |  9798 | `	}` |
|        - |  9799 | `	/* Point to the underlying VM */` |
|        3 |  9800 | `	pVm = pCtx->pVm;` |
|        - |  9801 | `	/* Initialize the aux data */` |
|        3 |  9802 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9803 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9804 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9805 | `	sAux.pVm = pVm;` |
|        - |  9806 | `	/* Extract */` |
|        3 |  9807 | `	zEnd = &zImport[nLen];` |
|        5 |  9808 | `	while( zImport < zEnd ){` |
|        3 |  9809 | `		int c = zImport[0];` |
|        3 |  9810 | `		pSuper = 0;` |
|        3 |  9811 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9812 | `			/* Import $_GET variables */` |
|        3 |  9813 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9814 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9815 | `			/* Import $_POST variables */` |
|      ! 0 |  9816 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9817 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9818 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9819 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9820 | `		}` |
|        3 |  9821 | `		if( pSuper ){` |
|        - |  9822 | `			/* Iterate throw array entries */` |
|        3 |  9823 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9824 | `		}` |
|        - |  9825 | `		/* Advance the cursor */` |
|        3 |  9826 | `		zImport++;` |
|        1 |  9827 | `	}` |
|        - |  9828 | `	/* All done,return TRUE*/` |
|        3 |  9829 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9830 | `	return PH7_OK;` |
|        1 |  9831 |  |
|        - |  9832 | `/*` |
|        - |  9833 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9834 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9835 | ` * information.` |
|        - |  9836 | ` */` |
|    10098 |  9837 | `static sxi32 VmEvalChunk(` |
|        - |  9838 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9839 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9840 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9841 | `	int iFlags,         /* Compile flag */` |
|        - |  9842 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9843 | `	)` |
|        2 |  9844 |  |
|        - |  9845 | `	SySet *pByteCode,aByteCode;` |
|        - |  9846 | `	SyBlob sSavedNs;` |
|    10100 |  9847 | `	ProcConsumer xErr = 0;` |
|    10100 |  9848 | `	void *pErrData = 0;` |
|        - |  9849 | `	/* Initialize bytecode container */` |
|    10100 |  9850 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10100 |  9851 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9852 | `	/* Reset the code generator */` |
|    10100 |  9853 | `	if( bTrueReturn ){` |
|        - |  9854 | `		/* Included file,log compile-time errors */` |
|     7535 |  9855 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9856 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9857 | `	}` |
|    10100 |  9858 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9859 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9860 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9861 | `	 * the caller's namespace is restored. */` |
|    10100 |  9862 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10100 |  9863 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10100 |  9864 | `	if( bTrueReturn ){` |
|        - |  9865 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7535 |  9866 | `		SyBlobReset(&pVm->sNamespace);` |
|     3767 |  9867 | `	}` |
|        - |  9868 | `	/* Swap bytecode container */` |
|    10100 |  9869 | `	pByteCode = pVm->pByteContainer;` |
|    10100 |  9870 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9871 | `	/* Compile the chunk */` |
|    10100 |  9872 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15149 |  9873 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9874 | `		/* Compilation error,return false */` |
|        3 |  9875 | `		if( pCtx ){` |
|        3 |  9876 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9877 | `		}` |
|        2 |  9878 | `	}else{` |
|        - |  9879 | `		/* Mount any newly defined classes */` |
|        - |  9880 | `		SyHashEntry *pEntry;` |
|        - |  9881 | `		ph7_class *pClass;` |
|        - |  9882 | `		ph7_value sResult; /* Return value */` |
|        - |  9883 | `		sxi32 rc;` |
|    10098 |  9884 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   276318 |  9885 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   261174 |  9886 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9887 | `			/* Only mount classes that haven't been mounted yet */` |
|   261174 |  9888 | `			if( !pClass->bMounted ){` |
|    62692 |  9889 | `				rc = VmMountUserClass(pVm,pClass);` |
|    62692 |  9890 | `				if( rc != SXRET_OK ){` |
|        - |  9891 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9892 | `					if( pCtx ){` |
|      ! 0 |  9893 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9894 | `					}` |
|      ! 0 |  9895 | `					goto Cleanup;` |
|        - |  9896 | `				}` |
|    31345 |  9897 | `			}` |
|        2 |  9898 | `		}` |
|    10098 |  9899 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9900 | `			/* Out of memory */` |
|      ! 0 |  9901 | `			if( pCtx ){` |
|      ! 0 |  9902 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9903 | `			}` |
|      ! 0 |  9904 | `			goto Cleanup;` |
|        - |  9905 | `		}` |
|    10098 |  9906 | `		if( bTrueReturn ){` |
|        - |  9907 | `			/* Assume a boolean true return value */` |
|     7535 |  9908 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9909 | `		}else{` |
|        - |  9910 | `			/* Assume a null return value */` |
|     2564 |  9911 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9912 | `		}` |
|        - |  9913 | `		/* Execute the compiled chunk */` |
|    10098 |  9914 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10098 |  9915 | `		if( pCtx ){` |
|        - |  9916 | `			/* Set the execution result */` |
|     7548 |  9917 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9918 | `		}` |
|    10098 |  9919 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9920 | `	}` |
|     5049 |  9921 | `Cleanup:` |
|        - |  9922 | `	/* Cleanup the mess left behind */` |
|    10100 |  9923 | `	pVm->pByteContainer = pByteCode;` |
|    10100 |  9924 | `	SySetRelease(&aByteCode);` |
|        - |  9925 | `	/* Restore caller's namespace state */` |
|    10100 |  9926 | `	SyBlobReset(&pVm->sNamespace);` |
|    10100 |  9927 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10100 |  9928 | `	SyBlobRelease(&sSavedNs);` |
|    10100 |  9929 | `	return SXRET_OK;` |
|        2 |  9930 |  |
|        - |  9931 | `/*` |
|        - |  9932 | ` * value eval(string $code)` |
|        - |  9933 | ` *   Evaluate a string as PHP code.` |
|        - |  9934 | ` * Parameter` |
|        - |  9935 | ` *  code: PHP code to evaluate.` |
|        - |  9936 | ` * Return` |
|        - |  9937 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9938 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9939 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9940 | ` */` |
|       16 |  9941 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9942 |  |
|        - |  9943 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9944 | `	if( nArg < 1 ){` |
|        - |  9945 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9946 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9947 | `		return SXRET_OK;` |
|        - |  9948 | `	}` |
|        - |  9949 | `	/* Chunk to evaluate */` |
|       18 |  9950 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9951 | `	if( sChunk.nByte < 1 ){` |
|        - |  9952 | `		/* Empty string,return NULL */` |
|        3 |  9953 | `		ph7_result_null(pCtx);` |
|        3 |  9954 | `		return SXRET_OK;` |
|        - |  9955 | `	}` |
|        - |  9956 | `	/* Eval the chunk */` |
|       16 |  9957 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9958 | `	return SXRET_OK;` |
|       10 |  9959 |  |
|        - |  9960 | `/*` |
|        - |  9961 | ` * Check if a file path is already included.` |
|        - |  9962 | ` */` |
|    15064 |  9963 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9964 |  |
|        - |  9965 | `	SyString *aEntries;` |
|        - |  9966 | `	sxu32 n;` |
|    15065 |  9967 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9968 | `	/* Perform a linear search */` |
| 56720651 |  9969 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9970 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9971 | `			/* Already included */` |
|        7 |  9972 | `			return TRUE;` |
|        - |  9973 | `		}` |
| 28352794 |  9974 | `	}` |
|    15059 |  9975 | `	return FALSE;` |
|     7533 |  9976 |  |
|        - |  9977 | `/*` |
|        - |  9978 | ` * Push a file path in the appropriate VM container.` |
|        - |  9979 | ` */` |
|    17606 |  9980 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9981 |  |
|        - |  9982 | `	SyString sPath;` |
|        - |  9983 | `	char *zDup;` |
|        - |  9984 | `#ifdef __WINNT__` |
|        - |  9985 | `	char *zCur;` |
|        - |  9986 | `#endif` |
|        - |  9987 | `	sxi32 rc;` |
|    17608 |  9988 | `	if( nLen < 0 ){` |
|     2544 |  9989 | `		nLen = SyStrlen(zPath);` |
|     1271 |  9990 | `	}` |
|        - |  9991 | `	/* Duplicate the file path first */` |
|    17608 |  9992 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17608 |  9993 | `	if( zDup == 0 ){` |
|      ! 0 |  9994 | `		return SXERR_MEM;` |
|        - |  9995 | `	}` |
|        - |  9996 | `#ifdef __WINNT__` |
|        - |  9997 | `	/* Normalize path on windows` |
|        - |  9998 | `	 * Example:` |
|        - |  9999 | `	 *    Path/To/File.php` |
|        - | 10000 | `	 * becomes` |
|        - | 10001 | `	 *   path\to\file.php` |
|        - | 10002 | `	 */` |
|        2 | 10003 | `	zCur = zDup;` |
|        2 | 10004 | `	while( zCur[0] != 0 ){` |
|        2 | 10005 | `		if( zCur[0] == '/' ){` |
|        2 | 10006 | `			zCur[0] = '\\';` |
|        2 | 10007 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10008 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10009 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10010 | `		}` |
|        2 | 10011 | `		zCur++;` |
|        2 | 10012 | `	}` |
|        - | 10013 | `#endif` |
|        - | 10014 | `	/* Install the file path */` |
|    17608 | 10015 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17608 | 10016 | `	if( !bMain ){` |
|    15065 | 10017 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10018 | `			/* Already included */` |
|        7 | 10019 | `			*pNew = 0;` |
|        4 | 10020 | `		}else{` |
|        - | 10021 | `			/* Insert in the corresponding container */` |
|    15059 | 10022 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 | 10023 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10024 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10025 | `				return rc;` |
|        - | 10026 | `			}` |
|    15059 | 10027 | `			*pNew = 1;` |
|        - | 10028 | `		}` |
|     7532 | 10029 | `	}` |
|    17608 | 10030 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17608 | 10031 | `	return SXRET_OK;` |
|     8805 | 10032 |  |
|        - | 10033 | `/*` |
|        - | 10034 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10035 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10036 | ` * indicates failure.` |
|        - | 10037 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10038 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10039 | ` * operations.` |
|        - | 10040 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10041 | ` * this function is a no-op.` |
|        - | 10042 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10043 | ` * constructs for more information.` |
|        - | 10044 | ` */` |
|     7540 | 10045 | `static sxi32 VmExecIncludedFile(` |
|        - | 10046 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10047 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10048 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10049 | `	 )` |
|        2 | 10050 |  |
|        - | 10051 | `	sxi32 rc;` |
|        - | 10052 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10053 | `	const ph7_io_stream *pStream;` |
|        - | 10054 | `	SyBlob sContents;` |
|        - | 10055 | `	void *pHandle;` |
|        - | 10056 | `	ph7_vm *pVm;` |
|        - | 10057 | `	int isNew;` |
|        - | 10058 | `	/* Initialize fields */` |
|     7542 | 10059 | `	pVm = pCtx->pVm;` |
|     7542 | 10060 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 | 10061 | `	isNew = 0;` |
|        - | 10062 | `	/* Extract the associated stream */` |
|     7542 | 10063 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10064 | `	/*` |
|        - | 10065 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10066 | `	 * in a read-only mode.` |
|        - | 10067 | `	 */` |
|     7542 | 10068 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 | 10069 | `	if( pHandle == 0 ){` |
|        3 | 10070 | `		return SXERR_IO;` |
|        - | 10071 | `	}` |
|     7539 | 10072 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 | 10073 | `	if( IncludeOnce && !isNew ){` |
|        - | 10074 | `		/* Already included */` |
|        5 | 10075 | `		rc = SXERR_EXISTS;` |
|        3 | 10076 | `	}else{` |
|        - | 10077 | `		/* Read the whole file contents */` |
|     7535 | 10078 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 | 10079 | `		if( rc == SXRET_OK ){` |
|        - | 10080 | `			SyString sScript;` |
|        - | 10081 | `			/* Compile and execute the script */` |
|     7535 | 10082 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 | 10083 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 | 10084 | `		}` |
|        - | 10085 | `	}` |
|        - | 10086 | `	/* Pop from the set of included file */` |
|     7539 | 10087 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10088 | `	/* Close the handle */` |
|     7539 | 10089 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10090 | `	/* Release the working buffer */` |
|     7539 | 10091 | `	SyBlobRelease(&sContents);` |
|        - | 10092 | `#else` |
|        - | 10093 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10094 | `	SXUNUSED(pPath);` |
|        - | 10095 | `	SXUNUSED(IncludeOnce);` |
|        - | 10096 | `	rc = SXERR_IO;` |
|        - | 10097 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 | 10098 | `	return rc;` |
|     3772 | 10099 |  |
|        - | 10100 | `/*` |
|        - | 10101 | ` * string get_include_path(void)` |
|        - | 10102 | ` *  Gets the current include_path configuration option.` |
|        - | 10103 | ` * Parameter` |
|        - | 10104 | ` *  None` |
|        - | 10105 | ` * Return` |
|        - | 10106 | ` *  Included paths as a string` |
|        - | 10107 | ` */` |
|        2 | 10108 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10109 |  |
|        3 | 10110 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10111 | `	SyString *aEntry;` |
|        - | 10112 | `	int dir_sep;` |
|        - | 10113 | `	sxu32 n;` |
|        - | 10114 | `#ifdef __WINNT__` |
|        1 | 10115 | `	dir_sep = ';';` |
|        - | 10116 | `#else` |
|        - | 10117 | `	/* Assume UNIX path separator */` |
|        2 | 10118 | `	dir_sep = ':';` |
|        - | 10119 | `#endif` |
|        1 | 10120 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10121 | `	SXUNUSED(apArg);` |
|        - | 10122 | `	/* Point to the list of import paths */` |
|        3 | 10123 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10124 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10125 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10126 | `		if( n > 0 ){` |
|        - | 10127 | `			/* Append dir seprator */` |
|      ! 0 | 10128 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10129 | `		}` |
|        - | 10130 | `		/* Append path */` |
|        3 | 10131 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10132 | `	}` |
|        3 | 10133 | `	return PH7_OK;` |
|        1 | 10134 |  |
|        - | 10135 | `/*` |
|        - | 10136 | ` * string get_get_included_files(void)` |
|        - | 10137 | ` *  Gets the current include_path configuration option.` |
|        - | 10138 | ` * Parameter` |
|        - | 10139 | ` *  None` |
|        - | 10140 | ` * Return` |
|        - | 10141 | ` *  Included paths as a string` |
|        - | 10142 | ` */` |
|        2 | 10143 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10144 |  |
|        3 | 10145 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10146 | `	ph7_value *pArray,*pWorker;` |
|        - | 10147 | `	SyString *pEntry;` |
|        - | 10148 | `	int c,d;` |
|        - | 10149 | `	/* Create an array and a working value */` |
|        3 | 10150 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10151 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10152 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10153 | `		/* Out of memory,return null */` |
|      ! 0 | 10154 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10155 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10156 | `		SXUNUSED(apArg);` |
|      ! 0 | 10157 | `		return PH7_OK;` |
|        - | 10158 | `	}` |
|        3 | 10159 | `	c = d = '/';` |
|        - | 10160 | `#ifdef __WINNT__` |
|        1 | 10161 | `	d = '\\';` |
|        - | 10162 | `#endif` |
|        - | 10163 | `	/* Iterate throw entries */` |
|        3 | 10164 | `	SySetResetCursor(pFiles);` |
|     3691 | 10165 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10166 | `		const char *zBase,*zEnd;` |
|        - | 10167 | `		int iLen;` |
|        - | 10168 | `		/* reset the string cursor */` |
|     3689 | 10169 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10170 | `		/* Extract base name */` |
|     3689 | 10171 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10172 | `		/* Ignore trailing '/' */` |
|     5533 | 10173 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10174 | `			zEnd--;` |
|      ! 0 | 10175 | `		}` |
|     3689 | 10176 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 | 10177 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 | 10178 | `			zEnd--;` |
|        1 | 10179 | `		}` |
|     3689 | 10180 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 | 10181 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10182 | `		/* Copy entry name */` |
|     3689 | 10183 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10184 | `		/* Perform the insertion */` |
|     3689 | 10185 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10186 | `	}` |
|        - | 10187 | `	/* All done,return the created array */` |
|        3 | 10188 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10189 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10190 | `	 * by the engine as soon we return from this foreign` |
|        - | 10191 | `	 * function.` |
|        - | 10192 | `	 */` |
|        3 | 10193 | `	return PH7_OK;` |
|        2 | 10194 |  |
|        - | 10195 | `/*` |
|        - | 10196 | ` * include:` |
|        - | 10197 | ` * According to the PHP reference manual.` |
|        - | 10198 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10199 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10200 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10201 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10202 | ` *  and the current working directory before failing. The include()` |
|        - | 10203 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10204 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10205 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10206 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10207 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10208 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10209 | ` *  directory to find the requested file.` |
|        - | 10210 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10211 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10212 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10213 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10214 | ` */` |
|     7528 | 10215 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10216 |  |
|        - | 10217 | `	SyString sFile;` |
|        - | 10218 | `	sxi32 rc;` |
|     7530 | 10219 | `	if( nArg < 1 ){` |
|        - | 10220 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10221 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10222 | `		return SXRET_OK;` |
|        - | 10223 | `	}` |
|        - | 10224 | `	/* File to include */` |
|     7530 | 10225 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 | 10226 | `	if( sFile.nByte < 1 ){` |
|        - | 10227 | `		/* Empty string,return NULL */` |
|      ! 0 | 10228 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10229 | `		return SXRET_OK;` |
|        - | 10230 | `	}` |
|        - | 10231 | `	/* Open,compile and execute the desired script */` |
|     7530 | 10232 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 | 10233 | `	if( rc != SXRET_OK ){` |
|        - | 10234 | `		/* Emit a warning and return false */` |
|        3 | 10235 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10236 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10237 | `	}` |
|     7530 | 10238 | `	return SXRET_OK;` |
|     3766 | 10239 |  |
|        - | 10240 | `/*` |
|        - | 10241 | ` * include_once:` |
|        - | 10242 | ` *  According to the PHP reference manual.` |
|        - | 10243 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10244 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10245 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10246 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10247 | ` *   just once.` |
|        - | 10248 | ` */` |
|        4 | 10249 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10250 |  |
|        - | 10251 | `	SyString sFile;` |
|        - | 10252 | `	sxi32 rc;` |
|        5 | 10253 | `	if( nArg < 1 ){` |
|        - | 10254 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10255 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10256 | `		return SXRET_OK;` |
|        - | 10257 | `	}` |
|        - | 10258 | `	/* File to include */` |
|        5 | 10259 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10260 | `	if( sFile.nByte < 1 ){` |
|        - | 10261 | `		/* Empty string,return NULL */` |
|      ! 0 | 10262 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10263 | `		return SXRET_OK;` |
|        - | 10264 | `	}` |
|        - | 10265 | `	/* Open,compile and execute the desired script */` |
|        5 | 10266 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10267 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10268 | `		/* File already included,return TRUE */` |
|        3 | 10269 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10270 | `		return SXRET_OK;` |
|        - | 10271 | `	}` |
|        3 | 10272 | `	if( rc != SXRET_OK ){` |
|        - | 10273 | `		/* Emit a warning and return false */` |
|      ! 0 | 10274 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10275 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10276 | ` 	}` |
|        3 | 10277 | `	return SXRET_OK;` |
|        3 | 10278 |  |
|        - | 10279 | `/*` |
|        - | 10280 | ` * require.` |
|        - | 10281 | ` *  According to the PHP reference manual.` |
|        - | 10282 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10283 | ` *   also produce a fatal level error.` |
|        - | 10284 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10285 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10286 | ` */` |
|        4 | 10287 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10288 |  |
|        - | 10289 | `	SyString sFile;` |
|        - | 10290 | `	sxi32 rc;` |
|        5 | 10291 | `	if( nArg < 1 ){` |
|        - | 10292 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10293 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10294 | `		return SXRET_OK;` |
|        - | 10295 | `	}` |
|        - | 10296 | `	/* File to include */` |
|        5 | 10297 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10298 | `	if( sFile.nByte < 1 ){` |
|        - | 10299 | `		/* Empty string,return NULL */` |
|      ! 0 | 10300 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10301 | `		return SXRET_OK;` |
|        - | 10302 | `	}` |
|        - | 10303 | `	/* Open,compile and execute the desired script */` |
|        5 | 10304 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10305 | `	if( rc != SXRET_OK ){` |
|        - | 10306 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10307 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10308 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10309 | `		return PH7_ABORT;` |
|        - | 10310 | `	}` |
|        5 | 10311 | `	return SXRET_OK;` |
|        3 | 10312 |  |
|        - | 10313 | `/*` |
|        - | 10314 | ` * require_once:` |
|        - | 10315 | ` *  According to the PHP reference manual.` |
|        - | 10316 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10317 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10318 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10319 | ` *   and how it differs from its non _once siblings.` |
|        - | 10320 | ` */` |
|        4 | 10321 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10322 |  |
|        - | 10323 | `	SyString sFile;` |
|        - | 10324 | `	sxi32 rc;` |
|        5 | 10325 | `	if( nArg < 1 ){` |
|        - | 10326 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10327 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10328 | `		return SXRET_OK;` |
|        - | 10329 | `	}` |
|        - | 10330 | `	/* File to include */` |
|        5 | 10331 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10332 | `	if( sFile.nByte < 1 ){` |
|        - | 10333 | `		/* Empty string,return NULL */` |
|      ! 0 | 10334 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10335 | `		return SXRET_OK;` |
|        - | 10336 | `	}` |
|        - | 10337 | `	/* Open,compile and execute the desired script */` |
|        5 | 10338 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10339 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10340 | `		/* File already included,return TRUE */` |
|        3 | 10341 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10342 | `		return SXRET_OK;` |
|        - | 10343 | `	}` |
|        3 | 10344 | `	if( rc != SXRET_OK ){` |
|        - | 10345 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10346 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10347 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10348 | `		return PH7_ABORT;` |
|        - | 10349 | `	}` |
|        3 | 10350 | `	return SXRET_OK;` |
|        3 | 10351 |  |
|        - | 10352 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10353 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10354 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10355 | `/* Table of built-in VM functions. */` |
|        - | 10356 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10357 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10358 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10359 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10360 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10361 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10362 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10363 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10364 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10365 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10366 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10367 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10368 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10369 | `	    /* Constants management */` |
|        - | 10370 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10371 | `	{ "define",   vm_builtin_define               },` |
|        - | 10372 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10373 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10374 | `	   /* Class/Object functions */` |
|        - | 10375 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10376 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10377 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10378 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10379 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10380 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10381 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10382 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10383 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10384 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10385 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10386 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10387 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10388 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10389 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10390 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10391 | `	   /* Random numbers/strings generators */` |
|        - | 10392 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10393 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10394 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10395 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10396 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10397 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10398 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10399 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10400 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10401 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10402 | `	   /* Language constructs functions */` |
|        - | 10403 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10404 | `	{ "print", vm_builtin_print                   },` |
|        - | 10405 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10406 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10407 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10408 | `	  /* Variable handling functions */` |
|        - | 10409 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10410 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10411 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10412 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10413 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10414 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10415 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10416 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10417 | `	  /* Ouput control functions */` |
|        - | 10418 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10419 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10420 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10421 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10422 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10423 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10424 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10425 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10426 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10427 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10428 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10429 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10430 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10431 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10432 | `	  /* Assertion functions */` |
|        - | 10433 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10434 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10435 | `	  /* Error reporting functions */` |
|        - | 10436 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10437 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10438 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10439 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10440 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10441 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10442 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10443 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10444 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10445 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10446 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10447 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10448 | `	  /* Release info */` |
|        - | 10449 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10450 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10451 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10452 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10453 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10454 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10455 | `	  /* hashmap */` |
|        - | 10456 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10457 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10458 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10459 | `	  /* URL related function */` |
|        - | 10460 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10461 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10462 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10463 | `	   /* XML processing functions */` |
|        - | 10464 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10465 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10466 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10467 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10468 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10469 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10470 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10471 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10472 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10473 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10474 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10475 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10476 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10477 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10478 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10479 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10480 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10481 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10482 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10483 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10484 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10485 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10486 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10487 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10488 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10489 | `	   /* Command line processing */` |
|        - | 10490 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10491 | `	   /* JSON encoding/decoding */` |
|        - | 10492 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10493 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10494 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10495 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10496 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10497 | `	   /* Files/URI inclusion facility */` |
|        - | 10498 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10499 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10500 | `	{ "include",      vm_builtin_include          },` |
|        - | 10501 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10502 | `	{ "require",      vm_builtin_require          },` |
|        - | 10503 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10504 | `};` |
|        - | 10505 | `/*` |
|        - | 10506 | ` * Register the built-in VM functions defined above.` |
|        - | 10507 | ` */` |
|     2288 | 10508 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10509 |  |
|        - | 10510 | `	sxi32 rc;` |
|        - | 10511 | `	sxu32 n;` |
|   286002 | 10512 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10513 | `		/* Note that these special functions have access` |
|        - | 10514 | `		 * to the underlying virtual machine as their` |
|        - | 10515 | `		 * private data.` |
|        - | 10516 | `		 */` |
|   283714 | 10517 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   283714 | 10518 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10519 | `			return rc;` |
|        - | 10520 | `		}` |
|   141858 | 10521 | `	}` |
|     2290 | 10522 | `	return SXRET_OK;` |
|     1146 | 10523 |  |
|        - | 10524 | `/*` |
|        - | 10525 | ` * Check if the given name refer to an installed class.` |
|        - | 10526 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10527 | ` */` |
|    16738 | 10528 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10529 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10530 | `	const char *zName,  /* Name of the target class */` |
|        - | 10531 | `	sxu32 nByte,        /* zName length */` |
|        - | 10532 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10533 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10534 | `						 */` |
|        - | 10535 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10536 | `	)` |
|        2 | 10537 |  |
|        - | 10538 | `	SyHashEntry *pEntry;` |
|        - | 10539 | `	ph7_class *pClass;` |
|     8369 | 10540 | `	SXUNUSED(iNest);` |
|        - | 10541 | `	/* Exact class lookup.` |
|        - | 10542 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10543 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    16740 | 10544 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    16740 | 10545 | `	if( pEntry == 0 ){` |
|       10 | 10546 | `		return 0;` |
|        - | 10547 | `	}` |
|    16732 | 10548 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    16732 | 10549 | `	if( !iLoadable ){` |
|    15592 | 10550 | `		return pClass;` |
|        - | 10551 | `	}` |
|        - | 10552 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1142 | 10553 | `	while(pClass){` |
|     1142 | 10554 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1142 | 10555 | `			return pClass;` |
|        - | 10556 | `		}` |
|      ! 0 | 10557 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10558 | `	}` |
|      ! 0 | 10559 | `	return 0;` |
|     8371 | 10560 |  |
|        - | 10561 | `/*` |
|        - | 10562 | ` * Reference Table Implementation` |
|        - | 10563 | ` * Status: stable <chm@symisc.net>` |
|        - | 10564 | ` * Intro` |
|        - | 10565 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10566 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10567 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10568 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10569 | ` *  Refer to the official for more information on this powerful` |
|        - | 10570 | ` *  extension.` |
|        - | 10571 | ` */` |
|        - | 10572 | `/*` |
|        - | 10573 | ` * Allocate a new reference entry.` |
|        - | 10574 | ` */` |
|  2996426 | 10575 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10576 |  |
|        - | 10577 | `	VmRefObj *pRef;` |
|        - | 10578 | `	/* Allocate a new instance */` |
|  2996428 | 10579 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2996428 | 10580 | `	if( pRef == 0 ){` |
|      ! 0 | 10581 | `		return 0;` |
|        - | 10582 | `	}` |
|        - | 10583 | `	/* Zero the structure */` |
|  2996428 | 10584 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10585 | `	/* Initialize fields */` |
|  2996428 | 10586 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2996428 | 10587 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2996428 | 10588 | `	pRef->nIdx = nIdx;` |
|  2996428 | 10589 | `	return pRef;` |
|  1498215 | 10590 |  |
|        - | 10591 | `/*` |
|        - | 10592 | ` * Default hash function used by the reference table` |
|        - | 10593 | ` * for lookup/insertion operations.` |
|        - | 10594 | ` */` |
| 16621884 | 10595 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10596 |  |
|        - | 10597 | `	/* Calculate the hash based on the memory object index */` |
| 16621886 | 10598 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10599 |  |
|        - | 10600 | `/*` |
|        - | 10601 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10602 | ` * in the reference table.` |
|        - | 10603 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10604 | ` * otherwise.` |
|        - | 10605 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10606 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10607 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10608 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10609 | ` * Refer to the official for more information on this powerful` |
|        - | 10610 | ` * extension.` |
|        - | 10611 | ` */` |
|  8944504 | 10612 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10613 |  |
|        - | 10614 | `	VmRefObj *pRef;` |
|        - | 10615 | `	sxu32 nBucket;` |
|        - | 10616 | `	/* Point to the appropriate bucket */` |
|  8944506 | 10617 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10618 | `	/* Perform the lookup */` |
|  8944506 | 10619 | `	pRef = pVm->apRefObj[nBucket];` |
| 18835993 | 10620 | `	for(;;){` |
| 37664230 | 10621 | `		if( pRef == 0 ){` |
|  3072600 | 10622 | `			break;` |
|        - | 10623 | `		}` |
| 34591632 | 10624 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10625 | `			/* Entry found */` |
|  5871908 | 10626 | `			return pRef;` |
|        - | 10627 | `		}` |
|        - | 10628 | `		/* Point to the next entry */` |
| 28719726 | 10629 | `		pRef = pRef->pNextCollide;` |
|        2 | 10630 | `	}` |
|        - | 10631 | `	/* No such entry,return NULL */` |
|  3072600 | 10632 | `	return 0;` |
|  4472254 | 10633 |  |
|        - | 10634 | `/*` |
|        - | 10635 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10636 | ` *` |
|        - | 10637 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10638 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10639 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10640 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10641 | ` * Refer to the official for more information on this powerful` |
|        - | 10642 | ` * extension.` |
|        - | 10643 | ` */` |
|  2996426 | 10644 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10645 |  |
|        - | 10646 | `	sxu32 nBucket;` |
|  2996428 | 10647 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10648 | `		VmRefObj **apNew;` |
|        - | 10649 | `		sxu32 nNew;` |
|        - | 10650 | `		/* Allocate a larger table */` |
|     3588 | 10651 | `		nNew = pVm->nRefSize << 1;` |
|     3588 | 10652 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3588 | 10653 | `		if( apNew ){` |
|     3588 | 10654 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10655 | `			sxu32 n;` |
|        - | 10656 | `			/* Zero the structure */` |
|     3588 | 10657 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10658 | `			/* Rehash all referenced entries */` |
|  2835972 | 10659 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10660 | `				/* Remove old collision links */` |
|  2832386 | 10661 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10662 | `				/* Point to the appropriate bucket */` |
|  2832386 | 10663 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10664 | `				/* Insert the entry  */` |
|  2832386 | 10665 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2832386 | 10666 | `				if( apNew[nBucket] ){` |
|  2298896 | 10667 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10668 | `				}` |
|  2832386 | 10669 | `				apNew[nBucket] = pEntry;` |
|        - | 10670 | `				/* Point to the next entry */` |
|  2832386 | 10671 | `				pEntry = pEntry->pNext;` |
|  1416194 | 10672 | `			}` |
|        - | 10673 | `			/* Release the old table */` |
|     3588 | 10674 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10675 | `			/* Install the new one */` |
|     3588 | 10676 | `			pVm->apRefObj = apNew;` |
|     3588 | 10677 | `			pVm->nRefSize = nNew;` |
|     1793 | 10678 | `		}` |
|     1793 | 10679 | `	}` |
|        - | 10680 | `	/* Point to the appropriate bucket */` |
|  2996428 | 10681 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10682 | `	/* Insert the entry */` |
|  2996428 | 10683 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2996428 | 10684 | `	if( pVm->apRefObj[nBucket] ){` |
|  2485292 | 10685 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1242807 | 10686 | `	}` |
|  2996428 | 10687 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2996428 | 10688 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2996428 | 10689 | `	pVm->nRefUsed++;` |
|  2996428 | 10690 | `	return SXRET_OK;` |
|        2 | 10691 |  |
|        - | 10692 | `/*` |
|        - | 10693 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10694 | ` * the reference table.` |
|        - | 10695 | ` * This function is invoked when the user perform an unset` |
|        - | 10696 | ` * call [i.e: unset($var); ].` |
|        - | 10697 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10698 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10699 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10700 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10701 | ` * Refer to the official for more information on this powerful` |
|        - | 10702 | ` * extension.` |
|        - | 10703 | ` */` |
|  2963988 | 10704 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10705 |  |
|        - | 10706 | `	ph7_hashmap_node **apNode;` |
|        - | 10707 | `	SyHashEntry **apEntry;` |
|        - | 10708 | `	sxu32 n;` |
|        - | 10709 | `	/* Point to the reference table */` |
|  2963990 | 10710 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2963990 | 10711 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10712 | `	/* Unlink the entry from the reference table */` |
|  3045124 | 10713 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    81136 | 10714 | `		if( apEntry[n] ){` |
|    81086 | 10715 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    40542 | 10716 | `		}` |
|    40569 | 10717 | `	}` |
|  5848682 | 10718 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2884694 | 10719 | `		if( apNode[n] ){` |
|     5638 | 10720 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2818 | 10721 | `		}` |
|  1442348 | 10722 | `	}` |
|  2963990 | 10723 | `	if( pRef->pPrevCollide ){` |
|  1115420 | 10724 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   558014 | 10725 | `	}else{` |
|  1848572 | 10726 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10727 | `	}` |
|  2963990 | 10728 | `	if( pRef->pNextCollide ){` |
|  1673228 | 10729 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   836797 | 10730 | `	}` |
|  2963990 | 10731 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10732 | `	/* Release the node */` |
|  2963990 | 10733 | `	SySetRelease(&pRef->aReference);` |
|  2963990 | 10734 | `	SySetRelease(&pRef->aArrEntries);` |
|  2963990 | 10735 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2963990 | 10736 | `	pVm->nRefUsed--;` |
|  2963990 | 10737 | `	return SXRET_OK;` |
|        2 | 10738 |  |
|        - | 10739 | `/*` |
|        - | 10740 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10741 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10742 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10743 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10744 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10745 | ` * Refer to the official for more information on this powerful` |
|        - | 10746 | ` * extension.` |
|        - | 10747 | ` */` |
|  3025228 | 10748 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10749 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10750 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10751 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10752 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10753 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10754 | `	)` |
|        2 | 10755 |  |
|  3025230 | 10756 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10757 | `	VmRefObj *pRef;` |
|        - | 10758 | `	/* Check if the referenced object already exists */` |
|  3025230 | 10759 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3025230 | 10760 | `	if( pRef == 0 ){` |
|        - | 10761 | `		/* Create a new entry */` |
|  2996428 | 10762 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2996428 | 10763 | `		if( pRef == 0 ){` |
|      ! 0 | 10764 | `			return SXERR_MEM;` |
|        - | 10765 | `		}` |
|  2996428 | 10766 | `		pRef->iFlags = iFlags;` |
|        - | 10767 | `		/* Install the entry */` |
|  2996428 | 10768 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1498213 | 10769 | `	}` |
|  3025402 | 10770 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10771 | `		/* Safely ignore the exception frame */` |
|      174 | 10772 | `		pFrame = pFrame->pParent;` |
|        2 | 10773 | `	}` |
|  3025230 | 10774 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10775 | `		VmSlot sRef;` |
|        - | 10776 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10777 | `		 * be deleted when we leave this frame.` |
|        - | 10778 | `		 */` |
|    76208 | 10779 | `		sRef.nIdx = nIdx;` |
|    76208 | 10780 | `		sRef.pUserData = pEntry;` |
|    76208 | 10781 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10782 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10783 | `		}` |
|    38103 | 10784 | `	}` |
|  3025230 | 10785 | `	if( pEntry ){` |
|        - | 10786 | `		/* Address of the hash-entry */` |
|   104820 | 10787 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    52409 | 10788 | `	}` |
|  3025230 | 10789 | `	if( pMapEntry ){` |
|        - | 10790 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2915542 | 10791 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1457770 | 10792 | `	}` |
|  3025230 | 10793 | `	return SXRET_OK;` |
|  1512616 | 10794 |  |
|        - | 10795 | `/*` |
|        - | 10796 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10797 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10798 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10799 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10800 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10801 | ` * Refer to the official for more information on this powerful` |
|        - | 10802 | ` * extension.` |
|        - | 10803 | ` */` |
|  2955268 | 10804 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10805 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10806 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10807 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10808 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10809 | `	)` |
|        2 | 10810 |  |
|        - | 10811 | `	VmRefObj *pRef;` |
|        - | 10812 | `	sxu32 n;` |
|        - | 10813 | `	/* Check if the referenced object already exists */` |
|  2955270 | 10814 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2955270 | 10815 | `	if( pRef == 0 ){` |
|        - | 10816 | `		/* Not such entry */` |
|    76154 | 10817 | `		return SXERR_NOTFOUND;` |
|        - | 10818 | `	}` |
|        - | 10819 | `	/* Remove the desired entry */` |
|  2879118 | 10820 | `	if( pEntry ){` |
|        - | 10821 | `		SyHashEntry **apEntry;` |
|       56 | 10822 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10823 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10824 | `			if( apEntry[n] == pEntry ){` |
|        - | 10825 | `				/* Nullify the entry */` |
|       56 | 10826 | `				apEntry[n] = 0;` |
|        - | 10827 | `				/*` |
|        - | 10828 | `				 * NOTE:` |
|        - | 10829 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10830 | `				 * we avoid wasting spaces.` |
|        - | 10831 | `				 */` |
|       27 | 10832 | `			}` |
|       79 | 10833 | `		}` |
|       27 | 10834 | `	}` |
|  2879118 | 10835 | `	if( pMapEntry ){` |
|        - | 10836 | `		ph7_hashmap_node **apNode;` |
|  2879064 | 10837 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5758214 | 10838 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2879152 | 10839 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10840 | `				/* nullify the entry */` |
|  2879064 | 10841 | `				apNode[n] = 0;` |
|  1439531 | 10842 | `			}` |
|  1439577 | 10843 | `		}` |
|  1439531 | 10844 | `	}` |
|  2879118 | 10845 | `	return SXRET_OK;` |
|  1477636 | 10846 |  |
|        - | 10847 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10848 | `/*` |
|        - | 10849 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10850 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10851 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10852 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10853 | ` * For more information on how to register IO stream devices,please` |
|        - | 10854 | ` * refer to the official documentation.` |
|        - | 10855 | ` */` |
|    23090 | 10856 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10857 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10858 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10859 | `	int nByte              /* *pzDevice length*/` |
|        - | 10860 | `	)` |
|        2 | 10861 |  |
|        - | 10862 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10863 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10864 | `	SyString sDev,sCur;` |
|        - | 10865 | `	sxu32 n,nEntry;` |
|        - | 10866 | `	int rc;` |
|        - | 10867 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23092 | 10868 | `	zNext = zCur = zIn = *pzDevice;` |
|    23092 | 10869 | `	zEnd = &zIn[nByte];` |
|  1477646 | 10870 | `	while( zIn < zEnd ){` |
|  1454558 | 10871 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10872 | `			/* Got one */` |
|        3 | 10873 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10874 | `			break;` |
|        - | 10875 | `		}` |
|        - | 10876 | `		/* Advance the cursor */` |
|  1454556 | 10877 | `		zIn++;` |
|        2 | 10878 | `	}` |
|    23092 | 10879 | `	if( zIn >= zEnd ){` |
|        - | 10880 | `		/* No such scheme,return the default stream */` |
|    23090 | 10881 | `		return pVm->pDefStream;` |
|        - | 10882 | `	}` |
|        3 | 10883 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10884 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10885 | `	SyStringFullTrim(&sDev);` |
|        - | 10886 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10887 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10888 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10889 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10890 | `		pStream = apStream[n];` |
|        3 | 10891 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10892 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10893 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10894 | `		if( rc == 0 ){` |
|        - | 10895 | `			/* Stream device found */` |
|        3 | 10896 | `			*pzDevice = zNext;` |
|        3 | 10897 | `			return pStream;` |
|        - | 10898 | `		}` |
|      ! 0 | 10899 | `	}` |
|        - | 10900 | `	/* No such stream,return NULL */` |
|      ! 0 | 10901 | `	return 0;` |
|    11547 | 10902 |  |
|        - | 10903 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10904 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10905 |  |
