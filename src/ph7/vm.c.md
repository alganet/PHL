# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4183/5422 lines (77.15%)

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
|   773006 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   773008 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   772978 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   772970 |    94 | `	return FALSE;` |
|   386527 |    95 |  |
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
|   108042 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   108044 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   108044 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   108044 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   108044 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   108044 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   108044 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   108044 |   260 | `	pFunc->iFlags = iFlags;` |
|   108044 |   261 | `	pFunc->pUserData = pUserData;` |
|   108044 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   108044 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   392092 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   392094 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    33660 |   283 | `		pName = &pFunc->sName;` |
|    16829 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   392094 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   392094 |   287 | `	if( pEntry ){` |
|   304712 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   304712 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   304712 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    87384 |   297 | `	pFunc->pNextName = 0;` |
|    87384 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    87384 |   299 | `	return rc;` |
|   196048 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    31058 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    31060 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    31060 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    31060 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    31030 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    31030 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    31030 |   324 | `	return rc;` |
|    15531 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2877634 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2877636 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2877636 |   342 | `	sInstr.iP1 = iP1;` |
|  2877636 |   343 | `	sInstr.iP2 = iP2;` |
|  2877636 |   344 | `	sInstr.p3  = p3;` |
|  2877636 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   182062 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    91030 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2877636 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2877636 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2877636 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   262560 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   262562 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   262562 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   262562 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   131280 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   131282 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   179434 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   179436 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   179436 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   801138 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   801140 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   170524 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   170526 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   559658 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   559660 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    26148 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    26150 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26150 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    26150 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26150 |   417 | `	return &aInstr[n - 2];` |
|    13076 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    15170 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    15172 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    15172 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    15172 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    15172 |   437 | `	pFrame->pUserData = pUserData;` |
|    15172 |   438 | `	pFrame->pThis = pThis;` |
|    15172 |   439 | `	pFrame->pVm = pVm;` |
|    15172 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    15172 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    15172 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    15172 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    15172 |   444 | `	return pFrame;` |
|     7587 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    15170 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    15172 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15172 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    15172 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    15172 |   466 | `	pVm->pFrame = pFrame;` |
|    15172 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    12622 |   469 | `		*ppFrame = pFrame;` |
|     6310 |   470 | `	}` |
|    15172 |   471 | `	return SXRET_OK;` |
|     7587 |   472 |  |
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
|    12620 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    12622 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12622 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    12622 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12622 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12564 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    89186 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    76624 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    38313 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    12564 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    89242 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    76680 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    38341 |   535 | `			}` |
|     6281 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    12622 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12622 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    12622 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12622 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    12622 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6310 |   544 | `	}` |
|    12622 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6245486 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6245758 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      272 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6245488 |   556 | `	return pFrame;` |
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
|    90826 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|    90828 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   360305 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   224066 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   224066 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    90828 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    46960 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    43870 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|      286 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      286 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|      142 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    43870 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   424244 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   358442 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   358442 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   358436 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   358436 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   179217 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    43870 |   738 | `	pClass->bMounted = TRUE;` |
|    43870 |   739 | `	return SXRET_OK;` |
|    45415 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1136 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1138 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1138 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4724 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3588 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3588 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3588 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3588 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3588 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3582 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3582 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3582 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3582 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1176 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      587 |   777 | `			}` |
|     3582 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3582 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3582 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1792 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1138 |   800 | `	return SXRET_OK;` |
|      570 |   801 |  |
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
|   311440 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   311442 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   303792 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   151895 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   311442 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   311442 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   311442 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   311442 |   830 | `	return pObj;` |
|   155722 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2141142 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2141144 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2141144 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1070571 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2141144 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2141144 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2141144 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2141144 |   853 | `	return pObj;` |
|  1070573 |   854 |  |
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
|    31246 |  1351 | `static ph7_value * VmNewOperandStack(` |
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
|    31248 |  1364 | `	nInstr += VM_STACK_GUARD;` |
|    31248 |  1365 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    31248 |  1366 | `	if( pStack == 0 ){` |
|      ! 0 |  1367 | `		return 0;` |
|        - |  1368 | `	}` |
|        - |  1369 | `	/* Initialize the operand stack */` |
|  1985822 |  1370 | `	while( nInstr > 0 ){` |
|  1954576 |  1371 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1954576 |  1372 | `		--nInstr;` |
|        2 |  1373 | `	}` |
|        - |  1374 | `	/* Ready for bytecode execution */` |
|    31248 |  1375 | `	return pStack;` |
|    15625 |  1376 |  |
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
|   557186 |  1489 | `static sxi32 VmInitCallContext(` |
|        - |  1490 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1491 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1492 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1493 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1494 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1495 | `	)` |
|        2 |  1496 |  |
|   557188 |  1497 | `	pOut->pFunc = pFunc;` |
|   557188 |  1498 | `	pOut->pVm   = pVm;` |
|   557188 |  1499 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   557188 |  1500 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1501 | `	/* Assume a null return value */` |
|   557188 |  1502 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   557188 |  1503 | `	pOut->pRet = pRet;` |
|   557188 |  1504 | `	pOut->iFlags = iFlags;` |
|   557188 |  1505 | `	return SXRET_OK;` |
|        2 |  1506 |  |
|        - |  1507 | `/*` |
|        - |  1508 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1509 | ` * left behind.` |
|        - |  1510 | ` */` |
|   557186 |  1511 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1512 |  |
|        - |  1513 | `	sxu32 n;` |
|   557188 |  1514 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6750 |  1515 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19266 |  1516 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12518 |  1517 | `			if( apObj[n] == 0 ){` |
|        - |  1518 | `				/* Already released */` |
|      250 |  1519 | `				continue;` |
|        - |  1520 | `			}` |
|    12270 |  1521 | `			PH7_MemObjRelease(apObj[n]);` |
|    12270 |  1522 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6136 |  1523 | `		}` |
|     6750 |  1524 | `		SySetRelease(&pCtx->sVar);` |
|     3374 |  1525 | `	}` |
|   557188 |  1526 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   557188 |  1542 |  |
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
|  3267594 |  1573 | `static void VmPopOperand(` |
|        - |  1574 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1575 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1576 | `	)` |
|        2 |  1577 |  |
|  3267596 |  1578 | `	ph7_value *pTos = *ppTos;` |
|  6938208 |  1579 | `	while( nPop > 0 ){` |
|  3670614 |  1580 | `		PH7_MemObjRelease(pTos);` |
|  3670614 |  1581 | `		pTos--;` |
|  3670614 |  1582 | `		nPop--;` |
|        2 |  1583 | `	}` |
|        - |  1584 | `	/* Top of the stack */` |
|  3267596 |  1585 | `	*ppTos = pTos;` |
|  3267596 |  1586 |  |
|        - |  1587 | `/*` |
|        - |  1588 | ` * Reserve a memory object.` |
|        - |  1589 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1590 | ` */` |
|  3003088 |  1591 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1592 |  |
|  3003090 |  1593 | `	ph7_value *pObj = 0;` |
|        - |  1594 | `	VmSlot *pSlot;` |
|        - |  1595 | `	sxu32 nIdx;` |
|        - |  1596 | `	/* Check for a free slot */` |
|  3003090 |  1597 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3003090 |  1598 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3003090 |  1599 | `	if( pSlot ){` |
|   861948 |  1600 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   861948 |  1601 | `		nIdx = pSlot->nIdx;` |
|   430973 |  1602 | `	}` |
|  3003090 |  1603 | `	if( pObj == 0 ){` |
|        - |  1604 | `		/* Reserve a new memory object */` |
|  2141144 |  1605 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2141144 |  1606 | `		if( pObj == 0 ){` |
|      ! 0 |  1607 | `			return 0;` |
|        - |  1608 | `		}` |
|  1070571 |  1609 | `	}` |
|        - |  1610 | `	/* Set a null default value */` |
|  3003090 |  1611 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3003090 |  1612 | `	pObj->nIdx = nIdx;` |
|  3003090 |  1613 | `	return pObj;` |
|  1501546 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1617 | ` */` |
|    29344 |  1618 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1619 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1620 | `	const char *zKey,  /* Entry key */` |
|        - |  1621 | `	sxu32 nByte,       /* Key length */` |
|        - |  1622 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1623 | `	)` |
|        2 |  1624 |  |
|        - |  1625 | `	ph7_value sKey;` |
|        - |  1626 | `	sxi32 rc;` |
|    29346 |  1627 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    29346 |  1628 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1629 | `	/* Perform the insertion */` |
|    29346 |  1630 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    29346 |  1631 | `	PH7_MemObjRelease(&sKey);` |
|    29346 |  1632 | `	return rc;` |
|        2 |  1633 |  |
|        - |  1634 | `/*` |
|        - |  1635 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1636 | ` * Return a pointer to the variable value on success.` |
|        - |  1637 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1638 | ` */` |
|  3055880 |  1639 | `static ph7_value * VmExtractMemObj(` |
|        - |  1640 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1641 | `	const SyString *pName, /* Variable name */` |
|        - |  1642 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1643 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1644 | `	)` |
|        2 |  1645 |  |
|  3055882 |  1646 | `	int bNullify = FALSE;` |
|        - |  1647 | `	SyHashEntry *pEntry;` |
|        - |  1648 | `	VmFrame *pFrame;` |
|        - |  1649 | `	ph7_value *pObj;` |
|        - |  1650 | `	sxu32 nIdx;` |
|        - |  1651 | `	sxi32 rc;` |
|        - |  1652 | `	/* Point to the top active frame */` |
|  3055882 |  1653 | `	pFrame = pVm->pFrame;` |
|  3055882 |  1654 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1655 | `	/* Perform the lookup */` |
|  3055882 |  1656 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1657 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1658 | `		pName = &sAnnon;` |
|        - |  1659 | `		/* Always nullify the object */` |
|      ! 0 |  1660 | `		bNullify = TRUE;` |
|      ! 0 |  1661 | `		bDup = FALSE;` |
|      ! 0 |  1662 | `	}` |
|        - |  1663 | `	/* Check the superglobals table first */` |
|  3055882 |  1664 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3055882 |  1665 | `	if( pEntry == 0 ){` |
|        - |  1666 | `		/* Query the top active frame */` |
|  3055846 |  1667 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3055846 |  1668 | `		if( pEntry == 0 ){` |
|    83096 |  1669 | `			char *zName = (char *)pName->zString;` |
|        - |  1670 | `			VmSlot sLocal;` |
|    83096 |  1671 | `			if( !bCreate ){` |
|        - |  1672 | `				/* Do not create the variable,return NULL instead */` |
|       36 |  1673 | `				return 0;` |
|        - |  1674 | `			}` |
|        - |  1675 | `			/* No such variable,automatically create a new one and install` |
|        - |  1676 | `			 * it in the current frame.` |
|        - |  1677 | `			 */` |
|    83062 |  1678 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    83062 |  1679 | `			if( pObj == 0 ){` |
|      ! 0 |  1680 | `				return 0;` |
|        - |  1681 | `			}` |
|    83062 |  1682 | `			nIdx = pObj->nIdx;` |
|    83062 |  1683 | `			if( bDup ){` |
|        - |  1684 | `				/* Duplicate name */` |
|      164 |  1685 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1686 | `				if( zName == 0 ){` |
|      ! 0 |  1687 | `					return 0;` |
|        - |  1688 | `				}` |
|       81 |  1689 | `			}` |
|        - |  1690 | `			/* Link to the top active VM frame */` |
|    83062 |  1691 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    83062 |  1692 | `			if( rc != SXRET_OK ){` |
|        - |  1693 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1694 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1695 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1696 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1697 | `				return 0;` |
|        - |  1698 | `			}` |
|    83062 |  1699 | `			if( pFrame->pParent != 0 ){` |
|        - |  1700 | `				/* Local variable */` |
|    76624 |  1701 | `				sLocal.nIdx = nIdx;` |
|    76624 |  1702 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    38313 |  1703 | `			}else{` |
|        - |  1704 | `				/* Register in the $GLOBALS array */` |
|     6440 |  1705 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1706 | `			}` |
|        - |  1707 | `			/* Install in the reference table */` |
|    83062 |  1708 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1709 | `			/* Save object index */` |
|    83062 |  1710 | `			pObj->nIdx = nIdx;` |
|    41532 |  1711 | `		}else{` |
|        - |  1712 | `			/* Extract variable contents */` |
|  2972752 |  1713 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2972752 |  1714 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2972752 |  1715 | `			if( bNullify && pObj ){` |
|      ! 0 |  1716 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1717 | `			}` |
|        - |  1718 | `		}` |
|  1528017 |  1719 | `	}else{` |
|        - |  1720 | `		/* Superglobal */` |
|       38 |  1721 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1722 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1723 | `	}` |
|  3055848 |  1724 | `	return pObj;` |
|  1528052 |  1725 |  |
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
|    31246 |  2557 | `static sxi32 VmByteCodeExec(` |
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
|    31248 |  2574 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    31248 |  2575 | `	if( nTos < 0 ){` |
|    29504 |  2576 | `		pTos = &pStack[-1];` |
|    14753 |  2577 | `	}else{` |
|     1746 |  2578 | `		pTos = &pStack[nTos];` |
|        - |  2579 | `	}` |
|    31248 |  2580 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    31248 |  2581 | `	pc = 0;` |
|        - |  2582 | `	/* Execute as much as we can */` |
|  4890697 |  2583 | `	for(;;){` |
|        - |  2584 | `		/* Fetch the instruction to execute */` |
|  9780692 |  2585 | `		pInstr = &aInstr[pc];` |
|  9780692 |  2586 | `		rc = SXRET_OK;` |
|        - |  2587 | `/*` |
|        - |  2588 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2589 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2590 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2591 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2592 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2593 | ` */` |
|  9780692 |  2594 | `		switch(pInstr->iOp){` |
|        - |  2595 | `/*` |
|        - |  2596 | ` * DONE: P1 * *` |
|        - |  2597 | ` *` |
|        - |  2598 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2599 | ` * and return immediately.` |
|        - |  2600 | ` */` |
|    15380 |  2601 | `case PH7_OP_DONE:` |
|    30762 |  2602 | `	if( pInstr->iP1 ){` |
|        - |  2603 | `#ifdef UNTRUST` |
|        - |  2604 | `		if( pTos < pStack ){` |
|        - |  2605 | `			goto Abort;` |
|        - |  2606 | `		}` |
|        - |  2607 | `#endif` |
|    17748 |  2608 | `		if( pLastRef ){` |
|    11572 |  2609 | `			*pLastRef = pTos->nIdx;` |
|     5785 |  2610 | `		}` |
|    17748 |  2611 | `		if( pResult ){` |
|        - |  2612 | `			/* Execution result */` |
|    16894 |  2613 | `			PH7_MemObjStore(pTos,pResult);` |
|     8446 |  2614 | `		}` |
|    17748 |  2615 | `		VmPopOperand(&pTos,1);` |
|    21889 |  2616 | `	}else if( pLastRef ){` |
|        - |  2617 | `		/* Nothing referenced */` |
|      958 |  2618 | `		*pLastRef = SXU32_HIGH;` |
|      478 |  2619 | `	}` |
|        - |  2620 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2621 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2622 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2623 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2624 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2625 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2626 | `	 * block can override it.` |
|        - |  2627 | `	 */` |
|    30764 |  2628 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    30762 |  2643 | `	goto Done;` |
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
|   210927 |  2691 | `case PH7_OP_JMP:` |
|   421900 |  2692 | `	pc = pInstr->iP2 - 1;` |
|   421900 |  2693 | `	break;` |
|        - |  2694 | `/*` |
|        - |  2695 | ` * JZ: P1 P2 *` |
|        - |  2696 | ` *` |
|        - |  2697 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2698 | ` * entry in the stack if P1 is zero.` |
|        - |  2699 | ` */` |
|   492268 |  2700 | `case PH7_OP_JZ:` |
|        - |  2701 | `#ifdef UNTRUST` |
|        - |  2702 | `	if( pTos < pStack ){` |
|        - |  2703 | `		goto Abort;` |
|        - |  2704 | `	}` |
|        - |  2705 | `#endif` |
|        - |  2706 | `	/* Get a boolean value */` |
|   984626 |  2707 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2708 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2709 | `	}` |
|   984626 |  2710 | `	if( !pTos->x.iVal ){` |
|        - |  2711 | `		/* Take the jump */` |
|   495926 |  2712 | `		pc = pInstr->iP2 - 1;` |
|   247962 |  2713 | `	}` |
|   984626 |  2714 | `	if( !pInstr->iP1 ){` |
|   785194 |  2715 | `		VmPopOperand(&pTos,1);` |
|   392618 |  2716 | `	}` |
|   984626 |  2717 | `	break;` |
|        - |  2718 | `/*` |
|        - |  2719 | ` * JNZ: P1 P2 *` |
|        - |  2720 | ` *` |
|        - |  2721 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2722 | ` * entry in the stack if P1 is zero.` |
|        - |  2723 | ` */` |
|    53368 |  2724 | `case PH7_OP_JNZ:` |
|        - |  2725 | `#ifdef UNTRUST` |
|        - |  2726 | `	if( pTos < pStack ){` |
|        - |  2727 | `		goto Abort;` |
|        - |  2728 | `	}` |
|        - |  2729 | `#endif` |
|        - |  2730 | `	/* Get a boolean value */` |
|   106738 |  2731 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2732 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2733 | `	}` |
|   106738 |  2734 | `	if( pTos->x.iVal ){` |
|        - |  2735 | `		/* Take the jump */` |
|     4368 |  2736 | `		pc = pInstr->iP2 - 1;` |
|     2183 |  2737 | `	}` |
|   106738 |  2738 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2739 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2740 | `	}` |
|   106738 |  2741 | `	break;` |
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
|   384881 |  2755 | `case PH7_OP_POP: {` |
|   769808 |  2756 | `	sxi32 n = pInstr->iP1;` |
|   769808 |  2757 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2758 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2759 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2760 | `	}` |
|   769808 |  2761 | `	VmPopOperand(&pTos,n);` |
|   769808 |  2762 | `	break;` |
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
|     6269 |  2785 | `case PH7_OP_NSSWITCH:` |
|    12540 |  2786 | `	SyBlobReset(&pVm->sNamespace);` |
|    12540 |  2787 | `	if( pInstr->p3 ){` |
|       51 |  2788 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2789 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2790 | `	}` |
|    12540 |  2791 | `	break;` |
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
|    12519 |  2923 | `case PH7_OP_ERR_CTRL:` |
|        - |  2924 | `	/*` |
|        - |  2925 | `	 * TICKET 1433-038:` |
|        - |  2926 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2927 | `	 * use the public API,to control error output.` |
|        - |  2928 | `	 */` |
|    25038 |  2929 | `	break;` |
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
|   812427 |  2989 | `case PH7_OP_LOADC: {` |
|        - |  2990 | `	ph7_value *pObj;` |
|        - |  2991 | `	/* Reserve a room */` |
|  1624900 |  2992 | `	pTos++;` |
|  2429379 |  2993 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1624900 |  2994 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2995 | `			SyHashEntry *pEntry;` |
|        - |  2996 | `			/* Candidate for expansion via user defined callbacks */` |
|    15988 |  2997 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15988 |  2998 | `			if( pEntry ){` |
|    15984 |  2999 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3000 | `				/* Set a NULL default value */` |
|    15984 |  3001 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15984 |  3002 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3003 | `				/* Invoke the callback and deal with the expanded value */` |
|    15984 |  3004 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3005 | `				/* Mark as constant */` |
|    15984 |  3006 | `				pTos->nIdx = SXU32_HIGH;` |
|    15984 |  3007 | `				break;` |
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
|  1608916 |  3039 | `		PH7_MemObjLoad(pObj,pTos);` |
|   804481 |  3040 | `	}else{` |
|        - |  3041 | `		/* Set a NULL value */` |
|      ! 0 |  3042 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3043 | `	}` |
|   804436 |  3044 | `LoadC_Done:` |
|        - |  3045 | `	/* Mark as constant */` |
|  1608918 |  3046 | `	pTos->nIdx = SXU32_HIGH;` |
|  1608918 |  3047 | `	break;` |
|        - |  3048 | `				  }` |
|        - |  3049 | `/*` |
|        - |  3050 | ` * LOAD: P1 * P3` |
|        - |  3051 | ` *` |
|        - |  3052 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3053 | ` * from the P3 operand.` |
|        - |  3054 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3055 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3056 | ` */` |
|  1331761 |  3057 | `case PH7_OP_LOAD:{` |
|        - |  3058 | `	ph7_value *pObj;` |
|        - |  3059 | `	SyString sName;` |
|  2663744 |  3060 | `	if( pInstr->p3 == 0 ){` |
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
|  2663726 |  3073 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3074 | `		/* Reserve a room for the target object */` |
|  2663726 |  3075 | `		pTos++;` |
|        - |  3076 | `	}` |
|        - |  3077 | `	/* Extract the requested memory object */` |
|  2663744 |  3078 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2663744 |  3079 | `	if( pObj == 0 ){` |
|       26 |  3080 | `		if( pInstr->iP1 ){` |
|        - |  3081 | `			/* Variable not found,load NULL */` |
|       26 |  3082 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3083 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3084 | `			}else{` |
|       26 |  3085 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3086 | `			}` |
|       26 |  3087 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1331775 |  3088 | `			break;` |
|      ! 0 |  3089 | `		}else{` |
|        - |  3090 | `			/* Fatal error */` |
|      ! 0 |  3091 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3092 | `			goto Abort;` |
|        - |  3093 | `		}` |
|        - |  3094 | `	}` |
|        - |  3095 | `	/* Load variable contents */` |
|  2663720 |  3096 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2663720 |  3097 | `	pTos->nIdx = pObj->nIdx;` |
|  2663720 |  3098 | `	break;` |
|        - |  3099 | `				   }` |
|        - |  3100 | `/*` |
|        - |  3101 | ` * LOAD_MAP P1 * *` |
|        - |  3102 | ` *` |
|        - |  3103 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3104 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3105 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3106 | ` */` |
|    18085 |  3107 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3108 | `	ph7_hashmap *pMap;` |
|        - |  3109 | `	/* Allocate a new hashmap instance */` |
|    36172 |  3110 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    36172 |  3111 | `	if( pMap == 0 ){` |
|      ! 0 |  3112 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3113 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3114 | `		goto Abort;` |
|        - |  3115 | `	}` |
|    36172 |  3116 | `	if( pInstr->iP1 > 0 ){` |
|     2238 |  3117 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3118 | `		/* Perform the insertion */` |
|     6838 |  3119 | `		while( pEntry < pTos ){` |
|     4602 |  3120 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3121 | `				/* Insertion by reference */` |
|      142 |  3122 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3123 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3124 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3125 | `					);` |
|       48 |  3126 | `			}else{` |
|        - |  3127 | `				/* Standard insertion */` |
|     6761 |  3128 | `				PH7_HashmapInsert(pMap,` |
|     4506 |  3129 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2253 |  3130 | `					&pEntry[1]` |
|        - |  3131 | `				);` |
|        - |  3132 | `			}` |
|        - |  3133 | `			/* Next pair on the stack */` |
|     4602 |  3134 | `			pEntry += 2;` |
|        2 |  3135 | `		}` |
|        - |  3136 | `		/* Pop P1 elements */` |
|     2238 |  3137 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1118 |  3138 | `	}` |
|        - |  3139 | `	/* Push the hashmap */` |
|    36172 |  3140 | `	pTos++;` |
|    36172 |  3141 | `	pTos->nIdx = SXU32_HIGH;` |
|    36172 |  3142 | `	pTos->x.pOther = pMap;` |
|    36172 |  3143 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    36172 |  3144 | `	break;` |
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
|   215327 |  3200 | `case PH7_OP_LOAD_IDX: {` |
|   430700 |  3201 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   430700 |  3202 | `	ph7_hashmap *pMap = 0;` |
|        - |  3203 | `	ph7_value *pIdx;` |
|   430700 |  3204 | `	pIdx = 0;` |
|   430700 |  3205 | `	if( pInstr->iP1 == 0 ){` |
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
|   430700 |  3222 | `		pIdx = pTos;` |
|   430700 |  3223 | `		pTos--;` |
|        - |  3224 | `	}` |
|   430700 |  3225 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
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
|    89780 |  3250 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3251 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3252 | `			ph7_value *pObj;` |
|      ! 0 |  3253 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3254 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3255 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3256 | `			}` |
|      ! 0 |  3257 | `		}` |
|      ! 0 |  3258 | `	}` |
|    89780 |  3259 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    89780 |  3260 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    89780 |  3261 | `		if( pInstr->iP2 ){` |
|        - |  3262 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3263 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3264 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3265 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3266 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3267 | `		}` |
|        - |  3268 | `		/* Point to the hashmap */` |
|    89780 |  3269 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    89780 |  3270 | `		if( pIdx ){` |
|        - |  3271 | `			/* Load the desired entry */` |
|    89780 |  3272 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    44889 |  3273 | `		}` |
|    89780 |  3274 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3275 | `			/* Create a new empty entry */` |
|      265 |  3276 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3277 | `			if( rc == SXRET_OK ){` |
|        - |  3278 | `				/* Point to the last inserted entry */` |
|      265 |  3279 | `				pNode = pMap->pLast;` |
|      132 |  3280 | `			}` |
|      132 |  3281 | `		}` |
|    44889 |  3282 | `	}` |
|    89780 |  3283 | `	if( pIdx ){` |
|    89780 |  3284 | `		PH7_MemObjRelease(pIdx);` |
|    44889 |  3285 | `	}` |
|    89780 |  3286 | `	if( rc == SXRET_OK ){` |
|        - |  3287 | `		/* Load entry contents */` |
|    41124 |  3288 | `		if( pMap->iRef < 2 ){` |
|        - |  3289 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3290 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3291 | `			 */` |
|       24 |  3292 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3293 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3294 | `		}else{` |
|    41102 |  3295 | `			pTos->nIdx = pNode->nValIdx;` |
|    41102 |  3296 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    41102 |  3297 | `			PH7_HashmapUnref(pMap);` |
|        - |  3298 | `		}` |
|    20563 |  3299 | `	}else{` |
|        - |  3300 | `		/* No such entry,load NULL */` |
|    48658 |  3301 | `		PH7_MemObjRelease(pTos);` |
|    48658 |  3302 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3303 | `	}` |
|    89780 |  3304 | `	break;` |
|        - |  3305 | `					  }` |
|        - |  3306 | `/*` |
|        - |  3307 | ` * LOAD_CLOSURE * * P3` |
|        - |  3308 | ` *` |
|        - |  3309 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3310 | ` * name in the stack.` |
|        - |  3311 | ` */` |
|        3 |  3312 | `case PH7_OP_LOAD_CLOSURE:{` |
|        7 |  3313 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        7 |  3314 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3315 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3316 | `		ph7_vm_func *pClosure;` |
|        - |  3317 | `		char *zName;` |
|        - |  3318 | `		sxu32 mLen;` |
|        - |  3319 | `		sxu32 n;` |
|        - |  3320 | `		/* Create a new VM function */` |
|        7 |  3321 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3322 | `		/* Generate an unique closure name */` |
|        7 |  3323 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        7 |  3324 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3325 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3326 | `			goto Abort;` |
|        - |  3327 | `		}` |
|        7 |  3328 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        7 |  3329 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3330 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3331 | `		}` |
|        - |  3332 | `		/* Zero the stucture */` |
|        7 |  3333 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3334 | `		/* Perform a structure assignment on read-only items */` |
|        7 |  3335 | `		pClosure->aArgs = pFunc->aArgs;` |
|        7 |  3336 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        7 |  3337 | `		pClosure->aStatic = pFunc->aStatic;` |
|        7 |  3338 | `		pClosure->iFlags = pFunc->iFlags;` |
|        7 |  3339 | `		pClosure->pUserData = pFunc->pUserData;` |
|        7 |  3340 | `		pClosure->sSignature = pFunc->sSignature;` |
|        7 |  3341 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3342 | `		/* Register the closure */` |
|        7 |  3343 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3344 | `		/* Set up closure environment */` |
|        7 |  3345 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        7 |  3346 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       19 |  3347 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3348 | `			ph7_value *pValue;` |
|       13 |  3349 | `			pEnv = &aEnv[n];` |
|       13 |  3350 | `			sEnv.sName  = pEnv->sName;` |
|       13 |  3351 | `			sEnv.iFlags = pEnv->iFlags;` |
|       13 |  3352 | `			sEnv.nIdx = SXU32_HIGH;` |
|       13 |  3353 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       13 |  3354 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3355 | `				/* Pass by reference */` |
|      ! 0 |  3356 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3357 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3358 | `					);` |
|      ! 0 |  3359 | `			}` |
|        - |  3360 | `			/* Standard pass by value */` |
|       13 |  3361 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       13 |  3362 | `			if( pValue ){` |
|        - |  3363 | `				/* Copy imported value */` |
|        7 |  3364 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        3 |  3365 | `			}` |
|        - |  3366 | `			/* Insert the imported variable */` |
|       13 |  3367 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        7 |  3368 | `		}` |
|        - |  3369 | `		/* Finally,load the closure name on the stack */` |
|        7 |  3370 | `		pTos++;` |
|        7 |  3371 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        3 |  3372 | `	}` |
|        7 |  3373 | `	break;` |
|        - |  3374 | `						 }` |
|        - |  3375 | `/*` |
|        - |  3376 | ` * STORE * P2 P3` |
|        - |  3377 | ` *` |
|        - |  3378 | ` * Perform a store (Assignment) operation.` |
|        - |  3379 | ` */` |
|   111384 |  3380 | `case PH7_OP_STORE: {` |
|        - |  3381 | `	ph7_value *pObj;` |
|        - |  3382 | `	SyString sName;` |
|        - |  3383 | `#ifdef UNTRUST` |
|        - |  3384 | `	if( pTos < pStack ){` |
|        - |  3385 | `		goto Abort;` |
|        - |  3386 | `	}` |
|        - |  3387 | `#endif` |
|   222770 |  3388 | `	if( pInstr->iP2 ){` |
|        - |  3389 | `		sxu32 nIdx;` |
|        - |  3390 | `		/* Member store operation */` |
|     2924 |  3391 | `		nIdx = pTos->nIdx;` |
|     2924 |  3392 | `		VmPopOperand(&pTos,1);` |
|     2924 |  3393 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3394 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3395 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3396 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3397 | `		}else{` |
|        - |  3398 | `			/* Point to the desired memory object */` |
|     2920 |  3399 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2920 |  3400 | `			if( pObj ){` |
|        - |  3401 | `				/* Perform the store operation */` |
|     2920 |  3402 | `				PH7_MemObjStore(pTos,pObj);` |
|     1459 |  3403 | `			}` |
|        - |  3404 | `		}` |
|   112847 |  3405 | `		break;` |
|   219848 |  3406 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3407 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3408 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3409 | `			/* Force a string cast */` |
|      ! 0 |  3410 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3411 | `		}` |
|        7 |  3412 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3413 | `		pTos--;` |
|        - |  3414 | `#ifdef UNTRUST` |
|        - |  3415 | `		if( pTos < pStack  ){` |
|        - |  3416 | `			goto Abort;` |
|        - |  3417 | `		}` |
|        - |  3418 | `#endif` |
|        4 |  3419 | `	}else{` |
|   219842 |  3420 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3421 | `	}` |
|        - |  3422 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   219848 |  3423 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   219848 |  3424 | `	if( pObj == 0 ){` |
|      ! 0 |  3425 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3426 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3427 | `		goto Abort;` |
|        - |  3428 | `	}` |
|   219848 |  3429 | `	if( !pInstr->p3 ){` |
|        7 |  3430 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3431 | `	}` |
|        - |  3432 | `	/* Perform the store operation */` |
|   219848 |  3433 | `	PH7_MemObjStore(pTos,pObj);` |
|   219848 |  3434 | `	break;` |
|        - |  3435 | `				   }` |
|        - |  3436 | `/*` |
|        - |  3437 | ` * STORE_IDX:   P1 * P3` |
|        - |  3438 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3439 | ` *` |
|        - |  3440 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3441 | ` */` |
|    80574 |  3442 | `case PH7_OP_STORE_IDX:` |
|        - |  3443 | `case PH7_OP_STORE_IDX_REF: {` |
|   161150 |  3444 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3445 | `	ph7_value *pKey;` |
|        - |  3446 | `	sxu32 nIdx;` |
|   161150 |  3447 | `	if( pInstr->iP1 ){` |
|        - |  3448 | `		/* Key is next on stack */` |
|    57066 |  3449 | `		pKey = pTos;` |
|    57066 |  3450 | `		pTos--;` |
|    28534 |  3451 | `	}else{` |
|   104086 |  3452 | `		pKey = 0;` |
|        - |  3453 | `	}` |
|   161150 |  3454 | `	nIdx = pTos->nIdx;` |
|   161150 |  3455 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3456 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3457 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3458 | `		 * checking true sharing count, then re-add after separation. */` |
|   161098 |  3459 | `		if( nIdx != SXU32_HIGH ){` |
|   161098 |  3460 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   241646 |  3461 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   161098 |  3462 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3463 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3464 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3465 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3466 | `				 * refcounts if the backing array was already separated. */` |
|   161098 |  3467 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   161098 |  3468 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   161098 |  3469 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   161098 |  3470 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   161098 |  3471 | `					pTos->x.pOther = pMap;` |
|    80550 |  3472 | `				}else{` |
|        - |  3473 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3474 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3475 | `					pMap = pCur;` |
|        - |  3476 | `				}` |
|    80550 |  3477 | `			}else{` |
|      ! 0 |  3478 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3479 | `			}` |
|    80550 |  3480 | `		}else{` |
|      ! 0 |  3481 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3482 | `		}` |
|   161098 |  3483 | `		if( pMap->iRef < 2 ){` |
|        - |  3484 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3485 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3486 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3487 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3488 | `			pMap->iRef = 2;` |
|      ! 0 |  3489 | `		}` |
|    80550 |  3490 | `	}else{` |
|        - |  3491 | `		ph7_value *pObj;` |
|       53 |  3492 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3493 | `		if( pObj == 0 ){` |
|      ! 0 |  3494 | `			if( pKey ){` |
|      ! 0 |  3495 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3496 | `			}` |
|      ! 0 |  3497 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3498 | `			break;` |
|        - |  3499 | `		}` |
|        - |  3500 | `		/* Phase#1: Load the array */` |
|       53 |  3501 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3502 | `			VmPopOperand(&pTos,1);` |
|       53 |  3503 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3504 | `				/* Force a string cast */` |
|      ! 0 |  3505 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3506 | `			}` |
|       53 |  3507 | `			if( pKey == 0 ){` |
|        - |  3508 | `				/* Append string */` |
|        3 |  3509 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3510 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3511 | `				}` |
|        2 |  3512 | `			}else{` |
|        - |  3513 | `				sxu32 nOfft;` |
|       51 |  3514 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3515 | `					/* Force an int cast */` |
|       51 |  3516 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3517 | `				}` |
|       51 |  3518 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3519 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3520 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3521 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3522 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3523 | `				}else{` |
|      ! 0 |  3524 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3525 | `						/* Perform an append operation */` |
|      ! 0 |  3526 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3527 | `					}` |
|        - |  3528 | `				}` |
|        - |  3529 | `			}` |
|       53 |  3530 | `			if( pKey ){` |
|       51 |  3531 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3532 | `			}` |
|       53 |  3533 | `			break;` |
|      ! 0 |  3534 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3535 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3536 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3537 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3538 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3539 | `				goto Abort;` |
|        - |  3540 | `			}` |
|      ! 0 |  3541 | `		}` |
|        - |  3542 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3543 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3544 | `	}` |
|   161098 |  3545 | `	VmPopOperand(&pTos,1);` |
|        - |  3546 | `	/* Phase#2: Perform the insertion */` |
|   161098 |  3547 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3548 | `		/* Insertion by reference */` |
|       15 |  3549 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3550 | `	}else{` |
|   161084 |  3551 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3552 | `	}` |
|   161098 |  3553 | `	if( pKey ){` |
|    57016 |  3554 | `		PH7_MemObjRelease(pKey);` |
|    28507 |  3555 | `	}` |
|   161098 |  3556 | `	break;` |
|        - |  3557 | `					   }` |
|        - |  3558 | `/*` |
|        - |  3559 | ` * INCR: P1 * *` |
|        - |  3560 | ` *` |
|        - |  3561 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3562 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3563 | ` * the stack and increment after that.` |
|        - |  3564 | ` */` |
|   151303 |  3565 | `case PH7_OP_INCR:` |
|        - |  3566 | `#ifdef UNTRUST` |
|        - |  3567 | `	if( pTos < pStack ){` |
|        - |  3568 | `		goto Abort;` |
|        - |  3569 | `	}` |
|        - |  3570 | `#endif` |
|   302652 |  3571 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302652 |  3572 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3573 | `			ph7_value *pObj;` |
|   302652 |  3574 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3575 | `				/* Force a numeric cast */` |
|   302652 |  3576 | `				PH7_MemObjToNumeric(pObj);` |
|   302652 |  3577 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3578 | `					pObj->rVal++;` |
|        - |  3579 | `					/* Try to get an integer representation */` |
|      ! 0 |  3580 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3581 | `				}else{` |
|   302652 |  3582 | `					pObj->x.iVal++;` |
|   302652 |  3583 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3584 | `				}` |
|   302652 |  3585 | `				if( pInstr->iP1 ){` |
|        - |  3586 | `					/* Pre-icrement */` |
|       71 |  3587 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3588 | `				}` |
|   151347 |  3589 | `			}` |
|   151349 |  3590 | `		}else{` |
|      ! 0 |  3591 | `			if( pInstr->iP1 ){` |
|        - |  3592 | `				/* Force a numeric cast */` |
|      ! 0 |  3593 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3594 | `				/* Pre-increment */` |
|      ! 0 |  3595 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3596 | `					pTos->rVal++;` |
|        - |  3597 | `					/* Try to get an integer representation */` |
|      ! 0 |  3598 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3599 | `				}else{` |
|      ! 0 |  3600 | `					pTos->x.iVal++;` |
|      ! 0 |  3601 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3602 | `				}` |
|      ! 0 |  3603 | `			}` |
|        - |  3604 | `		}` |
|   151347 |  3605 | `	}` |
|   302652 |  3606 | `	break;` |
|        - |  3607 | `/*` |
|        - |  3608 | ` * DECR: P1 * *` |
|        - |  3609 | ` *` |
|        - |  3610 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3611 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3612 | ` * and decrement after that.` |
|        - |  3613 | ` */` |
|        2 |  3614 | `case PH7_OP_DECR:` |
|        - |  3615 | `#ifdef UNTRUST` |
|        - |  3616 | `	if( pTos < pStack ){` |
|        - |  3617 | `		goto Abort;` |
|        - |  3618 | `	}` |
|        - |  3619 | `#endif` |
|        5 |  3620 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3621 | `		/* Force a numeric cast */` |
|        5 |  3622 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3623 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3624 | `			ph7_value *pObj;` |
|        5 |  3625 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3626 | `				/* Force a numeric cast */` |
|        5 |  3627 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3628 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3629 | `					pObj->rVal--;` |
|        - |  3630 | `					/* Try to get an integer representation */` |
|      ! 0 |  3631 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3632 | `				}else{` |
|        5 |  3633 | `					pObj->x.iVal--;` |
|        5 |  3634 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3635 | `				}` |
|        5 |  3636 | `				if( pInstr->iP1 ){` |
|        - |  3637 | `					/* Pre-icrement */` |
|      ! 0 |  3638 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3639 | `				}` |
|        2 |  3640 | `			}` |
|        3 |  3641 | `		}else{` |
|      ! 0 |  3642 | `			if( pInstr->iP1 ){` |
|        - |  3643 | `				/* Pre-increment */` |
|      ! 0 |  3644 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3645 | `					pTos->rVal--;` |
|        - |  3646 | `					/* Try to get an integer representation */` |
|      ! 0 |  3647 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3648 | `				}else{` |
|      ! 0 |  3649 | `					pTos->x.iVal--;` |
|      ! 0 |  3650 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3651 | `				}` |
|      ! 0 |  3652 | `			}` |
|        - |  3653 | `		}` |
|        2 |  3654 | `	}` |
|        5 |  3655 | `	break;` |
|        - |  3656 | `/*` |
|        - |  3657 | ` * UMINUS: * * *` |
|        - |  3658 | ` *` |
|        - |  3659 | ` * Perform a unary minus operation.` |
|        - |  3660 | ` */` |
|    23369 |  3661 | `case PH7_OP_UMINUS:` |
|        - |  3662 | `#ifdef UNTRUST` |
|        - |  3663 | `	if( pTos < pStack ){` |
|        - |  3664 | `		goto Abort;` |
|        - |  3665 | `	}` |
|        - |  3666 | `#endif` |
|        - |  3667 | `	/* Force a numeric (integer,real or both) cast */` |
|    46740 |  3668 | `	PH7_MemObjToNumeric(pTos);` |
|    46740 |  3669 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3670 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3671 | `	}` |
|    46740 |  3672 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    46710 |  3673 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23354 |  3674 | `	}` |
|    46740 |  3675 | `	break;` |
|        - |  3676 | `/*` |
|        - |  3677 | ` * UPLUS: * * *` |
|        - |  3678 | ` *` |
|        - |  3679 | ` * Perform a unary plus operation.` |
|        - |  3680 | ` */` |
|       16 |  3681 | `case PH7_OP_UPLUS:` |
|        - |  3682 | `#ifdef UNTRUST` |
|        - |  3683 | `	if( pTos < pStack ){` |
|        - |  3684 | `		goto Abort;` |
|        - |  3685 | `	}` |
|        - |  3686 | `#endif` |
|        - |  3687 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3688 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3689 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3690 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3691 | `	}` |
|       33 |  3692 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3693 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3694 | `	}` |
|       33 |  3695 | `	break;` |
|        - |  3696 | `/*` |
|        - |  3697 | ` * OP_LNOT: * * *` |
|        - |  3698 | ` *` |
|        - |  3699 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3700 | ` * with its complement.` |
|        - |  3701 | ` */` |
|    39632 |  3702 | `case PH7_OP_LNOT:` |
|        - |  3703 | `#ifdef UNTRUST` |
|        - |  3704 | `	if( pTos < pStack ){` |
|        - |  3705 | `		goto Abort;` |
|        - |  3706 | `	}` |
|        - |  3707 | `#endif` |
|        - |  3708 | `	/* Force a boolean cast */` |
|    79310 |  3709 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3710 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3711 | `	}` |
|    79310 |  3712 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    79310 |  3713 | `	break;` |
|        - |  3714 | `/*` |
|        - |  3715 | ` * OP_BITNOT: * * *` |
|        - |  3716 | ` *` |
|        - |  3717 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3718 | ` * with its ones-complement.` |
|        - |  3719 | ` */` |
|       14 |  3720 | `case PH7_OP_BITNOT:` |
|        - |  3721 | `#ifdef UNTRUST` |
|        - |  3722 | `	if( pTos < pStack ){` |
|        - |  3723 | `		goto Abort;` |
|        - |  3724 | `	}` |
|        - |  3725 | `#endif` |
|        - |  3726 | `	/* Force an integer cast */` |
|       30 |  3727 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3728 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3729 | `	}` |
|       30 |  3730 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3731 | `	break;` |
|        - |  3732 | `/* OP_MUL * * *` |
|        - |  3733 | ` * OP_MUL_STORE * * *` |
|        - |  3734 | ` *` |
|        - |  3735 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3736 | ` * and push the result back onto the stack.` |
|        - |  3737 | ` */` |
|     1243 |  3738 | `case PH7_OP_MUL:` |
|        - |  3739 | `case PH7_OP_MUL_STORE: {` |
|     2488 |  3740 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3741 | `	/* Force the operand to be numeric */` |
|        - |  3742 | `#ifdef UNTRUST` |
|        - |  3743 | `	if( pNos < pStack ){` |
|        - |  3744 | `		goto Abort;` |
|        - |  3745 | `	}` |
|        - |  3746 | `#endif` |
|     2488 |  3747 | `	PH7_MemObjToNumeric(pTos);` |
|     2488 |  3748 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3749 | `	/* Perform the requested operation */` |
|     2488 |  3750 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3751 | `		/* Floating point arithemic */` |
|        - |  3752 | `		ph7_real a,b,r;` |
|       17 |  3753 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3754 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3755 | `		}` |
|       17 |  3756 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3757 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3758 | `		}` |
|       17 |  3759 | `		a = pNos->rVal;` |
|       17 |  3760 | `		b = pTos->rVal;` |
|       17 |  3761 | `		r = a * b;` |
|        - |  3762 | `		/* Push the result */` |
|       17 |  3763 | `		pNos->rVal = r;` |
|       17 |  3764 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3765 | `		/* Try to get an integer representation */` |
|       17 |  3766 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3767 | `	}else{` |
|        - |  3768 | `		/* Integer arithmetic */` |
|        - |  3769 | `		sxi64 a,b,r;` |
|     2472 |  3770 | `		a = pNos->x.iVal;` |
|     2472 |  3771 | `		b = pTos->x.iVal;` |
|     2472 |  3772 | `		r = a * b;` |
|        - |  3773 | `		/* Push the result */` |
|     2472 |  3774 | `		pNos->x.iVal = r;` |
|     2472 |  3775 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3776 | `	}` |
|     2488 |  3777 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3778 | `		ph7_value *pObj;` |
|       25 |  3779 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3780 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       25 |  3781 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       25 |  3782 | `			PH7_MemObjStore(pNos,pObj);` |
|       12 |  3783 | `		}` |
|       12 |  3784 | `	}` |
|     2488 |  3785 | `	VmPopOperand(&pTos,1);` |
|     2488 |  3786 | `	break;` |
|        - |  3787 | `				 }` |
|        - |  3788 | `/* OP_ADD * * *` |
|        - |  3789 | ` *` |
|        - |  3790 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3791 | ` * and push the result back onto the stack.` |
|        - |  3792 | ` */` |
|      429 |  3793 | `case PH7_OP_ADD:{` |
|      860 |  3794 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3795 | `#ifdef UNTRUST` |
|        - |  3796 | `	if( pNos < pStack ){` |
|        - |  3797 | `		goto Abort;` |
|        - |  3798 | `	}` |
|        - |  3799 | `#endif` |
|        - |  3800 | `	/* Perform the addition */` |
|      860 |  3801 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      860 |  3802 | `	VmPopOperand(&pTos,1);` |
|      860 |  3803 | `	break;` |
|        - |  3804 | `				}` |
|        - |  3805 | `/*` |
|        - |  3806 | ` * OP_ADD_STORE * * *` |
|        - |  3807 | ` *` |
|        - |  3808 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3809 | ` * and push the result back onto the stack.` |
|        - |  3810 | ` */` |
|      483 |  3811 | `case PH7_OP_ADD_STORE:{` |
|      968 |  3812 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3813 | `	ph7_value *pObj;` |
|        - |  3814 | `	sxu32 nIdx;` |
|        - |  3815 | `#ifdef UNTRUST` |
|        - |  3816 | `	if( pNos < pStack ){` |
|        - |  3817 | `		goto Abort;` |
|        - |  3818 | `	}` |
|        - |  3819 | `#endif` |
|        - |  3820 | `	/* Perform the addition */` |
|      968 |  3821 | `	nIdx = pTos->nIdx;` |
|      968 |  3822 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3823 | `	/* Peform the store operation */` |
|      968 |  3824 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3825 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      968 |  3826 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      968 |  3827 | `		PH7_MemObjStore(pTos,pObj);` |
|      483 |  3828 | `	}` |
|        - |  3829 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      968 |  3830 | `	PH7_MemObjStore(pTos,pNos);` |
|      968 |  3831 | `	VmPopOperand(&pTos,1);` |
|      968 |  3832 | `	break;` |
|        - |  3833 | `				}` |
|        - |  3834 | `/* OP_SUB * * *` |
|        - |  3835 | ` *` |
|        - |  3836 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3837 | ` * first (what was next on the stack) from the second (the` |
|        - |  3838 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3839 | ` */` |
|      299 |  3840 | `case PH7_OP_SUB: {` |
|      600 |  3841 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3842 | `#ifdef UNTRUST` |
|        - |  3843 | `	if( pNos < pStack ){` |
|        - |  3844 | `		goto Abort;` |
|        - |  3845 | `	}` |
|        - |  3846 | `#endif` |
|      600 |  3847 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3848 | `		/* Floating point arithemic */` |
|        - |  3849 | `		ph7_real a,b,r;` |
|       95 |  3850 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3851 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3852 | `		}` |
|       95 |  3853 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3854 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3855 | `		}` |
|       95 |  3856 | `		a = pNos->rVal;` |
|       95 |  3857 | `		b = pTos->rVal;` |
|       95 |  3858 | `		r = a - b;` |
|        - |  3859 | `		/* Push the result */` |
|       95 |  3860 | `		pNos->rVal = r;` |
|       95 |  3861 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3862 | `		/* Try to get an integer representation */` |
|       95 |  3863 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3864 | `	}else{` |
|        - |  3865 | `		/* Integer arithmetic */` |
|        - |  3866 | `		sxi64 a,b,r;` |
|      506 |  3867 | `		a = pNos->x.iVal;` |
|      506 |  3868 | `		b = pTos->x.iVal;` |
|      506 |  3869 | `		r = a - b;` |
|        - |  3870 | `		/* Push the result */` |
|      506 |  3871 | `		pNos->x.iVal = r;` |
|      506 |  3872 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3873 | `	}` |
|      600 |  3874 | `	VmPopOperand(&pTos,1);` |
|      600 |  3875 | `	break;` |
|        - |  3876 | `				 }` |
|        - |  3877 | `/* OP_SUB_STORE * * *` |
|        - |  3878 | ` *` |
|        - |  3879 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3880 | ` * first (what was next on the stack) from the second (the` |
|        - |  3881 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3882 | ` */` |
|        1 |  3883 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3884 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3885 | `	ph7_value *pObj;` |
|        - |  3886 | `#ifdef UNTRUST` |
|        - |  3887 | `	if( pNos < pStack ){` |
|        - |  3888 | `		goto Abort;` |
|        - |  3889 | `	}` |
|        - |  3890 | `#endif` |
|        3 |  3891 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3892 | `		/* Floating point arithemic */` |
|        - |  3893 | `		ph7_real a,b,r;` |
|      ! 0 |  3894 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3895 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3896 | `		}` |
|      ! 0 |  3897 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3898 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3899 | `		}` |
|      ! 0 |  3900 | `		a = pTos->rVal;` |
|      ! 0 |  3901 | `		b = pNos->rVal;` |
|      ! 0 |  3902 | `		r = a - b;` |
|        - |  3903 | `		/* Push the result */` |
|      ! 0 |  3904 | `		pNos->rVal = r;` |
|      ! 0 |  3905 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3906 | `		/* Try to get an integer representation */` |
|      ! 0 |  3907 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3908 | `	}else{` |
|        - |  3909 | `		/* Integer arithmetic */` |
|        - |  3910 | `		sxi64 a,b,r;` |
|        3 |  3911 | `		a = pTos->x.iVal;` |
|        3 |  3912 | `		b = pNos->x.iVal;` |
|        3 |  3913 | `		r = a - b;` |
|        - |  3914 | `		/* Push the result */` |
|        3 |  3915 | `		pNos->x.iVal = r;` |
|        3 |  3916 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3917 | `	}` |
|        3 |  3918 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3919 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3920 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3921 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3922 | `	}` |
|        3 |  3923 | `	VmPopOperand(&pTos,1);` |
|        3 |  3924 | `	break;` |
|        - |  3925 | `				 }` |
|        - |  3926 |  |
|        - |  3927 | `/*` |
|        - |  3928 | ` * OP_MOD * * *` |
|        - |  3929 | ` *` |
|        - |  3930 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3931 | ` * first (what was next on the stack) from the second (the` |
|        - |  3932 | ` * top of the stack) and push the remainder after division` |
|        - |  3933 | ` * onto the stack.` |
|        - |  3934 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3935 | ` */` |
|      296 |  3936 | `case PH7_OP_MOD:{` |
|      594 |  3937 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3938 | `	sxi64 a,b,r;` |
|        - |  3939 | `#ifdef UNTRUST` |
|        - |  3940 | `	if( pNos < pStack ){` |
|        - |  3941 | `		goto Abort;` |
|        - |  3942 | `	}` |
|        - |  3943 | `#endif` |
|        - |  3944 | `	/* Force the operands to be integer */` |
|      594 |  3945 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3946 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3947 | `	}` |
|      594 |  3948 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3949 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3950 | `	}` |
|        - |  3951 | `	/* Perform the requested operation */` |
|      594 |  3952 | `	a = pNos->x.iVal;` |
|      594 |  3953 | `	b = pTos->x.iVal;` |
|      594 |  3954 | `	if( b == 0 ){` |
|        3 |  3955 | `		r = 0;` |
|        3 |  3956 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3957 | `		/* goto Abort; */` |
|        2 |  3958 | `	}else{` |
|      591 |  3959 | `		r = a%b;` |
|        - |  3960 | `	}` |
|        - |  3961 | `	/* Push the result */` |
|      594 |  3962 | `	pNos->x.iVal = r;` |
|      594 |  3963 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3964 | `	VmPopOperand(&pTos,1);` |
|      594 |  3965 | `	break;` |
|        - |  3966 | `				}` |
|        - |  3967 | `/*` |
|        - |  3968 | ` * OP_MOD_STORE * * *` |
|        - |  3969 | ` *` |
|        - |  3970 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3971 | ` * first (what was next on the stack) from the second (the` |
|        - |  3972 | ` * top of the stack) and push the remainder after division` |
|        - |  3973 | ` * onto the stack.` |
|        - |  3974 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3975 | ` */` |
|        1 |  3976 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3977 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3978 | `	ph7_value *pObj;` |
|        - |  3979 | `	sxi64 a,b,r;` |
|        - |  3980 | `#ifdef UNTRUST` |
|        - |  3981 | `	if( pNos < pStack ){` |
|        - |  3982 | `		goto Abort;` |
|        - |  3983 | `	}` |
|        - |  3984 | `#endif` |
|        - |  3985 | `	/* Force the operands to be integer */` |
|        3 |  3986 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3987 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3988 | `	}` |
|        3 |  3989 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3990 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3991 | `	}` |
|        - |  3992 | `	/* Perform the requested operation */` |
|        3 |  3993 | `	a = pTos->x.iVal;` |
|        3 |  3994 | `	b = pNos->x.iVal;` |
|        3 |  3995 | `	if( b == 0 ){` |
|      ! 0 |  3996 | `		r = 0;` |
|      ! 0 |  3997 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3998 | `		/* goto Abort; */` |
|      ! 0 |  3999 | `	}else{` |
|        3 |  4000 | `		r = a%b;` |
|        - |  4001 | `	}` |
|        - |  4002 | `	/* Push the result */` |
|        3 |  4003 | `	pNos->x.iVal = r;` |
|        3 |  4004 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4005 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4006 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4007 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4008 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4009 | `	}` |
|        3 |  4010 | `	VmPopOperand(&pTos,1);` |
|        3 |  4011 | `	break;` |
|        - |  4012 | `				}` |
|        - |  4013 | `/*` |
|        - |  4014 | ` * OP_DIV * * *` |
|        - |  4015 | ` *` |
|        - |  4016 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4017 | ` * first (what was next on the stack) from the second (the` |
|        - |  4018 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4019 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4020 | ` */` |
|       28 |  4021 | `case PH7_OP_DIV:{` |
|       58 |  4022 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4023 | `	ph7_real a,b,r;` |
|        - |  4024 | `#ifdef UNTRUST` |
|        - |  4025 | `	if( pNos < pStack ){` |
|        - |  4026 | `		goto Abort;` |
|        - |  4027 | `	}` |
|        - |  4028 | `#endif` |
|        - |  4029 | `	/* Force the operands to be real */` |
|       58 |  4030 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  4031 | `		PH7_MemObjToReal(pTos);` |
|       26 |  4032 | `	}` |
|       58 |  4033 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  4034 | `		PH7_MemObjToReal(pNos);` |
|        9 |  4035 | `	}` |
|        - |  4036 | `	/* Perform the requested operation */` |
|       58 |  4037 | `	a = pNos->rVal;` |
|       58 |  4038 | `	b = pTos->rVal;` |
|       58 |  4039 | `	if( b == 0 ){` |
|        - |  4040 | `		/* Division by zero */` |
|        3 |  4041 | `		pNos->rVal = 0;` |
|        3 |  4042 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4043 | `		/* goto Abort; */` |
|        2 |  4044 | `	}else{` |
|       55 |  4045 | `		r = a/b;` |
|        - |  4046 | `		/* Push the result */` |
|       55 |  4047 | `		pNos->rVal = r;` |
|       55 |  4048 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4049 | `		/* Try to get an integer representation */` |
|       55 |  4050 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4051 | `	}` |
|       58 |  4052 | `	VmPopOperand(&pTos,1);` |
|       58 |  4053 | `	break;` |
|        - |  4054 | `				}` |
|        - |  4055 | `/*` |
|        - |  4056 | ` * OP_DIV_STORE * * *` |
|        - |  4057 | ` *` |
|        - |  4058 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4059 | ` * first (what was next on the stack) from the second (the` |
|        - |  4060 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4061 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4062 | ` */` |
|        1 |  4063 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4064 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4065 | `	ph7_value *pObj;` |
|        - |  4066 | `	ph7_real a,b,r;` |
|        - |  4067 | `#ifdef UNTRUST` |
|        - |  4068 | `	if( pNos < pStack ){` |
|        - |  4069 | `		goto Abort;` |
|        - |  4070 | `	}` |
|        - |  4071 | `#endif` |
|        - |  4072 | `	/* Force the operands to be real */` |
|        3 |  4073 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4074 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4075 | `	}` |
|        3 |  4076 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4077 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4078 | `	}` |
|        - |  4079 | `	/* Perform the requested operation */` |
|        3 |  4080 | `	a = pTos->rVal;` |
|        3 |  4081 | `	b = pNos->rVal;` |
|        3 |  4082 | `	if( b == 0 ){` |
|        - |  4083 | `		/* Division by zero */` |
|      ! 0 |  4084 | `		r = 0;` |
|      ! 0 |  4085 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4086 | `		/* goto Abort; */` |
|      ! 0 |  4087 | `	}else{` |
|        3 |  4088 | `		r = a/b;` |
|        - |  4089 | `		/* Push the result */` |
|        3 |  4090 | `		pNos->rVal = r;` |
|        3 |  4091 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4092 | `		/* Try to get an integer representation */` |
|        3 |  4093 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4094 | `	}` |
|        3 |  4095 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4096 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4097 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4098 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4099 | `	}` |
|        3 |  4100 | `	VmPopOperand(&pTos,1);` |
|        3 |  4101 | `	break;` |
|        - |  4102 | `				}` |
|        - |  4103 | `/* OP_BAND * * *` |
|        - |  4104 | ` *` |
|        - |  4105 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4106 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4107 | ` * two elements.` |
|        - |  4108 | `*/` |
|        - |  4109 | `/* OP_BOR * * *` |
|        - |  4110 | ` *` |
|        - |  4111 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4112 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4113 | ` * two elements.` |
|        - |  4114 | ` */` |
|        - |  4115 | `/* OP_BXOR * * *` |
|        - |  4116 | ` *` |
|        - |  4117 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4118 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4119 | ` * two elements.` |
|        - |  4120 | ` */` |
|       30 |  4121 | `case PH7_OP_BAND:` |
|        - |  4122 | `case PH7_OP_BOR:` |
|        - |  4123 | `case PH7_OP_BXOR:{` |
|       62 |  4124 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4125 | `	sxi64 a,b,r;` |
|        - |  4126 | `#ifdef UNTRUST` |
|        - |  4127 | `	if( pNos < pStack ){` |
|        - |  4128 | `		goto Abort;` |
|        - |  4129 | `	}` |
|        - |  4130 | `#endif` |
|        - |  4131 | `	/* Force the operands to be integer */` |
|       62 |  4132 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4133 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4134 | `	}` |
|       62 |  4135 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4136 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4137 | `	}` |
|        - |  4138 | `	/* Perform the requested operation */` |
|       62 |  4139 | `	a = pNos->x.iVal;` |
|       62 |  4140 | `	b = pTos->x.iVal;` |
|       62 |  4141 | `	switch(pInstr->iOp){` |
|        6 |  4142 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4143 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4144 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4145 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4146 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4147 | `	case PH7_OP_BAND:` |
|       38 |  4148 | `	default:          r = a&b; break;` |
|        - |  4149 | `	}` |
|        - |  4150 | `	/* Push the result */` |
|       62 |  4151 | `	pNos->x.iVal = r;` |
|       62 |  4152 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4153 | `	VmPopOperand(&pTos,1);` |
|       62 |  4154 | `	break;` |
|        - |  4155 | `				 }` |
|        - |  4156 | `/* OP_BAND_STORE * * *` |
|        - |  4157 | ` *` |
|        - |  4158 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4159 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4160 | ` * two elements.` |
|        - |  4161 | `*/` |
|        - |  4162 | `/* OP_BOR_STORE * * *` |
|        - |  4163 | ` *` |
|        - |  4164 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4165 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4166 | ` * two elements.` |
|        - |  4167 | ` */` |
|        - |  4168 | `/* OP_BXOR_STORE * * *` |
|        - |  4169 | ` *` |
|        - |  4170 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4171 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4172 | ` * two elements.` |
|        - |  4173 | ` */` |
|        7 |  4174 | `case PH7_OP_BAND_STORE:` |
|        - |  4175 | `case PH7_OP_BOR_STORE:` |
|        - |  4176 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4177 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4178 | `	ph7_value *pObj;` |
|        - |  4179 | `	sxi64 a,b,r;` |
|        - |  4180 | `#ifdef UNTRUST` |
|        - |  4181 | `	if( pNos < pStack ){` |
|        - |  4182 | `		goto Abort;` |
|        - |  4183 | `	}` |
|        - |  4184 | `#endif` |
|        - |  4185 | `	/* Force the operands to be integer */` |
|       15 |  4186 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4187 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4188 | `	}` |
|       15 |  4189 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4190 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4191 | `	}` |
|        - |  4192 | `	/* Perform the requested operation */` |
|       15 |  4193 | `	a = pTos->x.iVal;` |
|       15 |  4194 | `	b = pNos->x.iVal;` |
|       15 |  4195 | `	switch(pInstr->iOp){` |
|        2 |  4196 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4197 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4198 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4199 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4200 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4201 | `	case PH7_OP_BAND:` |
|        5 |  4202 | `	default:          r = a&b; break;` |
|        - |  4203 | `	}` |
|        - |  4204 | `	/* Push the result */` |
|       15 |  4205 | `	pNos->x.iVal = r;` |
|       15 |  4206 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4207 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4208 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4209 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4210 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4211 | `	}` |
|       15 |  4212 | `	VmPopOperand(&pTos,1);` |
|       15 |  4213 | `	break;` |
|        - |  4214 | `				 }` |
|        - |  4215 | `/* OP_SHL * * *` |
|        - |  4216 | ` *` |
|        - |  4217 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4218 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4219 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4220 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4221 | ` */` |
|        - |  4222 | `/* OP_SHR * * *` |
|        - |  4223 | ` *` |
|        - |  4224 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4225 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4226 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4227 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4228 | ` */` |
|        9 |  4229 | `case PH7_OP_SHL:` |
|        - |  4230 | `case PH7_OP_SHR: {` |
|       19 |  4231 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4232 | `	sxi64 a,r;` |
|        - |  4233 | `	sxi32 b;` |
|        - |  4234 | `#ifdef UNTRUST` |
|        - |  4235 | `	if( pNos < pStack ){` |
|        - |  4236 | `		goto Abort;` |
|        - |  4237 | `	}` |
|        - |  4238 | `#endif` |
|        - |  4239 | `	/* Force the operands to be integer */` |
|       19 |  4240 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4241 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4242 | `	}` |
|       19 |  4243 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4244 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4245 | `	}` |
|        - |  4246 | `	/* Perform the requested operation */` |
|       19 |  4247 | `	a = pNos->x.iVal;` |
|       19 |  4248 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4249 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4250 | `		r = a << b;` |
|        6 |  4251 | `	}else{` |
|        9 |  4252 | `		r = a >> b;` |
|        - |  4253 | `	}` |
|        - |  4254 | `	/* Push the result */` |
|       19 |  4255 | `	pNos->x.iVal = r;` |
|       19 |  4256 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4257 | `	VmPopOperand(&pTos,1);` |
|       19 |  4258 | `	break;` |
|        - |  4259 | `				 }` |
|        - |  4260 | `/*  OP_SHL_STORE * * *` |
|        - |  4261 | ` *` |
|        - |  4262 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4263 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4264 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4265 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4266 | ` */` |
|        - |  4267 | `/* OP_SHR_STORE * * *` |
|        - |  4268 | ` *` |
|        - |  4269 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4270 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4271 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4272 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4273 | ` */` |
|        7 |  4274 | `case PH7_OP_SHL_STORE:` |
|        - |  4275 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4276 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4277 | `	ph7_value *pObj;` |
|        - |  4278 | `	sxi64 a,r;` |
|        - |  4279 | `	sxi32 b;` |
|        - |  4280 | `#ifdef UNTRUST` |
|        - |  4281 | `	if( pNos < pStack ){` |
|        - |  4282 | `		goto Abort;` |
|        - |  4283 | `	}` |
|        - |  4284 | `#endif` |
|        - |  4285 | `	/* Force the operands to be integer */` |
|       15 |  4286 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4287 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4288 | `	}` |
|       15 |  4289 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4290 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4291 | `	}` |
|        - |  4292 | `	/* Perform the requested operation */` |
|       15 |  4293 | `	a = pTos->x.iVal;` |
|       15 |  4294 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4295 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4296 | `		r = a << b;` |
|        4 |  4297 | `	}else{` |
|        9 |  4298 | `		r = a >> b;` |
|        - |  4299 | `	}` |
|        - |  4300 | `	/* Push the result */` |
|       15 |  4301 | `	pNos->x.iVal = r;` |
|       15 |  4302 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4303 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4304 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4305 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4306 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4307 | `	}` |
|       15 |  4308 | `	VmPopOperand(&pTos,1);` |
|       15 |  4309 | `	break;` |
|        - |  4310 | `				 }` |
|        - |  4311 | `/* CAT:  P1 * *` |
|        - |  4312 | ` *` |
|        - |  4313 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4314 | ` * back.` |
|        - |  4315 | ` */` |
|    61919 |  4316 | `case PH7_OP_CAT:{` |
|        - |  4317 | `	ph7_value *pNos,*pCur;` |
|   123840 |  4318 | `	if( pInstr->iP1 < 1 ){` |
|    96882 |  4319 | `		pNos = &pTos[-1];` |
|    48442 |  4320 | `	}else{` |
|    26960 |  4321 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4322 | `	}` |
|        - |  4323 | `#ifdef UNTRUST` |
|        - |  4324 | `	if( pNos < pStack ){` |
|        - |  4325 | `		goto Abort;` |
|        - |  4326 | `	}` |
|        - |  4327 | `#endif` |
|        - |  4328 | `	/* Force a string cast */` |
|   123840 |  4329 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1134 |  4330 | `		PH7_MemObjToString(pNos);` |
|      566 |  4331 | `	}` |
|   123840 |  4332 | `	pCur = &pNos[1];` |
|   249642 |  4333 | `	while( pCur <= pTos ){` |
|   125804 |  4334 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50544 |  4335 | `			PH7_MemObjToString(pCur);` |
|    25271 |  4336 | `		}` |
|        - |  4337 | `		/* Perform the concatenation */` |
|   125804 |  4338 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   125766 |  4339 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    62882 |  4340 | `		}` |
|   125804 |  4341 | `		SyBlobRelease(&pCur->sBlob);` |
|   125804 |  4342 | `		pCur++;` |
|        2 |  4343 | `	}` |
|   123840 |  4344 | `	pTos = pNos;` |
|   123840 |  4345 | `	break;` |
|        - |  4346 | `				}` |
|        - |  4347 | `/*  CAT_STORE: * * *` |
|        - |  4348 | ` *` |
|        - |  4349 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4350 | ` * back.` |
|        - |  4351 | ` */` |
|     3356 |  4352 | `case PH7_OP_CAT_STORE:{` |
|     6714 |  4353 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4354 | `	ph7_value *pObj;` |
|        - |  4355 | `#ifdef UNTRUST` |
|        - |  4356 | `	if( pNos < pStack ){` |
|        - |  4357 | `		goto Abort;` |
|        - |  4358 | `	}` |
|        - |  4359 | `#endif` |
|     6714 |  4360 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4361 | `		/* Force a string cast */` |
|      ! 0 |  4362 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4363 | `	}` |
|     6714 |  4364 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4365 | `		/* Force a string cast */` |
|      ! 0 |  4366 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4367 | `	}` |
|        - |  4368 | `	/* Perform the concatenation (Reverse order) */` |
|     6714 |  4369 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6714 |  4370 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3356 |  4371 | `	}` |
|        - |  4372 | `	/* Perform the store operation */` |
|     6714 |  4373 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4374 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6714 |  4375 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6714 |  4376 | `		PH7_MemObjStore(pTos,pObj);` |
|     3356 |  4377 | `	}` |
|     6714 |  4378 | `	PH7_MemObjStore(pTos,pNos);` |
|     6714 |  4379 | `	VmPopOperand(&pTos,1);` |
|     6714 |  4380 | `	break;` |
|        - |  4381 | `				}` |
|        - |  4382 | `/* OP_AND: * * *` |
|        - |  4383 | ` *` |
|        - |  4384 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4385 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4386 | ` * stack.` |
|        - |  4387 | ` */` |
|        - |  4388 | `/* OP_OR: * * *` |
|        - |  4389 | ` *` |
|        - |  4390 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4391 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4392 | ` * stack.` |
|        - |  4393 | ` */` |
|    94196 |  4394 | `case PH7_OP_LAND:` |
|        - |  4395 | `case PH7_OP_LOR: {` |
|   188438 |  4396 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4397 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4398 | `#ifdef UNTRUST` |
|        - |  4399 | `	if( pNos < pStack ){` |
|        - |  4400 | `		goto Abort;` |
|        - |  4401 | `	}` |
|        - |  4402 | `#endif` |
|        - |  4403 | `	/* Force a boolean cast */` |
|   188438 |  4404 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4405 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4406 | `	}` |
|   188438 |  4407 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4408 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4409 | `	}` |
|   188438 |  4410 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   188438 |  4411 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   188438 |  4412 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4413 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86068 |  4414 | `		v1 = and_logic[v1*3+v2];` |
|    43057 |  4415 | `	}else{` |
|        - |  4416 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102372 |  4417 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4418 | `	}` |
|   188438 |  4419 | `	if( v1 == 2 ){` |
|      ! 0 |  4420 | `		v1 = 1;` |
|      ! 0 |  4421 | `	}` |
|   188438 |  4422 | `	VmPopOperand(&pTos,1);` |
|   188438 |  4423 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   188438 |  4424 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   188438 |  4425 | `	break;` |
|        - |  4426 | `				 }` |
|        - |  4427 | `/* OP_LXOR: * * *` |
|        - |  4428 | ` *` |
|        - |  4429 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4430 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4431 | ` * stack.` |
|        - |  4432 | ` * According to the PHP language reference manual:` |
|        - |  4433 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4434 | ` *  TRUE,but not both.` |
|        - |  4435 | ` */` |
|        5 |  4436 | `case PH7_OP_LXOR:{` |
|       11 |  4437 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4438 | `	sxi32 v = 0;` |
|        - |  4439 | `#ifdef UNTRUST` |
|        - |  4440 | `	if( pNos < pStack ){` |
|        - |  4441 | `		goto Abort;` |
|        - |  4442 | `	}` |
|        - |  4443 | `#endif` |
|        - |  4444 | `	/* Force a boolean cast */` |
|       11 |  4445 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4446 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4447 | `	}` |
|       11 |  4448 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4449 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4450 | `	}` |
|       11 |  4451 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4452 | `		v = 1;` |
|        3 |  4453 | `	}` |
|       11 |  4454 | `	VmPopOperand(&pTos,1);` |
|       11 |  4455 | `	pTos->x.iVal = v;` |
|       11 |  4456 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4457 | `	break;` |
|        - |  4458 | `				 }` |
|        - |  4459 | `/* OP_EQ P1 P2 P3` |
|        - |  4460 | ` *` |
|        - |  4461 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4462 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4463 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4464 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4465 | ` */` |
|        - |  4466 | `/* OP_NEQ P1 P2 P3` |
|        - |  4467 | ` *` |
|        - |  4468 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4469 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4470 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4471 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4472 | ` */` |
|     3845 |  4473 | `case PH7_OP_EQ:` |
|        - |  4474 | `case PH7_OP_NEQ: {` |
|     7692 |  4475 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4476 | `	/* Perform the comparison and act accordingly */` |
|        - |  4477 | `#ifdef UNTRUST` |
|        - |  4478 | `	if( pNos < pStack ){` |
|        - |  4479 | `		goto Abort;` |
|        - |  4480 | `	}` |
|        - |  4481 | `#endif` |
|     7692 |  4482 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7692 |  4483 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4484 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7683 |  4485 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7648 |  4486 | `		rc = rc == 0;` |
|     3825 |  4487 | `	}else{` |
|       28 |  4488 | `		rc = rc != 0;` |
|        - |  4489 | `	}` |
|     7692 |  4490 | `	VmPopOperand(&pTos,1);` |
|     7692 |  4491 | `	if( !pInstr->iP2 ){` |
|        - |  4492 | `		/* Push comparison result without taking the jump */` |
|     7692 |  4493 | `		PH7_MemObjRelease(pTos);` |
|     7692 |  4494 | `		pTos->x.iVal = rc;` |
|        - |  4495 | `		/* Invalidate any prior representation */` |
|     7692 |  4496 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3847 |  4497 | `	}else{` |
|      ! 0 |  4498 | `		if( rc ){` |
|        - |  4499 | `			/* Jump to the desired location */` |
|      ! 0 |  4500 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4501 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4502 | `		}` |
|        - |  4503 | `	}` |
|     7692 |  4504 | `	break;` |
|        - |  4505 | `				 }` |
|        - |  4506 | `/* OP_TEQ P1 P2 *` |
|        - |  4507 | ` *` |
|        - |  4508 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4509 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4510 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4511 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4512 | ` */` |
|   129956 |  4513 | `case PH7_OP_TEQ: {` |
|   259914 |  4514 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4515 | `	/* Perform the comparison and act accordingly */` |
|        - |  4516 | `#ifdef UNTRUST` |
|        - |  4517 | `	if( pNos < pStack ){` |
|        - |  4518 | `		goto Abort;` |
|        - |  4519 | `	}` |
|        - |  4520 | `#endif` |
|   259914 |  4521 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   259914 |  4522 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4523 | `		rc = 0;` |
|        2 |  4524 | `	}else{` |
|   259912 |  4525 | `		rc = rc == 0;` |
|        - |  4526 | `	}` |
|   259914 |  4527 | `	VmPopOperand(&pTos,1);` |
|   259914 |  4528 | `	if( !pInstr->iP2 ){` |
|        - |  4529 | `		/* Push comparison result without taking the jump */` |
|   259914 |  4530 | `		PH7_MemObjRelease(pTos);` |
|   259914 |  4531 | `		pTos->x.iVal = rc;` |
|        - |  4532 | `		/* Invalidate any prior representation */` |
|   259914 |  4533 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   129958 |  4534 | `	}else{` |
|      ! 0 |  4535 | `		if( rc ){` |
|        - |  4536 | `			/* Jump to the desired location */` |
|      ! 0 |  4537 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4538 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4539 | `		}` |
|        - |  4540 | `	}` |
|   259914 |  4541 | `	break;` |
|        - |  4542 | `				 }` |
|        - |  4543 | `/* OP_TNE P1 P2 *` |
|        - |  4544 | ` *` |
|        - |  4545 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4546 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4547 | ` * instruction.` |
|        - |  4548 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4549 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4550 | ` *` |
|        - |  4551 | ` */` |
|   101400 |  4552 | `case PH7_OP_TNE: {` |
|   202802 |  4553 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4554 | `	/* Perform the comparison and act accordingly */` |
|        - |  4555 | `#ifdef UNTRUST` |
|        - |  4556 | `	if( pNos < pStack ){` |
|        - |  4557 | `		goto Abort;` |
|        - |  4558 | `	}` |
|        - |  4559 | `#endif` |
|   202802 |  4560 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   202802 |  4561 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4562 | `		rc = 1;` |
|        2 |  4563 | `	}else{` |
|   202800 |  4564 | `		rc = rc != 0;` |
|        - |  4565 | `	}` |
|   202802 |  4566 | `	VmPopOperand(&pTos,1);` |
|   202802 |  4567 | `	if( !pInstr->iP2 ){` |
|        - |  4568 | `		/* Push comparison result without taking the jump */` |
|   202802 |  4569 | `		PH7_MemObjRelease(pTos);` |
|   202802 |  4570 | `		pTos->x.iVal = rc;` |
|        - |  4571 | `		/* Invalidate any prior representation */` |
|   202802 |  4572 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   101402 |  4573 | `	}else{` |
|      ! 0 |  4574 | `		if( rc ){` |
|        - |  4575 | `			/* Jump to the desired location */` |
|      ! 0 |  4576 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4577 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4578 | `		}` |
|        - |  4579 | `	}` |
|   202802 |  4580 | `	break;` |
|        - |  4581 | `				 }` |
|        - |  4582 | `/* OP_LT P1 P2 P3` |
|        - |  4583 | ` *` |
|        - |  4584 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4585 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4586 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4587 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4588 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4589 | ` *` |
|        - |  4590 | ` */` |
|        - |  4591 | `/* OP_LE P1 P2 P3` |
|        - |  4592 | ` *` |
|        - |  4593 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4594 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4595 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4596 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4597 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4598 | ` *` |
|        - |  4599 | ` */` |
|   102469 |  4600 | `case PH7_OP_LT:` |
|        - |  4601 | `case PH7_OP_LE: {` |
|   204984 |  4602 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4603 | `	/* Perform the comparison and act accordingly */` |
|        - |  4604 | `#ifdef UNTRUST` |
|        - |  4605 | `	if( pNos < pStack ){` |
|        - |  4606 | `		goto Abort;` |
|        - |  4607 | `	}` |
|        - |  4608 | `#endif` |
|   204984 |  4609 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204984 |  4610 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4611 | `		rc = 0;` |
|   204980 |  4612 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4613 | `		rc = rc < 1;` |
|      205 |  4614 | `	}else{` |
|   204570 |  4615 | `		rc = rc < 0;` |
|        - |  4616 | `	}` |
|   204984 |  4617 | `	VmPopOperand(&pTos,1);` |
|   204984 |  4618 | `	if( !pInstr->iP2 ){` |
|        - |  4619 | `		/* Push comparison result without taking the jump */` |
|   204984 |  4620 | `		PH7_MemObjRelease(pTos);` |
|   204984 |  4621 | `		pTos->x.iVal = rc;` |
|        - |  4622 | `		/* Invalidate any prior representation */` |
|   204984 |  4623 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102515 |  4624 | `	}else{` |
|      ! 0 |  4625 | `		if( rc ){` |
|        - |  4626 | `			/* Jump to the desired location */` |
|      ! 0 |  4627 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4628 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4629 | `		}` |
|        - |  4630 | `	}` |
|   204984 |  4631 | `	break;` |
|        - |  4632 | `				}` |
|        - |  4633 | `/* OP_GT P1 P2 P3` |
|        - |  4634 | ` *` |
|        - |  4635 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4636 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4637 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4638 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4639 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4640 | ` *` |
|        - |  4641 | ` */` |
|        - |  4642 | `/* OP_GE P1 P2 P3` |
|        - |  4643 | ` *` |
|        - |  4644 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4645 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4646 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4647 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4648 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4649 | ` *` |
|        - |  4650 | ` */` |
|    48811 |  4651 | `case PH7_OP_GT:` |
|        - |  4652 | `case PH7_OP_GE: {` |
|    97624 |  4653 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4654 | `	/* Perform the comparison and act accordingly */` |
|        - |  4655 | `#ifdef UNTRUST` |
|        - |  4656 | `	if( pNos < pStack ){` |
|        - |  4657 | `		goto Abort;` |
|        - |  4658 | `	}` |
|        - |  4659 | `#endif` |
|    97624 |  4660 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97624 |  4661 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4662 | `		rc = 0;` |
|    97620 |  4663 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97468 |  4664 | `		rc = rc >= 0;` |
|    48735 |  4665 | `	}else{` |
|      150 |  4666 | `		rc = rc > 0;` |
|        - |  4667 | `	}` |
|    97624 |  4668 | `	VmPopOperand(&pTos,1);` |
|    97624 |  4669 | `	if( !pInstr->iP2 ){` |
|        - |  4670 | `		/* Push comparison result without taking the jump */` |
|    97624 |  4671 | `		PH7_MemObjRelease(pTos);` |
|    97624 |  4672 | `		pTos->x.iVal = rc;` |
|        - |  4673 | `		/* Invalidate any prior representation */` |
|    97624 |  4674 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48813 |  4675 | `	}else{` |
|      ! 0 |  4676 | `		if( rc ){` |
|        - |  4677 | `			/* Jump to the desired location */` |
|      ! 0 |  4678 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4679 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4680 | `		}` |
|        - |  4681 | `	}` |
|    97624 |  4682 | `	break;` |
|        - |  4683 | `				}` |
|        - |  4684 | `/* OP_SEQ P1 P2 *` |
|        - |  4685 | ` * Strict string comparison.` |
|        - |  4686 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4687 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4688 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4689 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4690 | ` * use PH7_OP_EQ.` |
|        - |  4691 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4692 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4693 | ` */` |
|        - |  4694 | `/* OP_SNE P1 P2 *` |
|        - |  4695 | ` * Strict string comparison.` |
|        - |  4696 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4697 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4698 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4699 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4700 | ` * use PH7_OP_EQ.` |
|        - |  4701 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4702 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4703 | ` */` |
|       18 |  4704 | `case PH7_OP_SEQ:` |
|        - |  4705 | `case PH7_OP_SNE: {` |
|       38 |  4706 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4707 | `	SyString s1,s2;` |
|        - |  4708 | `	/* Perform the comparison and act accordingly */` |
|        - |  4709 | `#ifdef UNTRUST` |
|        - |  4710 | `	if( pNos < pStack ){` |
|        - |  4711 | `		goto Abort;` |
|        - |  4712 | `	}` |
|        - |  4713 | `#endif` |
|        - |  4714 | `	/* Force a string cast */` |
|       38 |  4715 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4716 | `		PH7_MemObjToString(pTos);` |
|        2 |  4717 | `	}` |
|       38 |  4718 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4719 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4720 | `	}` |
|       38 |  4721 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4722 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4723 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4724 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4725 | `		rc = rc != 0;` |
|      ! 0 |  4726 | `	}else{` |
|       38 |  4727 | `		rc = rc == 0;` |
|        - |  4728 | `	}` |
|       38 |  4729 | `	VmPopOperand(&pTos,1);` |
|       38 |  4730 | `	if( !pInstr->iP2 ){` |
|        - |  4731 | `		/* Push comparison result without taking the jump */` |
|       38 |  4732 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4733 | `		pTos->x.iVal = rc;` |
|        - |  4734 | `		/* Invalidate any prior representation */` |
|       38 |  4735 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4736 | `	}else{` |
|      ! 0 |  4737 | `		if( rc ){` |
|        - |  4738 | `			/* Jump to the desired location */` |
|      ! 0 |  4739 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4740 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4741 | `		}` |
|        - |  4742 | `	}` |
|       38 |  4743 | `	break;` |
|        - |  4744 | `				 }` |
|        - |  4745 | `/*` |
|        - |  4746 | ` * OP_LOAD_REF * * *` |
|        - |  4747 | ` * Push the index of a referenced object on the stack.` |
|        - |  4748 | ` */` |
|       57 |  4749 | `case PH7_OP_LOAD_REF: {` |
|        - |  4750 | `	sxu32 nIdx;` |
|        - |  4751 | `#ifdef UNTRUST` |
|        - |  4752 | `	if( pTos < pStack ){` |
|        - |  4753 | `		goto Abort;` |
|        - |  4754 | `	}` |
|        - |  4755 | `#endif` |
|        - |  4756 | `	/* Extract memory object index */` |
|      115 |  4757 | `	nIdx = pTos->nIdx;` |
|      115 |  4758 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4759 | `		/* Nullify the object */` |
|       95 |  4760 | `		PH7_MemObjRelease(pTos);` |
|        - |  4761 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4762 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4763 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4764 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4765 | `	}` |
|      115 |  4766 | `	break;` |
|        - |  4767 | `					  }` |
|        - |  4768 | `/*` |
|        - |  4769 | ` * OP_STORE_REF * * P3` |
|        - |  4770 | ` * Perform an assignment operation by reference.` |
|        - |  4771 | ` */` |
|       15 |  4772 | ` case PH7_OP_STORE_REF: {` |
|       32 |  4773 | `	 SyString sName = { 0 , 0 };` |
|        - |  4774 | `	 VmFrame *pFrameLocal;` |
|        - |  4775 | `	SyHashEntry *pEntry;` |
|        - |  4776 | `	sxu32 nIdx;` |
|        - |  4777 | `#ifdef UNTRUST` |
|        - |  4778 | `	if( pTos < pStack ){` |
|        - |  4779 | `		goto Abort;` |
|        - |  4780 | `	}` |
|        - |  4781 | `#endif` |
|       32 |  4782 | `	if( pInstr->p3 == 0 ){` |
|        - |  4783 | `		char *zName;` |
|        - |  4784 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4785 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4786 | `			/* Force a string cast */` |
|      ! 0 |  4787 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4788 | `		}` |
|      ! 0 |  4789 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4790 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4791 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4792 | `			if( zName ){` |
|      ! 0 |  4793 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4794 | `			}` |
|      ! 0 |  4795 | `		}` |
|      ! 0 |  4796 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4797 | `		pTos--;` |
|      ! 0 |  4798 | `	}else{` |
|       32 |  4799 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4800 | `	}` |
|       32 |  4801 | `	nIdx = pTos->nIdx;` |
|       32 |  4802 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4803 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4804 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4805 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4806 | `		}else{` |
|        - |  4807 | `			ph7_value *pObj;` |
|        - |  4808 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4809 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4810 | `			if( pObj == 0 ){` |
|      ! 0 |  4811 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4812 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4813 | `				goto Abort;` |
|        - |  4814 | `			}` |
|        - |  4815 | `			/* Perform the store operation */` |
|      ! 0 |  4816 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4817 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4818 | `		}` |
|       32 |  4819 | `	}else if( sName.nByte > 0){` |
|       32 |  4820 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4821 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4822 | `		}else{` |
|       32 |  4823 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  4824 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4825 | `			/* Query the local frame */` |
|       32 |  4826 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  4827 | `			if( pEntry ){` |
|      ! 0 |  4828 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4829 | `			}else{` |
|       32 |  4830 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  4831 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4832 | `					/* Insert in the $GLOBALS array */` |
|       28 |  4833 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  4834 | `				}` |
|       32 |  4835 | `				if( rc == SXRET_OK ){` |
|       32 |  4836 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  4837 | `				}` |
|        - |  4838 | `			}` |
|        - |  4839 | `		}` |
|       15 |  4840 | `	}` |
|       32 |  4841 | `	break;` |
|        - |  4842 | `				 }` |
|        - |  4843 | `/*` |
|        - |  4844 | ` * OP_UPLINK P1 * *` |
|        - |  4845 | ` * Link a variable to the top active VM frame.` |
|        - |  4846 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4847 | ` */` |
|       25 |  4848 | `case PH7_OP_UPLINK: {` |
|       52 |  4849 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4850 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4851 | `		SyString sName;` |
|        - |  4852 | `		/* Perform the link */` |
|      104 |  4853 | `		while( pLink <= pTos ){` |
|       54 |  4854 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4855 | `				/* Force a string cast */` |
|      ! 0 |  4856 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4857 | `			}` |
|       54 |  4858 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4859 | `			if( sName.nByte > 0 ){` |
|       54 |  4860 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4861 | `			}` |
|       54 |  4862 | `			pLink++;` |
|        2 |  4863 | `		}` |
|       25 |  4864 | `	}` |
|       52 |  4865 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4866 | `	break;` |
|        - |  4867 | `					}` |
|        - |  4868 | `/*` |
|        - |  4869 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4870 | ` * Push an exception in the corresponding container so that` |
|        - |  4871 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4872 | ` */` |
|       29 |  4873 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       60 |  4874 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4875 | `	VmFrame *pFrameLocal;` |
|        - |  4876 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       60 |  4877 | `	pException->iFinallyDone = 0;` |
|       60 |  4878 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4879 | `	/* Create the exception frame */` |
|       60 |  4880 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       60 |  4881 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4882 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4883 | `		goto Abort;` |
|        - |  4884 | `	}` |
|        - |  4885 | `	/* Mark the special frame */` |
|       60 |  4886 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       60 |  4887 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4888 | `	/* Point to the frame that trigger the exception */` |
|       60 |  4889 | `	pFrameLocal = pFrameLocal->pParent;` |
|       60 |  4890 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       60 |  4891 | `	pException->pFrame = pFrameLocal;` |
|       60 |  4892 | `	break;` |
|        - |  4893 | `							}` |
|        - |  4894 | `/*` |
|        - |  4895 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4896 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4897 | ` */` |
|       28 |  4898 | `case PH7_OP_POP_EXCEPTION: {` |
|       58 |  4899 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       58 |  4900 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4901 | `		ph7_exception **apException;` |
|        - |  4902 | `		/* Pop the loaded exception */` |
|       28 |  4903 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  4904 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  4905 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  4906 | `		}` |
|       13 |  4907 | `	}` |
|       58 |  4908 | `	pException->pFrame = 0;` |
|        - |  4909 | `	/* Leave the exception frame */` |
|       58 |  4910 | `	VmLeaveFrame(&(*pVm));` |
|        - |  4911 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       58 |  4912 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  4913 | `		sxi32 rcFinally;` |
|       19 |  4914 | `		pException->iFinallyDone = 1;` |
|       19 |  4915 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  4916 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  4917 | `			goto Abort;` |
|        - |  4918 | `		}` |
|        9 |  4919 | `	}` |
|       58 |  4920 | `	break;` |
|        - |  4921 | `							}` |
|        - |  4922 |  |
|        - |  4923 | `/*` |
|        - |  4924 | ` * OP_THROW * P2 *` |
|        - |  4925 | ` * Throw an user exception.` |
|        - |  4926 | ` */` |
|       17 |  4927 | `case PH7_OP_THROW: {` |
|       36 |  4928 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  4929 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4930 | `#ifdef UNTRUST` |
|        - |  4931 | `	if( pTos < pStack ){` |
|        - |  4932 | `		goto Abort;` |
|        - |  4933 | `	}` |
|        - |  4934 | `#endif` |
|       36 |  4935 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4936 | `	/* Tell the upper layer that an exception was thrown */` |
|       36 |  4937 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       36 |  4938 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       36 |  4939 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4940 | `		ph7_class *pException;` |
|        - |  4941 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4942 | `		 */` |
|       36 |  4943 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       36 |  4944 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4945 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4946 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4947 | `			if( rc == SXERR_ABORT ){` |
|        - |  4948 | `				/* Abort processing immediately */` |
|      ! 0 |  4949 | `				goto Abort;` |
|        - |  4950 | `			}` |
|      ! 0 |  4951 | `		}else{` |
|        - |  4952 | `			/* Throw the exception */` |
|       36 |  4953 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       36 |  4954 | `			if( rc == SXERR_ABORT ){` |
|        - |  4955 | `				/* Abort processing immediately */` |
|        9 |  4956 | `				goto Abort;` |
|        - |  4957 | `			}` |
|        - |  4958 | `		}` |
|       15 |  4959 | `	}else{` |
|        - |  4960 | `		/* Expecting a class instance */` |
|      ! 0 |  4961 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4962 | `		if( rc == SXERR_ABORT ){` |
|        - |  4963 | `			/* Abort processing immediately */` |
|      ! 0 |  4964 | `			goto Abort;` |
|        - |  4965 | `		}` |
|        - |  4966 | `	}` |
|        - |  4967 | `	/* Pop the top entry */` |
|       28 |  4968 | `	VmPopOperand(&pTos,1);` |
|        - |  4969 | `	/* Perform an unconditional jump */` |
|       28 |  4970 | `	pc = nJump - 1;` |
|       28 |  4971 | `	break;` |
|        - |  4972 | `				   }` |
|        - |  4973 | `/*` |
|        - |  4974 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4975 | ` * Prepare a foreach step.` |
|        - |  4976 | ` */` |
|     4859 |  4977 | `case PH7_OP_FOREACH_INIT: {` |
|     9720 |  4978 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4979 | `	void *pName;` |
|        - |  4980 | `#ifdef UNTRUST` |
|        - |  4981 | `	if( pTos < pStack ){` |
|        - |  4982 | `		goto Abort;` |
|        - |  4983 | `	}` |
|        - |  4984 | `#endif` |
|     9720 |  4985 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4986 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4987 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4988 | `			/* Force a string cast */` |
|      ! 0 |  4989 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4990 | `		}` |
|        - |  4991 | `		/* Duplicate name */` |
|      ! 0 |  4992 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4993 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4994 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4995 | `		}` |
|      ! 0 |  4996 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4997 | `	}` |
|     9720 |  4998 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4999 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5000 | `			/* Force a string cast */` |
|      ! 0 |  5001 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5002 | `		}` |
|        - |  5003 | `		/* Duplicate name */` |
|      ! 0 |  5004 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5005 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5006 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5007 | `		}` |
|      ! 0 |  5008 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5009 | `	}` |
|        - |  5010 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9720 |  5011 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5012 | `		/* Jump out of the loop */` |
|      ! 0 |  5013 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5014 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5015 | `		}` |
|      ! 0 |  5016 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5017 | `	}else{` |
|        - |  5018 | `		ph7_foreach_step *pStep;` |
|     9720 |  5019 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9720 |  5020 | `		if( pStep == 0 ){` |
|      ! 0 |  5021 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5022 | `			/* Jump out of the loop */` |
|      ! 0 |  5023 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5024 | `		}else{` |
|        - |  5025 | `			/* Zero the structure */` |
|     9720 |  5026 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5027 | `			/* Prepare the step */` |
|     9720 |  5028 | `			pStep->iFlags = pInfo->iFlags;` |
|     9720 |  5029 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5030 | `				ph7_hashmap *pMap;` |
|        - |  5031 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5032 | `				 * source array so mutations don't affect other sharers. */` |
|     9704 |  5033 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5034 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5035 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5036 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5037 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5038 | `						 * variable still points at the same hashmap as` |
|        - |  5039 | `						 * the stack value. */` |
|       10 |  5040 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5041 | `							pCur->iRef--;` |
|       10 |  5042 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5043 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5044 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5045 | `						}` |
|        4 |  5046 | `					}` |
|        4 |  5047 | `				}` |
|     9704 |  5048 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5049 | `				/* Reset the internal loop cursor */` |
|     9704 |  5050 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5051 | `				/* Mark the step */` |
|     9704 |  5052 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9704 |  5053 | `				pStep->xIter.pMap = pMap;` |
|     9704 |  5054 | `				pMap->iRef++;` |
|     4853 |  5055 | `			}else{` |
|       18 |  5056 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5057 | `				ph7_class *pIteratorClass;` |
|        - |  5058 | `				/* Check if the object implements Iterator */` |
|       18 |  5059 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       21 |  5060 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5061 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5062 | `					ph7_class_method *pRewind;` |
|        7 |  5063 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        7 |  5064 | `					pStep->xIter.pThis = pThis;` |
|        7 |  5065 | `					pThis->iRef++;` |
|        7 |  5066 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|        7 |  5067 | `					if( pRewind ){` |
|        7 |  5068 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        3 |  5069 | `					}` |
|        4 |  5070 | `				}else{` |
|        - |  5071 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5072 | `					ph7_class *pIterAggClass;` |
|       12 |  5073 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5074 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5075 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5076 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5077 | `						ph7_class_method *pGetIter;` |
|        3 |  5078 | `						int iterAggOk = 0;` |
|        3 |  5079 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5080 | `						if( pGetIter ){` |
|        - |  5081 | `							ph7_value sResult;` |
|        3 |  5082 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5083 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5084 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5085 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5086 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5087 | `									ph7_class_method *pRewind;` |
|        3 |  5088 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5089 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5090 | `									pIterObj->iRef++;` |
|        - |  5091 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5092 | `									pStep->pOwner = pThis;` |
|        3 |  5093 | `									pThis->iRef++;` |
|        3 |  5094 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5095 | `									if( pRewind ){` |
|        3 |  5096 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5097 | `									}` |
|        3 |  5098 | `									iterAggOk = 1;` |
|        1 |  5099 | `								}` |
|        1 |  5100 | `							}` |
|        3 |  5101 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5102 | `						}` |
|        3 |  5103 | `						if( !iterAggOk ){` |
|        - |  5104 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5105 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5106 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5107 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5108 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5109 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5110 | `						}` |
|        2 |  5111 | `					}else{` |
|        - |  5112 | `						/* Plain object iteration via hAttr */` |
|        9 |  5113 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5114 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5115 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5116 | `						pThis->iRef++;` |
|        - |  5117 | `					}` |
|        - |  5118 | `				}` |
|        - |  5119 | `			}` |
|        - |  5120 | `		}` |
|     9720 |  5121 | `		if( pStep ){` |
|     9720 |  5122 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5123 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5124 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5125 | `				/* Jump out of the loop */` |
|      ! 0 |  5126 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5127 | `			}` |
|     4859 |  5128 | `		}` |
|        - |  5129 | `	}` |
|     9720 |  5130 | `	VmPopOperand(&pTos,1);` |
|     9720 |  5131 | `	break;` |
|        - |  5132 | `						  }` |
|        - |  5133 | `/*` |
|        - |  5134 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5135 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5136 | ` */` |
|    78036 |  5137 | `case PH7_OP_FOREACH_STEP: {` |
|   156074 |  5138 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5139 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5140 | `	ph7_value *pValue;` |
|        - |  5141 | `	VmFrame *pFrameLocal;` |
|        - |  5142 | `	/* Peek the last step */` |
|   156074 |  5143 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   156074 |  5144 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   156074 |  5145 | `	pFrameLocal = pVm->pFrame;` |
|   156074 |  5146 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   156074 |  5147 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   156014 |  5148 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5149 | `		ph7_hashmap_node *pNode;` |
|        - |  5150 | `		/* Extract the current node value */` |
|   156014 |  5151 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   156014 |  5152 | `		if( pNode == 0 ){` |
|        - |  5153 | `			/* No more entry to process */` |
|     9702 |  5154 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9702 |  5155 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5156 | `				/* Break the reference with the last element */` |
|        7 |  5157 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5158 | `			}` |
|        - |  5159 | `			/* Automatically reset the loop cursor */` |
|     9702 |  5160 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5161 | `			/* Cleanup the mess left behind */` |
|     9702 |  5162 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9702 |  5163 | `			SySetPop(&pInfo->aStep);` |
|     9702 |  5164 | `			PH7_HashmapUnref(pMap);` |
|     4852 |  5165 | `		}else{` |
|   146314 |  5166 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5167 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5168 | `				if( pKey ){` |
|      416 |  5169 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5170 | `				}` |
|      207 |  5171 | `			}` |
|   146314 |  5172 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5173 | `				SyHashEntry *pEntry;` |
|        - |  5174 | `				/* Pass by reference */` |
|       24 |  5175 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5176 | `				if( pEntry ){` |
|       22 |  5177 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5178 | `				}else{` |
|        4 |  5179 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5180 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5181 | `				}` |
|       13 |  5182 | `			}else{` |
|        - |  5183 | `				/* Make a copy of the entry value */` |
|   146292 |  5184 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   146292 |  5185 | `				if( pValue ){` |
|   146292 |  5186 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    73145 |  5187 | `				}` |
|        - |  5188 | `			}` |
|        2 |  5189 | `		}` |
|    78068 |  5190 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5191 | `		/* Iterator-based iteration.` |
|        - |  5192 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5193 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5194 | `		 */` |
|       37 |  5195 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5196 | `		ph7_class_method *pMethod;` |
|        - |  5197 | `		ph7_value sResult;` |
|       37 |  5198 | `		int isValid = 0;` |
|        - |  5199 | `		/* Call next() to advance — but skip on the first iteration */` |
|       37 |  5200 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        9 |  5201 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        5 |  5202 | `		}else{` |
|       29 |  5203 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       29 |  5204 | `			if( pMethod ){` |
|       29 |  5205 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       14 |  5206 | `			}` |
|        - |  5207 | `		}` |
|        - |  5208 | `		/* Call valid() */` |
|       37 |  5209 | `		PH7_MemObjInit(pVm,&sResult);` |
|       37 |  5210 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       37 |  5211 | `		if( pMethod ){` |
|       37 |  5212 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       37 |  5213 | `			PH7_MemObjToBool(&sResult);` |
|       37 |  5214 | `			isValid = (sResult.x.iVal != 0);` |
|       18 |  5215 | `		}` |
|       37 |  5216 | `		PH7_MemObjRelease(&sResult);` |
|       37 |  5217 | `		if( !isValid ){` |
|        - |  5218 | `			/* Iterator exhausted */` |
|        7 |  5219 | `			pc = pInstr->iP2 - 1;` |
|        - |  5220 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|        7 |  5221 | `			if( pStep->pOwner ){` |
|        3 |  5222 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5223 | `			}` |
|        7 |  5224 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        7 |  5225 | `			SySetPop(&pInfo->aStep);` |
|        7 |  5226 | `			PH7_ClassInstanceUnref(pThis);` |
|        4 |  5227 | `		}else{` |
|        - |  5228 | `			/* Call current() to get value */` |
|       31 |  5229 | `			PH7_MemObjInit(pVm,&sResult);` |
|       31 |  5230 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       31 |  5231 | `			if( pMethod ){` |
|       31 |  5232 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       15 |  5233 | `			}` |
|       31 |  5234 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       31 |  5235 | `			if( pValue ){` |
|       31 |  5236 | `				PH7_MemObjStore(&sResult,pValue);` |
|       15 |  5237 | `			}` |
|       31 |  5238 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5239 | `			/* Call key() if needed */` |
|       31 |  5240 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5241 | `				ph7_value sKey;` |
|       23 |  5242 | `				PH7_MemObjInit(pVm,&sKey);` |
|       23 |  5243 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       23 |  5244 | `				if( pMethod ){` |
|       23 |  5245 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       11 |  5246 | `				}` |
|       23 |  5247 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       23 |  5248 | `				if( pValue ){` |
|       23 |  5249 | `					PH7_MemObjStore(&sKey,pValue);` |
|       11 |  5250 | `				}` |
|       23 |  5251 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  5252 | `			}` |
|        - |  5253 | `		}` |
|       19 |  5254 | `	}else{` |
|       25 |  5255 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5256 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5257 | `		SyHashEntry *pEntry;` |
|        - |  5258 | `		/* Point to the next attribute */` |
|       29 |  5259 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5260 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5261 | `			/* Check access permission */` |
|       31 |  5262 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5263 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5264 | `					break; /* Access is granted */` |
|        - |  5265 | `			}` |
|        1 |  5266 | `		}` |
|       25 |  5267 | `		if( pEntry == 0 ){` |
|        - |  5268 | `			/* Clean up the mess left behind */` |
|        9 |  5269 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5270 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5271 | `				/* Break the reference with the last element */` |
|        3 |  5272 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5273 | `			}` |
|        9 |  5274 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5275 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5276 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5277 | `		}else{` |
|       17 |  5278 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5279 | `			ph7_value *pAttrValue;` |
|       17 |  5280 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5281 | `				/* Fill with the current attribute name */` |
|       17 |  5282 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5283 | `				if( pKey ){` |
|       17 |  5284 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5285 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5286 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5287 | `				}` |
|        8 |  5288 | `			}` |
|        - |  5289 | `			/* Extract attribute value */` |
|       17 |  5290 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5291 | `			if( pAttrValue ){` |
|       17 |  5292 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5293 | `					/* Pass by reference */` |
|        3 |  5294 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5295 | `					if( pEntry ){` |
|        3 |  5296 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5297 | `					}else{` |
|      ! 0 |  5298 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5299 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5300 | `					}` |
|        2 |  5301 | `				}else{` |
|        - |  5302 | `					/* Make a copy of the attribute value */` |
|       15 |  5303 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5304 | `					if( pValue ){` |
|       15 |  5305 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5306 | `					}` |
|        - |  5307 | `				}` |
|        8 |  5308 | `			}` |
|        - |  5309 | `		}` |
|        - |  5310 | `	}` |
|   156074 |  5311 | `	break;` |
|        - |  5312 | `						  }` |
|        - |  5313 | `/*` |
|        - |  5314 | ` * OP_MEMBER P1 P2` |
|        - |  5315 | ` * Load class attribute/method on the stack.` |
|        - |  5316 | ` */` |
|     2079 |  5317 | `case PH7_OP_MEMBER: {` |
|        - |  5318 | `	ph7_class_instance *pThis;` |
|        - |  5319 | `	ph7_value *pNos;` |
|        - |  5320 | `	SyString sName;` |
|     4160 |  5321 | `	if( !pInstr->iP1 ){` |
|     4062 |  5322 | `		pNos = &pTos[-1];` |
|        - |  5323 | `#ifdef UNTRUST` |
|        - |  5324 | `		if( pNos < pStack ){` |
|        - |  5325 | `			goto Abort;` |
|        - |  5326 | `		}` |
|        - |  5327 | `#endif` |
|     4062 |  5328 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5329 | `			ph7_class *pClass;` |
|        - |  5330 | `			/* Class already instantiated */` |
|     4062 |  5331 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5332 | `			/* Point to the instantiated class */` |
|     4062 |  5333 | `			pClass = pThis->pClass;` |
|        - |  5334 | `			/* Extract attribute name first */` |
|     4062 |  5335 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4062 |  5336 | `			if( pInstr->iP2 ){` |
|        - |  5337 | `				/* Method call */` |
|      278 |  5338 | `				ph7_class_method *pMeth = 0;` |
|      278 |  5339 | `				if( sName.nByte > 0 ){` |
|        - |  5340 | `					/* Extract the target method */` |
|      278 |  5341 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      138 |  5342 | `				}` |
|      278 |  5343 | `				if( pMeth == 0 ){` |
|      ! 0 |  5344 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5345 | `						&pClass->sName,&sName` |
|        - |  5346 | `						);` |
|        - |  5347 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5348 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5349 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5350 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5351 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5352 | `				}else{` |
|        - |  5353 | `					/* Push method name on the stack */` |
|      278 |  5354 | `					PH7_MemObjRelease(pTos);` |
|      278 |  5355 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      278 |  5356 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5357 | `				}` |
|      278 |  5358 | `				pTos->nIdx = SXU32_HIGH;` |
|      140 |  5359 | `			}else{` |
|        - |  5360 | `				/* Attribute access */` |
|     3786 |  5361 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5362 | `				SyHashEntry *pEntry;` |
|        - |  5363 | `				/* Extract the target attribute */` |
|     3786 |  5364 | `				if( sName.nByte > 0 ){` |
|     3786 |  5365 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3786 |  5366 | `					if( pEntry ){` |
|        - |  5367 | `						/* Point to the attribute value */` |
|     3784 |  5368 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1891 |  5369 | `					}` |
|     1892 |  5370 | `				}` |
|     3786 |  5371 | `				if( pObjAttr == 0 ){` |
|        - |  5372 | `					/* No such attribute,load null */` |
|        4 |  5373 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5374 | `						&pClass->sName,&sName);` |
|        - |  5375 | `					/* Call the __get magic method if available */` |
|        3 |  5376 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5377 | `				}` |
|     3786 |  5378 | `				VmPopOperand(&pTos,1);` |
|        - |  5379 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5380 | `				 * This is due to the following case:` |
|        - |  5381 | `				 *     (new TestClass())->foo;` |
|        - |  5382 | `				 */` |
|     3786 |  5383 | `				pThis->iRef++;` |
|     3786 |  5384 | `				PH7_MemObjRelease(pTos);` |
|     3786 |  5385 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3786 |  5386 | `				if( pObjAttr ){` |
|     3784 |  5387 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5388 | `					/* Check attribute access */` |
|     3784 |  5389 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5390 | `						/* Load attribute */` |
|     3784 |  5391 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3784 |  5392 | `						if( pValue ){` |
|     3784 |  5393 | `							if( pThis->iRef < 2 ){` |
|        - |  5394 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5395 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5396 | `								 */` |
|        3 |  5397 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5398 | `							}else{` |
|        - |  5399 | `								/* Simple load */` |
|     3782 |  5400 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5401 | `							}` |
|     3784 |  5402 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3782 |  5403 | `								if( pThis->iRef > 1 ){` |
|        - |  5404 | `									/* Load attribute index */` |
|     3780 |  5405 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1889 |  5406 | `								}` |
|     1890 |  5407 | `							}` |
|     1891 |  5408 | `						}` |
|     1891 |  5409 | `					}` |
|     1891 |  5410 | `				}` |
|        - |  5411 | `				/* Safely unreference the object */` |
|     3786 |  5412 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5413 | `			}` |
|     2032 |  5414 | `		}else{` |
|      ! 0 |  5415 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5416 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5417 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5418 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5419 | `		}` |
|     2032 |  5420 | `	}else{` |
|        - |  5421 | `		/* Static member access using class name */` |
|      100 |  5422 | `		pNos = pTos;` |
|      100 |  5423 | `		pThis = 0;` |
|      100 |  5424 | `		if( !pInstr->p3 ){` |
|       88 |  5425 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       88 |  5426 | `			pNos--;` |
|        - |  5427 | `#ifdef UNTRUST` |
|        - |  5428 | `			if( pNos < pStack ){` |
|        - |  5429 | `				goto Abort;` |
|        - |  5430 | `			}` |
|        - |  5431 | `#endif` |
|       45 |  5432 | `		}else{` |
|        - |  5433 | `			/* Attribute name already computed */` |
|       14 |  5434 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5435 | `		}` |
|      100 |  5436 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      100 |  5437 | `			ph7_class *pClass = 0;` |
|      100 |  5438 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5439 | `				/* Class already instantiated */` |
|      ! 0 |  5440 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5441 | `				pClass = pThis->pClass;` |
|      ! 0 |  5442 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5443 | `			}else{` |
|        - |  5444 | `				/* Try to extract the target class */` |
|      100 |  5445 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      100 |  5446 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      100 |  5447 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5448 | `					/* Handle self/static/parent keywords */` |
|      100 |  5449 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5450 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5451 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5452 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5453 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5454 | `						}` |
|       86 |  5455 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5456 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5457 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5458 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5459 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5460 | `							pClass = pSelf->pBase;` |
|        6 |  5461 | `						}` |
|        8 |  5462 | `					}else{` |
|       46 |  5463 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5464 | `					}` |
|       49 |  5465 | `				}` |
|        - |  5466 | `			}` |
|      100 |  5467 | `			if( pClass == 0 ){` |
|        - |  5468 | `				/* Undefined class */` |
|      ! 0 |  5469 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5470 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5471 | `					);` |
|      ! 0 |  5472 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5473 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5474 | `				}` |
|      ! 0 |  5475 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5476 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5477 | `			}else{` |
|      100 |  5478 | `				if( pInstr->iP2 ){` |
|        - |  5479 | `					/* Method call */` |
|       30 |  5480 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5481 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5482 | `						/* Extract the target method */` |
|       30 |  5483 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5484 | `					}` |
|       30 |  5485 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5486 | `						if( pMeth ){` |
|      ! 0 |  5487 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5488 | `								&pClass->sName,&sName` |
|        - |  5489 | `								);` |
|      ! 0 |  5490 | `						}else{` |
|      ! 0 |  5491 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5492 | `								&pClass->sName,&sName` |
|        - |  5493 | `								);` |
|        - |  5494 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5495 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5496 | `						}` |
|        - |  5497 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5498 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5499 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5500 | `						}` |
|      ! 0 |  5501 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5502 | `					}else{` |
|        - |  5503 | `						/* Push method name on the stack */` |
|       30 |  5504 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5505 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5506 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5507 | `					}` |
|       30 |  5508 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5509 | `				}else{` |
|        - |  5510 | `					/* Attribute access */` |
|       72 |  5511 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5512 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5513 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5514 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5515 | `						/* ::class returns the fully qualified class name */` |
|        - |  5516 | `						/* Pop the attribute name from the stack */` |
|       54 |  5517 | `						if( !pInstr->p3 ){` |
|       54 |  5518 | `							VmPopOperand(&pTos,1);` |
|       26 |  5519 | `						}` |
|       54 |  5520 | `						PH7_MemObjRelease(pTos);` |
|        - |  5521 | `						/* Load the class name */` |
|       54 |  5522 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5523 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5524 | `					}else{` |
|        - |  5525 | `						/* Extract the target attribute */` |
|       20 |  5526 | `						if( sName.nByte > 0 ){` |
|       20 |  5527 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5528 | `						}` |
|       20 |  5529 | `						if( pAttr == 0 ){` |
|        - |  5530 | `							/* No such attribute,load null */` |
|      ! 0 |  5531 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5532 | `								&pClass->sName,&sName);` |
|        - |  5533 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5534 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5535 | `						}` |
|        - |  5536 | `						/* Pop the attribute name from the stack */` |
|       20 |  5537 | `						if( !pInstr->p3 ){` |
|        7 |  5538 | `							VmPopOperand(&pTos,1);` |
|        3 |  5539 | `						}` |
|       20 |  5540 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5541 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5542 | `						if( pAttr ){` |
|       20 |  5543 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5544 | `								/* Access to a non static attribute */` |
|      ! 0 |  5545 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5546 | `									&pClass->sName,&pAttr->sName` |
|        - |  5547 | `									);` |
|      ! 0 |  5548 | `							}else{` |
|        - |  5549 | `								ph7_value *pValue;` |
|        - |  5550 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5551 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5552 | `									/* Load the desired attribute */` |
|       20 |  5553 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5554 | `									if( pValue ){` |
|       20 |  5555 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5556 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5557 | `											/* Load index number */` |
|       14 |  5558 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5559 | `										}` |
|        9 |  5560 | `									}` |
|        9 |  5561 | `								}` |
|        - |  5562 | `							}` |
|        9 |  5563 | `						}` |
|        - |  5564 | `					}` |
|        - |  5565 | `				}` |
|      100 |  5566 | `				if( pThis ){` |
|        - |  5567 | `					/* Safely unreference the object */` |
|      ! 0 |  5568 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5569 | `				}` |
|        - |  5570 | `			}` |
|       51 |  5571 | `		}else{` |
|        - |  5572 | `			/* Pop operands */` |
|      ! 0 |  5573 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5574 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5575 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5576 | `			}` |
|      ! 0 |  5577 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5578 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5579 | `		}` |
|        - |  5580 | `	}` |
|     4160 |  5581 | `	break;` |
|        - |  5582 | `					}` |
|        - |  5583 | `/*` |
|        - |  5584 | ` * OP_NEW P1 * * *` |
|        - |  5585 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5586 | ` */` |
|      305 |  5587 | `case PH7_OP_NEW: {` |
|      612 |  5588 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      612 |  5589 | `	ph7_class *pClass = 0;` |
|        - |  5590 | `	ph7_class_instance *pNew;` |
|      612 |  5591 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5592 | `		/* Try to extract the desired class */` |
|      917 |  5593 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      610 |  5594 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      305 |  5595 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5596 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5597 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5598 | `	}` |
|      612 |  5599 | `	if( pClass == 0 ){` |
|        - |  5600 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5601 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5602 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5603 | `			);` |
|        - |  5604 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5605 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5606 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5607 | `			/* Pop given arguments */` |
|      ! 0 |  5608 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5609 | `		}` |
|      ! 0 |  5610 | `		goto Abort;` |
|      ! 0 |  5611 | `	}else{` |
|        - |  5612 | `		ph7_class_method *pCons;` |
|        - |  5613 | `		/* Create a new class instance */` |
|      612 |  5614 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      612 |  5615 | `		if( pNew == 0 ){` |
|      ! 0 |  5616 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5617 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5618 | `				&pClass->sName` |
|        - |  5619 | `			);` |
|      ! 0 |  5620 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5621 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5622 | `				/* Pop given arguments */` |
|      ! 0 |  5623 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5624 | `			}` |
|      ! 0 |  5625 | `			break;` |
|        - |  5626 | `		}` |
|        - |  5627 | `		/* Check if a constructor is available */` |
|      612 |  5628 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      612 |  5629 | `		if( pCons == 0 ){` |
|      528 |  5630 | `			SyString *pName = &pClass->sName;` |
|        - |  5631 | `			/* Check for a constructor with the same base class name */` |
|      528 |  5632 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      263 |  5633 | `		}` |
|      612 |  5634 | `		if( pCons ){` |
|        - |  5635 | `			/* Call the class constructor */` |
|       86 |  5636 | `			SySetReset(&aArg);` |
|      160 |  5637 | `			while( pArg < pTos ){` |
|       76 |  5638 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       76 |  5639 | `				pArg++;` |
|        2 |  5640 | `			}` |
|       86 |  5641 | `			if( pVm->bErrReport ){` |
|        - |  5642 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5643 | `				sxu32 n;` |
|       43 |  5644 | `				n = SySetUsed(&aArg);` |
|        - |  5645 | `				/* Emit a notice for missing arguments */` |
|       95 |  5646 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       53 |  5647 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       53 |  5648 | `					if( pFuncArg ){` |
|       53 |  5649 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5650 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5651 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5652 | `						}` |
|       26 |  5653 | `					}` |
|       53 |  5654 | `					n++;` |
|        1 |  5655 | `				}` |
|       21 |  5656 | `			}` |
|       86 |  5657 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5658 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       86 |  5659 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5660 | `				pNew->iRef = 1;` |
|      ! 0 |  5661 | `			}` |
|       42 |  5662 | `		}` |
|      612 |  5663 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5664 | `			/* Pop given arguments */` |
|       68 |  5665 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       33 |  5666 | `		}` |
|      612 |  5667 | `		PH7_MemObjRelease(pTos);` |
|      612 |  5668 | `		pTos->x.pOther = pNew;` |
|      612 |  5669 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5670 | `	}` |
|      612 |  5671 | `	break;` |
|        - |  5672 | `				 }` |
|        - |  5673 | `/*` |
|        - |  5674 | ` * OP_CLONE * * *` |
|        - |  5675 | ` * Perfome a clone operation.` |
|        - |  5676 | ` */` |
|       23 |  5677 | `case PH7_OP_CLONE: {` |
|        - |  5678 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5679 | `#ifdef UNTRUST` |
|        - |  5680 | `	if( pTos < pStack ){` |
|        - |  5681 | `		goto Abort;` |
|        - |  5682 | `	}` |
|        - |  5683 | `#endif` |
|        - |  5684 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5685 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5686 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5687 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5688 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5689 | `		break;` |
|        - |  5690 | `	}` |
|        - |  5691 | `	/* Point to the source */` |
|       44 |  5692 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5693 | `	/* Perform the clone operation */` |
|       44 |  5694 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5695 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5696 | `	if( pClone == 0 ){` |
|      ! 0 |  5697 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5698 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5699 | `	}else{` |
|        - |  5700 | `		/* Load the cloned object */` |
|       44 |  5701 | `		pTos->x.pOther = pClone;` |
|       44 |  5702 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5703 | `	}` |
|       44 |  5704 | `	break;` |
|        - |  5705 | `				   }` |
|        - |  5706 | `/*` |
|        - |  5707 | ` * OP_SWITCH * * P3` |
|        - |  5708 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5709 | ` */` |
|       18 |  5710 | `case PH7_OP_SWITCH: {` |
|       38 |  5711 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5712 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5713 | `	ph7_value sValue,sCaseValue;` |
|        - |  5714 | `	sxu32 n,nEntry;` |
|        - |  5715 | `#ifdef UNTRUST` |
|        - |  5716 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5717 | `		goto Abort;` |
|        - |  5718 | `	}` |
|        - |  5719 | `#endif` |
|        - |  5720 | `	/* Point to the case table  */` |
|       38 |  5721 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5722 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5723 | `	/* Select the appropriate case block to execute */` |
|       38 |  5724 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5725 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5726 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5727 | `		pCase = &aCase[n];` |
|       92 |  5728 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5729 | `		/* Execute the case expression first */` |
|       92 |  5730 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5731 | `		/* Compare the two expression */` |
|       92 |  5732 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5733 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5734 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5735 | `		if( rc == 0 ){` |
|        - |  5736 | `			/* Value match,jump to this block */` |
|       38 |  5737 | `			pc = pCase->nStart - 1;` |
|       38 |  5738 | `			break;` |
|        - |  5739 | `		}` |
|       29 |  5740 | `	}` |
|       38 |  5741 | `	VmPopOperand(&pTos,1);` |
|       38 |  5742 | `	if( n >= nEntry ){` |
|        - |  5743 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5744 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5745 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5746 | `		}else{` |
|        - |  5747 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5748 | `			pc = pSwitch->nOut - 1;` |
|        - |  5749 | `		}` |
|      ! 0 |  5750 | `	}` |
|       38 |  5751 | `	break;` |
|        - |  5752 | `					}` |
|        - |  5753 | `/*` |
|        - |  5754 | ` * OP_CALL P1 * *` |
|        - |  5755 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5756 | ` *  function on the stack.` |
|        - |  5757 | ` */` |
|   284841 |  5758 | `case PH7_OP_CALL: {` |
|   569728 |  5759 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5760 | `	SyHashEntry *pEntry;` |
|        - |  5761 | `	SyString sName;` |
|        - |  5762 | `	/* Extract function name */` |
|   569728 |  5763 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5764 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5765 | `			ph7_value sResult;` |
|      ! 0 |  5766 | `			SySetReset(&aArg);` |
|      ! 0 |  5767 | `			while( pArg < pTos ){` |
|      ! 0 |  5768 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5769 | `				pArg++;` |
|      ! 0 |  5770 | `			}` |
|      ! 0 |  5771 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5772 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5773 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5774 | `			SySetReset(&aArg);` |
|        - |  5775 | `			/* Pop given arguments */` |
|      ! 0 |  5776 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5777 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5778 | `			}` |
|        - |  5779 | `			/* Copy result */` |
|      ! 0 |  5780 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5781 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5782 | `		}else{` |
|        3 |  5783 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5784 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5785 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5786 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5787 | `			}else{` |
|        - |  5788 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5789 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5790 | `			}` |
|        - |  5791 | `			/* Pop given arguments */` |
|        3 |  5792 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5793 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5794 | `			}` |
|        - |  5795 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5796 | `			PH7_MemObjRelease(pTos);` |
|        - |  5797 | `		}` |
|   284608 |  5798 | `		break;` |
|        - |  5799 | `	}` |
|   569726 |  5800 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5801 | `	/* Check for a compiled function first.` |
|        - |  5802 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5803 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   569726 |  5804 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5805 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5806 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5807 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5808 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5809 | `	 * function calls inside namespaces. */` |
|   569726 |  5810 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5811 | `		const char *zFunc;` |
|        - |  5812 | `		const char *zEnd;` |
|        - |  5813 | `		const char *z;` |
|        - |  5814 | `		SyString sGlobal;` |
|       15 |  5815 | `		zFunc = sName.zString;` |
|       15 |  5816 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5817 | `		z = zEnd;` |
|        - |  5818 | `		/* Find last namespace separator */` |
|      133 |  5819 | `		while( z > zFunc ){` |
|      133 |  5820 | `			if( z[-1] == '\\' ){` |
|       15 |  5821 | `				break;` |
|        - |  5822 | `			}` |
|      119 |  5823 | `			z--;` |
|        1 |  5824 | `		}` |
|       15 |  5825 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5826 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5827 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5828 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5829 | `		}` |
|        7 |  5830 | `	}` |
|   569726 |  5831 | `	if( pEntry ){` |
|        - |  5832 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5833 | `		ph7_class_instance *pThis;` |
|        - |  5834 | `		ph7_value *pFrameStack;` |
|        - |  5835 | `		ph7_vm_func *pVmFunc;` |
|        - |  5836 | `		ph7_class *pSelf;` |
|        - |  5837 | `		VmFrame *pFrame;` |
|        - |  5838 | `		ph7_value *pObj;` |
|        - |  5839 | `		VmSlot sArg;` |
|        - |  5840 | `		sxu32 n;` |
|        - |  5841 | `		/* initialize fields */` |
|    12536 |  5842 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12536 |  5843 | `		pThis = 0;` |
|    12536 |  5844 | `		pSelf = 0;` |
|    12536 |  5845 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5846 | `			ph7_class_method *pMeth;` |
|        - |  5847 | `			/* Class method call */` |
|     1604 |  5848 | `			ph7_value *pTarget = &pTos[-1];` |
|     1604 |  5849 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5850 | `				/* Extract the 'this' pointer */` |
|     1604 |  5851 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5852 | `					/* Instance already loaded */` |
|     1570 |  5853 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1570 |  5854 | `					pThis->iRef++;` |
|     1570 |  5855 | `					pSelf = pThis->pClass;` |
|      784 |  5856 | `				}` |
|     1604 |  5857 | `				if( pSelf == 0 ){` |
|       36 |  5858 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5859 | `						/* "Late Static Binding" class name */` |
|       44 |  5860 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5861 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5862 | `					}` |
|       36 |  5863 | `					if( pSelf == 0 ){` |
|       13 |  5864 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5865 | `					}` |
|       17 |  5866 | `				}` |
|     1604 |  5867 | `				if( pThis == 0  ){` |
|       36 |  5868 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5869 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       36 |  5870 | `					if( pFrameLocal->pParent ){` |
|        - |  5871 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5872 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5873 | `						if( pThis ){` |
|       13 |  5874 | `							pThis->iRef++;` |
|        6 |  5875 | `						}` |
|        9 |  5876 | `					}` |
|       17 |  5877 | `				}` |
|     1604 |  5878 | `				VmPopOperand(&pTos,1);` |
|     1604 |  5879 | `				PH7_MemObjRelease(pTos);` |
|        - |  5880 | `				/* Synchronize pointers */` |
|     1604 |  5881 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5882 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5883 | `				 * user have already computed the random generated unique class method name` |
|        - |  5884 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5885 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5886 | `				 */` |
|     1604 |  5887 | `				while( pArg < pStack ){` |
|      ! 0 |  5888 | `					pArg++;` |
|      ! 0 |  5889 | `				}` |
|     1604 |  5890 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5891 | `					/* Check if the call is allowed */` |
|     1604 |  5892 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1604 |  5893 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5894 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5895 | `							/* Pop given arguments */` |
|      ! 0 |  5896 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5897 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5898 | `							}` |
|        - |  5899 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5900 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5901 | `							break;` |
|        - |  5902 | `						}` |
|        3 |  5903 | `					}` |
|      801 |  5904 | `				}` |
|      801 |  5905 | `			}` |
|      801 |  5906 | `		}` |
|        - |  5907 | `		/* Check The recursion limit */` |
|    12536 |  5908 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5909 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5910 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5911 | `				&pVmFunc->sName);` |
|        - |  5912 | `			/* Pop given arguments */` |
|        3 |  5913 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5914 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5915 | `			}` |
|        - |  5916 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5917 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5918 | `			break;` |
|        - |  5919 | `		}` |
|    12534 |  5920 | `		if( pVmFunc->pNextName ){` |
|        - |  5921 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5922 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5923 | `		}` |
|        - |  5924 | `		/* Extract the formal argument set */` |
|    12534 |  5925 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5926 | `		/* Create a new VM frame  */` |
|    12534 |  5927 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12534 |  5928 | `		if( rc != SXRET_OK ){` |
|        - |  5929 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5930 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5931 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5932 | `				&pVmFunc->sName);` |
|        - |  5933 | `			/* Pop given arguments */` |
|      ! 0 |  5934 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5935 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5936 | `			}` |
|        - |  5937 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5938 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5939 | `			break;` |
|        - |  5940 | `		}` |
|    12534 |  5941 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5942 | `			/* Install the '$this' variable */` |
|        - |  5943 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1580 |  5944 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1580 |  5945 | `			if( pObj ){` |
|        - |  5946 | `				/* Reflect the change */` |
|     1580 |  5947 | `				pObj->x.pOther = pThis;` |
|     1580 |  5948 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      789 |  5949 | `			}` |
|      789 |  5950 | `		}` |
|    12534 |  5951 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5952 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5953 | `			/* Install static variables */` |
|      ! 0 |  5954 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5955 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5956 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5957 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5958 | `					/* Initialize the static variables */` |
|      ! 0 |  5959 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5960 | `					if( pObj ){` |
|        - |  5961 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5962 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5963 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5964 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5965 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5966 | `						}` |
|      ! 0 |  5967 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5968 | `					}else{` |
|      ! 0 |  5969 | `						continue;` |
|        - |  5970 | `					}` |
|      ! 0 |  5971 | `				}` |
|        - |  5972 | `				/* Install in the current frame */` |
|      ! 0 |  5973 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5974 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5975 | `			}` |
|      ! 0 |  5976 | `		}` |
|        - |  5977 | `		/* Push arguments in the local frame */` |
|    12534 |  5978 | `		n = 0;` |
|    34582 |  5979 | `		while( pArg < pTos ){` |
|    22050 |  5980 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21900 |  5981 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5982 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5983 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5984 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5985 | `						goto Abort;` |
|        - |  5986 | `					}` |
|      ! 0 |  5987 | `				}` |
|        - |  5988 | `				/* Make sure the given arguments are of the correct type */` |
|    21900 |  5989 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5990 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5991 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5992 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5993 | `						ph7_class *pClass;` |
|        - |  5994 | `						/* Try to extract the desired class */` |
|      ! 0 |  5995 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5996 | `						if( pClass ){` |
|      ! 0 |  5997 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5998 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5999 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6000 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6001 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6002 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6003 | `								}` |
|      ! 0 |  6004 | `							}else{` |
|        - |  6005 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  6006 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6007 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  6008 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6009 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6010 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6011 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6012 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6013 | `								}` |
|        - |  6014 | `							}` |
|      ! 0 |  6015 | `						}` |
|     1088 |  6016 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6017 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6018 | `						/* Cast to the desired type */` |
|      ! 0 |  6019 | `						xCast(pArg);` |
|      ! 0 |  6020 | `					}` |
|      543 |  6021 | `				}` |
|    21900 |  6022 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6023 | `					/* Pass by reference */` |
|       50 |  6024 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6025 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6026 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6027 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6028 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6029 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6030 | `						}` |
|        - |  6031 | `						/* Switch to pass by value */` |
|      ! 0 |  6032 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6033 | `					}else{` |
|        - |  6034 | `						SyHashEntry *pRefEntry;` |
|        - |  6035 | `						/* Install the referenced variable in the private function frame */` |
|       50 |  6036 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       50 |  6037 | `						if( pRefEntry == 0 ){` |
|       74 |  6038 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       48 |  6039 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       50 |  6040 | `							sArg.nIdx = pArg->nIdx;` |
|       50 |  6041 | `							sArg.pUserData = 0;` |
|       50 |  6042 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  6043 | `						}` |
|       50 |  6044 | `						pObj = 0;` |
|        - |  6045 | `					}` |
|       26 |  6046 | `				}else{` |
|        - |  6047 | `					/* Pass by value,make a copy of the given argument */` |
|    21852 |  6048 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6049 | `				}` |
|    10951 |  6050 | `			}else{` |
|        - |  6051 | `				char zName[32];` |
|        - |  6052 | `				SyString sArgName;` |
|        - |  6053 | `				/* Set a dummy name */` |
|      152 |  6054 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  6055 | `				sArgName.zString = zName;` |
|        - |  6056 | `				/* Annonymous argument */` |
|      152 |  6057 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6058 | `			}` |
|    22050 |  6059 | `			if( pObj ){` |
|    22002 |  6060 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6061 | `				/* Insert argument index  */` |
|    22002 |  6062 | `				sArg.nIdx = pObj->nIdx;` |
|    22002 |  6063 | `				sArg.pUserData = 0;` |
|    22002 |  6064 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11000 |  6065 | `			}` |
|    22050 |  6066 | `			PH7_MemObjRelease(pArg);` |
|    22050 |  6067 | `			pArg++;` |
|    22050 |  6068 | `			++n;` |
|        2 |  6069 | `		}` |
|        - |  6070 | `		/* Set up closure environment */` |
|    12534 |  6071 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6072 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6073 | `			ph7_value *pValue;` |
|        - |  6074 | `			sxu32 iEnv;` |
|       11 |  6075 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6076 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6077 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6078 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6079 | `					/* Do not install null value */` |
|       11 |  6080 | `					continue;` |
|        - |  6081 | `				}` |
|       11 |  6082 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6083 | `				if( pValue == 0 ){` |
|      ! 0 |  6084 | `					continue;` |
|        - |  6085 | `				}` |
|        - |  6086 | `				/* Invalidate any prior representation */` |
|       11 |  6087 | `				PH7_MemObjRelease(pValue);` |
|        - |  6088 | `				/* Duplicate bound variable value */` |
|       11 |  6089 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6090 | `			}` |
|        5 |  6091 | `		}` |
|        - |  6092 | `		/* Process default values */` |
|    14398 |  6093 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1866 |  6094 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1860 |  6095 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1860 |  6096 | `				if( pObj ){` |
|        - |  6097 | `					/* Evaluate the default value and extract it's result */` |
|     1860 |  6098 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1860 |  6099 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6100 | `						goto Abort;` |
|        - |  6101 | `					}` |
|        - |  6102 | `					/* Insert argument index */` |
|     1860 |  6103 | `					sArg.nIdx = pObj->nIdx;` |
|     1860 |  6104 | `					sArg.pUserData = 0;` |
|     1860 |  6105 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6106 | `					/* Make sure the default argument is of the correct type */` |
|     1860 |  6107 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6108 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6109 | `						/* Cast to the desired type */` |
|      ! 0 |  6110 | `						xCast(pObj);` |
|      ! 0 |  6111 | `					}` |
|      929 |  6112 | `				}` |
|      929 |  6113 | `			}` |
|     1866 |  6114 | `			++n;` |
|        2 |  6115 | `		}` |
|        - |  6116 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6117 | `		 * does not return anything.` |
|        - |  6118 | `		 */` |
|    12534 |  6119 | `		PH7_MemObjRelease(pTos);` |
|    12534 |  6120 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6121 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12534 |  6122 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12534 |  6123 | `		if( pFrameStack == 0 ){` |
|        - |  6124 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6125 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6126 | `				&pVmFunc->sName);` |
|      ! 0 |  6127 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6128 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6129 | `			}` |
|      ! 0 |  6130 | `			break;` |
|        - |  6131 | `		}` |
|    12534 |  6132 | `		if( pSelf ){` |
|        - |  6133 | `			/* Push class name */` |
|     1602 |  6134 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      800 |  6135 | `		}` |
|        - |  6136 | `		/* Increment nesting level */` |
|    12534 |  6137 | `		pVm->nRecursionDepth++;` |
|        - |  6138 | `		/* Execute function body */` |
|    12534 |  6139 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  6140 | `		/* Decrement nesting level */` |
|    12534 |  6141 | `		pVm->nRecursionDepth--;` |
|    12534 |  6142 | `		if( pSelf ){` |
|        - |  6143 | `			/* Pop class name */` |
|     1602 |  6144 | `			(void)SySetPop(&pVm->aSelf);` |
|      800 |  6145 | `		}` |
|        - |  6146 | `		/* Cleanup the mess left behind */` |
|    12534 |  6147 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6148 | `			/* Return by reference,reflect that */` |
|        9 |  6149 | `			if( n != SXU32_HIGH ){` |
|        9 |  6150 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6151 | `				sxu32 i;` |
|        - |  6152 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6153 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6154 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6155 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6156 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6157 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6158 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6159 | `								&pVmFunc->sName);` |
|      ! 0 |  6160 | `						}` |
|      ! 0 |  6161 | `						n = SXU32_HIGH;` |
|      ! 0 |  6162 | `						break;` |
|        - |  6163 | `					}` |
|        3 |  6164 | `				}` |
|        5 |  6165 | `			}else{` |
|      ! 0 |  6166 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6167 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6168 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6169 | `						&pVmFunc->sName);` |
|      ! 0 |  6170 | `				}` |
|        - |  6171 | `			}` |
|        9 |  6172 | `			pTos->nIdx = n;` |
|        4 |  6173 | `		}` |
|        - |  6174 | `		/* Cleanup the mess left behind */` |
|    12534 |  6175 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6176 | `			/* An exception was throw in this frame */` |
|        7 |  6177 | `			pFrame = pFrame->pParent;` |
|        7 |  6178 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6179 | `				/* Pop the resutlt */` |
|        5 |  6180 | `				VmPopOperand(&pTos,1);` |
|        - |  6181 | `				/* Jump to this destination */` |
|        5 |  6182 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  6183 | `				rc = PH7_OK;` |
|        3 |  6184 | `			}else{` |
|        3 |  6185 | `				if( pFrame->pParent ){` |
|        3 |  6186 | `					rc = PH7_EXCEPTION;` |
|        2 |  6187 | `				}else{` |
|        - |  6188 | `					/* Continue normal execution */` |
|      ! 0 |  6189 | `					rc = PH7_OK;` |
|        - |  6190 | `				}` |
|        - |  6191 | `			}` |
|        3 |  6192 | `		}` |
|        - |  6193 | `		/* Free the operand stack */` |
|    12534 |  6194 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6195 | `		/* Leave the frame */` |
|    12534 |  6196 | `		VmLeaveFrame(&(*pVm));` |
|    12534 |  6197 | `		if( rc == PH7_ABORT ){` |
|        - |  6198 | `			/* Abort processing immeditaley */` |
|        7 |  6199 | `			goto Abort;` |
|    12528 |  6200 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6201 | `			goto Exception;` |
|        - |  6202 | `		}` |
|     6264 |  6203 | `	}else{` |
|        - |  6204 | `		ph7_user_func *pFunc;` |
|        - |  6205 | `		ph7_context sCtx;` |
|        - |  6206 | `		ph7_value sRet;` |
|        - |  6207 | `		/* Look for an installed foreign function.` |
|        - |  6208 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6209 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6210 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6211 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6212 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6213 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   557192 |  6214 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   557192 |  6215 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6216 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6217 | `			const char *zShort = sName.zString;` |
|        - |  6218 | `			sxu32 i;` |
|      217 |  6219 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6220 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6221 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6222 | `				}` |
|      102 |  6223 | `			}` |
|       15 |  6224 | `			if( zShort != sName.zString ){` |
|       15 |  6225 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6226 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6227 | `			}` |
|        7 |  6228 | `		}` |
|   557192 |  6229 | `		if( pEntry == 0 ){` |
|        - |  6230 | `			/* Call to undefined function */` |
|        5 |  6231 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6232 | `			/* Pop given arguments */` |
|        5 |  6233 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6234 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6235 | `			}` |
|        - |  6236 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6237 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6238 | `			break;` |
|        - |  6239 | `		}` |
|   557188 |  6240 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6241 | `		/* Start collecting function arguments */` |
|   557188 |  6242 | `		SySetReset(&aArg);` |
|  1493100 |  6243 | `		while( pArg < pTos ){` |
|   935914 |  6244 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   935914 |  6245 | `			pArg++;` |
|        2 |  6246 | `		}` |
|        - |  6247 | `		/* Assume a null return value */` |
|   557188 |  6248 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6249 | `		/* Init the call context */` |
|   557188 |  6250 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6251 | `		/* Call the foreign function */` |
|   557188 |  6252 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6253 | `		/* Release the call context */` |
|   557188 |  6254 | `		VmReleaseCallContext(&sCtx);` |
|   557188 |  6255 | `		if( rc == PH7_ABORT ){` |
|      463 |  6256 | `			goto Abort;` |
|   556726 |  6257 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6258 | `			VmFrame *pFrm = pVm->pFrame;` |
|        7 |  6259 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|        7 |  6260 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6261 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6262 | `				goto Exception;` |
|        - |  6263 | `			}` |
|        - |  6264 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6265 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6266 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6267 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6268 | `			}` |
|        - |  6269 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6270 | `			VmPopOperand(&pTos,1);` |
|        - |  6271 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6272 | `			pFrm = pVm->pFrame;` |
|        7 |  6273 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6274 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6275 | `			}` |
|        7 |  6276 | `			break;` |
|        - |  6277 | `		}` |
|   556720 |  6278 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6279 | `			/* Pop function name and arguments */` |
|   539220 |  6280 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   269631 |  6281 | `		}` |
|        - |  6282 | `		/* Save foreign function return value */` |
|   556720 |  6283 | `		PH7_MemObjStore(&sRet,pTos);` |
|   556720 |  6284 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6285 | `	}` |
|   569244 |  6286 | `	break;` |
|        - |  6287 | `				  }` |
|        - |  6288 | `/*` |
|        - |  6289 | ` * OP_CONSUME: P1 * *` |
|        - |  6290 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6291 | ` */` |
|    11044 |  6292 | `case PH7_OP_CONSUME: {` |
|    22090 |  6293 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22090 |  6294 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6295 |  |
|    22090 |  6296 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22090 |  6297 | `	pCur = pOut;` |
|        - |  6298 | `	/* Start the consume process  */` |
|    44178 |  6299 | `	while( pOut <= pTos ){` |
|        - |  6300 | `		/* Force a string cast */` |
|    22090 |  6301 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6302 | `			PH7_MemObjToString(pOut);` |
|      149 |  6303 | `		}` |
|    22090 |  6304 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6305 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6306 | `			/* Invoke the output consumer callback */` |
|    12160 |  6307 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12160 |  6308 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6309 | `				/* Increment output length */` |
|     5584 |  6310 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2791 |  6311 | `			}` |
|    12160 |  6312 | `			SyBlobRelease(&pOut->sBlob);` |
|    12160 |  6313 | `			if( rc == SXERR_ABORT ){` |
|        - |  6314 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6315 | `				goto Abort;` |
|        - |  6316 | `			}` |
|     6079 |  6317 | `		}` |
|    22090 |  6318 | `		pOut++;` |
|        2 |  6319 | `	}` |
|    22090 |  6320 | `	pTos = &pCur[-1];` |
|    22088 |  6321 | `	break;` |
|        - |  6322 | `					 }` |
|        - |  6323 |  |
|        - |  6324 | `		} /* Switch() */` |
|  9749446 |  6325 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6326 | `	} /* For(;;) */` |
|    15380 |  6327 | `Done:` |
|    30762 |  6328 | `	SySetRelease(&aArg);` |
|    30762 |  6329 | `	return SXRET_OK;` |
|      238 |  6330 | `Abort:` |
|      477 |  6331 | `	SySetRelease(&aArg);` |
|     1661 |  6332 | `	while( pTos >= pStack ){` |
|     1185 |  6333 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6334 | `		pTos--;` |
|        1 |  6335 | `	}` |
|      477 |  6336 | `	return PH7_ABORT;` |
|        1 |  6337 | `Exception:` |
|        3 |  6338 | `	SySetRelease(&aArg);` |
|        5 |  6339 | `	while( pTos >= pStack ){` |
|        3 |  6340 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6341 | `		pTos--;` |
|        1 |  6342 | `	}` |
|        3 |  6343 | `	return PH7_EXCEPTION;` |
|    15621 |  6344 |  |
|        - |  6345 | `/*` |
|        - |  6346 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6347 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6348 | ` * See block-comment on that function for additional information.` |
|        - |  6349 | ` */` |
|    14682 |  6350 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6351 |  |
|        - |  6352 | `	ph7_value *pStack;` |
|        - |  6353 | `	sxi32 rc;` |
|        - |  6354 | `	/* Allocate a new operand stack */` |
|    14684 |  6355 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14684 |  6356 | `	if( pStack == 0 ){` |
|      ! 0 |  6357 | `		return SXERR_MEM;` |
|        - |  6358 | `	}` |
|        - |  6359 | `	/* Execute the program */` |
|    14684 |  6360 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6361 | `	/* Free the operand stack */` |
|    14684 |  6362 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6363 | `	/* Execution result */` |
|    14684 |  6364 | `	return rc;` |
|     7343 |  6365 |  |
|        - |  6366 | `/*` |
|        - |  6367 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6368 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6369 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6370 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6371 | ` * execution ends.` |
|        - |  6372 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6373 | ` * additional information.` |
|        - |  6374 | ` */` |
|     2280 |  6375 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6376 |  |
|        - |  6377 | `	VmShutdownCB *pEntry;` |
|        - |  6378 | `	ph7_value *apArg[10];` |
|        - |  6379 | `	sxu32 n,nEntry;` |
|        - |  6380 | `	int i;` |
|        - |  6381 | `	/* Point to the stack of registered callbacks */` |
|     2282 |  6382 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25082 |  6383 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    22802 |  6384 | `		apArg[i] = 0;` |
|    11402 |  6385 | `	}` |
|     2284 |  6386 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6387 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6388 | `		if( pEntry ){` |
|        - |  6389 | `			/* Prepare callback arguments if any */` |
|        3 |  6390 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6391 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6392 | `					break;` |
|        - |  6393 | `				}` |
|      ! 0 |  6394 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6395 | `			}` |
|        - |  6396 | `			/* Invoke the callback */` |
|        3 |  6397 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6398 | `			/*` |
|        - |  6399 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6400 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6401 | `			 */` |
|        3 |  6402 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6403 | `			if( pEntry ){` |
|        3 |  6404 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6405 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6406 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6407 | `				}` |
|        1 |  6408 | `			}` |
|        1 |  6409 | `		}` |
|        2 |  6410 | `	}` |
|     2282 |  6411 | `	SySetReset(&pVm->aShutdown);` |
|     2282 |  6412 |  |
|        - |  6413 | `/*` |
|        - |  6414 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6415 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6416 | ` * See block-comment on that function for additional information.` |
|        - |  6417 | ` */` |
|     2288 |  6418 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6419 |  |
|        - |  6420 | `	/* Make sure we are ready to execute this program */` |
|     2290 |  6421 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6422 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6423 | `	}` |
|        - |  6424 | `	/* Set the execution magic number  */` |
|     2290 |  6425 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6426 | `	/* Execute the program */` |
|     2290 |  6427 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6428 | `	/* Invoke any shutdown callbacks */` |
|     2286 |  6429 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6430 | `	/*` |
|        - |  6431 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6432 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6433 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6434 | `	 */` |
|     2286 |  6435 | `	return SXRET_OK;` |
|     1146 |  6436 |  |
|        - |  6437 | `/*` |
|        - |  6438 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6439 | ` * the desired message.` |
|        - |  6440 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6441 | ` * in 'api.c' for additional information.` |
|        - |  6442 | ` */` |
|      350 |  6443 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6444 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6445 | `	SyString *pString /* Message to output */` |
|        - |  6446 | `	)` |
|        2 |  6447 |  |
|      352 |  6448 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6449 | `	sxi32 rc = SXRET_OK;` |
|        - |  6450 | `	/* Call the output consumer */` |
|      352 |  6451 | `	if( pString->nByte > 0 ){` |
|      352 |  6452 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6453 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6454 | `			/* Increment output length */` |
|       17 |  6455 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6456 | `		}` |
|      175 |  6457 | `	}` |
|      352 |  6458 | `	return rc;` |
|        2 |  6459 |  |
|        - |  6460 | `/*` |
|        - |  6461 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6462 | ` * callback to consume the formatted message.` |
|        - |  6463 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6464 | ` * in 'api.c' for additional information.` |
|        - |  6465 | ` */` |
|        2 |  6466 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6467 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6468 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6469 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6470 | `	)` |
|        1 |  6471 |  |
|        3 |  6472 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6473 | `	sxi32 rc = SXRET_OK;` |
|        - |  6474 | `	SyBlob sWorker;` |
|        - |  6475 | `	/* Format the message and call the output consumer */` |
|        3 |  6476 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6477 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6478 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6479 | `		/* Consume the formatted message */` |
|        3 |  6480 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6481 | `	}` |
|        3 |  6482 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6483 | `		/* Increment output length */` |
|      ! 0 |  6484 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6485 | `	}` |
|        - |  6486 | `	/* Release the working buffer */` |
|        3 |  6487 | `	SyBlobRelease(&sWorker);` |
|        3 |  6488 | `	return rc;` |
|        1 |  6489 |  |
|        - |  6490 | `/*` |
|        - |  6491 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6492 | ` * This function never fail and always return a pointer` |
|        - |  6493 | ` * to a null terminated string.` |
|        - |  6494 | ` */` |
|       12 |  6495 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6496 |  |
|       13 |  6497 | `	const char *zOp = "Unknown     ";` |
|       13 |  6498 | `	switch(nOp){` |
|        3 |  6499 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6500 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6501 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6502 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6503 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6504 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6505 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6506 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6507 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6508 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6509 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6510 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6511 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6512 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6513 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6514 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6515 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6516 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6517 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6518 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6519 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6520 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6521 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6522 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6523 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6524 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6525 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6526 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6527 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6528 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6529 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6530 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6531 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6532 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6533 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6534 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6535 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6536 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6537 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6538 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6539 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6540 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6541 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6542 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6543 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6544 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6545 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6546 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6547 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6548 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6549 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6550 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6551 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6552 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6553 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6554 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6555 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6556 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6557 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6558 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6559 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6560 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6561 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6562 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6563 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6564 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6565 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6566 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6567 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6568 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6569 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6570 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6571 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6572 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6573 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6574 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6575 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6576 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6577 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6578 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6579 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6580 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6581 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6582 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6583 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6584 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6585 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6586 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6587 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6588 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6589 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6590 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6591 | `	default:` |
|      ! 0 |  6592 | `		break;` |
|        - |  6593 | `	}` |
|       13 |  6594 | `	return zOp;` |
|        1 |  6595 |  |
|        - |  6596 | `/*` |
|        - |  6597 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6598 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6599 | ` * is responsible of consuming the generated dump.` |
|        - |  6600 | ` */` |
|        2 |  6601 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6602 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6603 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6604 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6605 | `	)` |
|        1 |  6606 |  |
|        - |  6607 | `	sxi32 rc;` |
|        3 |  6608 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6609 | `	return rc;` |
|        1 |  6610 |  |
|        - |  6611 | `/*` |
|        - |  6612 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6613 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6614 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6615 | ` * in 'compile.c' for additional information.` |
|        - |  6616 | ` */` |
|        8 |  6617 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6618 |  |
|        9 |  6619 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6620 | `	/* Evaluate and expand constant value */` |
|        9 |  6621 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6622 |  |
|        - |  6623 | `/*` |
|        - |  6624 | ` * Section:` |
|        - |  6625 | ` *  Function handling functions.` |
|        - |  6626 | ` * Status:` |
|        - |  6627 | ` *    Stable.` |
|        - |  6628 | ` */` |
|        - |  6629 | `/*` |
|        - |  6630 | ` * int func_num_args(void)` |
|        - |  6631 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6632 | ` * Parameters` |
|        - |  6633 | ` *   None.` |
|        - |  6634 | ` * Return` |
|        - |  6635 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6636 | ` *  or -1 if called from the globe scope.` |
|        - |  6637 | ` */` |
|      906 |  6638 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6639 |  |
|        - |  6640 | `	VmFrame *pFrame;` |
|        - |  6641 | `	ph7_vm *pVm;` |
|        - |  6642 | `	/* Point to the target VM */` |
|      908 |  6643 | `	pVm = pCtx->pVm;` |
|        - |  6644 | `	/* Current frame */` |
|      908 |  6645 | `	pFrame = pVm->pFrame;` |
|      908 |  6646 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      908 |  6647 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6648 | `		SXUNUSED(nArg);` |
|      ! 0 |  6649 | `		SXUNUSED(apArg);` |
|        - |  6650 | `		/* Global frame,return -1 */` |
|      ! 0 |  6651 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6652 | `		return SXRET_OK;` |
|        - |  6653 | `	}` |
|        - |  6654 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6655 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6656 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6657 | `	return SXRET_OK;` |
|      455 |  6658 |  |
|        - |  6659 | `/*` |
|        - |  6660 | ` * value func_get_arg(int $arg_num)` |
|        - |  6661 | ` *   Return an item from the argument list.` |
|        - |  6662 | ` * Parameters` |
|        - |  6663 | ` *  Argument number(index start from zero).` |
|        - |  6664 | ` * Return` |
|        - |  6665 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6666 | ` */` |
|       22 |  6667 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6668 |  |
|       24 |  6669 | `	ph7_value *pObj = 0;` |
|       24 |  6670 | `	VmSlot *pSlot = 0;` |
|        - |  6671 | `	VmFrame *pFrame;` |
|        - |  6672 | `	ph7_vm *pVm;` |
|        - |  6673 | `	/* Point to the target VM */` |
|       24 |  6674 | `	pVm = pCtx->pVm;` |
|        - |  6675 | `	/* Current frame */` |
|       24 |  6676 | `	pFrame = pVm->pFrame;` |
|       24 |  6677 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  6678 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6679 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6680 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6681 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6682 | `		return SXRET_OK;` |
|        - |  6683 | `	}` |
|        - |  6684 | `	/* Extract the desired index */` |
|       21 |  6685 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6686 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6687 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6688 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6689 | `		return SXRET_OK;` |
|        - |  6690 | `	}` |
|        - |  6691 | `	/* Extract the desired argument */` |
|       21 |  6692 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6693 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6694 | `			/* Return the desired argument */` |
|       21 |  6695 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6696 | `		}else{` |
|        - |  6697 | `			/* No such argument,return false */` |
|      ! 0 |  6698 | `			ph7_result_bool(pCtx,0);` |
|        - |  6699 | `		}` |
|       11 |  6700 | `	}else{` |
|        - |  6701 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6702 | `		ph7_result_bool(pCtx,0);` |
|        - |  6703 | `	}` |
|       21 |  6704 | `	return SXRET_OK;` |
|       13 |  6705 |  |
|        - |  6706 | `/*` |
|        - |  6707 | ` * array func_get_args_byref(void)` |
|        - |  6708 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6709 | ` * Parameters` |
|        - |  6710 | ` *  None.` |
|        - |  6711 | ` * Return` |
|        - |  6712 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6713 | ` *  member of the current user-defined function's argument list.` |
|        - |  6714 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6715 | ` * NOTE:` |
|        - |  6716 | ` *  Arguments are returned to the array by reference.` |
|        - |  6717 | ` */` |
|        2 |  6718 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6719 |  |
|        - |  6720 | `	ph7_value *pArray;` |
|        - |  6721 | `	VmFrame *pFrame;` |
|        - |  6722 | `	VmSlot *aSlot;` |
|        - |  6723 | `	sxu32 n;` |
|        - |  6724 | `	/* Point to the current frame */` |
|        3 |  6725 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6726 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  6727 | `	if( pFrame->pParent == 0 ){` |
|        - |  6728 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6729 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6730 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6731 | `		return SXRET_OK;` |
|        - |  6732 | `	}` |
|        - |  6733 | `	/* Create a new array */` |
|        3 |  6734 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6735 | `	if( pArray == 0 ){` |
|      ! 0 |  6736 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6737 | `		SXUNUSED(apArg);` |
|      ! 0 |  6738 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6739 | `		return SXRET_OK;` |
|        - |  6740 | `	}` |
|        - |  6741 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6742 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6743 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6744 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6745 | `	}` |
|        - |  6746 | `	/* Return the freshly created array */` |
|        3 |  6747 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6748 | `	return SXRET_OK;` |
|        2 |  6749 |  |
|        - |  6750 | `/*` |
|        - |  6751 | ` * array func_get_args(void)` |
|        - |  6752 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6753 | ` * Parameters` |
|        - |  6754 | ` *  None.` |
|        - |  6755 | ` * Return` |
|        - |  6756 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6757 | ` *  member of the current user-defined function's argument list.` |
|        - |  6758 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6759 | ` */` |
|       62 |  6760 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6761 |  |
|       64 |  6762 | `	ph7_value *pObj = 0;` |
|        - |  6763 | `	ph7_value *pArray;` |
|        - |  6764 | `	VmFrame *pFrame;` |
|        - |  6765 | `	VmSlot *aSlot;` |
|        - |  6766 | `	sxu32 n;` |
|        - |  6767 | `	/* Point to the current frame */` |
|       64 |  6768 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6769 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       64 |  6770 | `	if( pFrame->pParent == 0 ){` |
|        - |  6771 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6772 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6773 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6774 | `		return SXRET_OK;` |
|        - |  6775 | `	}` |
|        - |  6776 | `	/* Create a new array */` |
|       64 |  6777 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6778 | `	if( pArray == 0 ){` |
|      ! 0 |  6779 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6780 | `		SXUNUSED(apArg);` |
|      ! 0 |  6781 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6782 | `		return SXRET_OK;` |
|        - |  6783 | `	}` |
|        - |  6784 | `	/* Start filling the array with the given arguments */` |
|       64 |  6785 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6786 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6787 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6788 | `		if( pObj ){` |
|      130 |  6789 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6790 | `		}` |
|       66 |  6791 | `	}` |
|        - |  6792 | `	/* Return the freshly created array */` |
|       64 |  6793 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6794 | `	return SXRET_OK;` |
|       33 |  6795 |  |
|        - |  6796 | `/*` |
|        - |  6797 | ` * bool function_exists(string $name)` |
|        - |  6798 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6799 | ` * Parameters` |
|        - |  6800 | ` *  The name of the desired function.` |
|        - |  6801 | ` * Return` |
|        - |  6802 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6803 | ` */` |
|     1644 |  6804 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6805 |  |
|        - |  6806 | `	const char *zName;` |
|        - |  6807 | `	ph7_vm *pVm;` |
|        - |  6808 | `	int nLen;` |
|        - |  6809 | `	int res;` |
|     1646 |  6810 | `	if( nArg < 1 ){` |
|        - |  6811 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6812 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6813 | `		return SXRET_OK;` |
|        - |  6814 | `	}` |
|        - |  6815 | `	/* Point to the target VM */` |
|     1646 |  6816 | `	pVm = pCtx->pVm;` |
|        - |  6817 | `	/* Extract the function name */` |
|     1646 |  6818 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6819 | `	/* Assume the function is not defined */` |
|     1646 |  6820 | `	res = 0;` |
|        - |  6821 | `	/* Perform the lookup */` |
|     2466 |  6822 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1640 |  6823 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6824 | `			/* Function is defined */` |
|      206 |  6825 | `			res = 1;` |
|      102 |  6826 | `	}` |
|     1646 |  6827 | `	ph7_result_bool(pCtx,res);` |
|     1646 |  6828 | `	return SXRET_OK;` |
|      824 |  6829 |  |
|        - |  6830 | `/*` |
|        - |  6831 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6832 | ` * [i.e: Whether it is callable or not].` |
|        - |  6833 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6834 | ` */` |
|    16232 |  6835 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6836 |  |
|    16234 |  6837 | `	int res = 0;` |
|    16234 |  6838 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6839 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6840 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6841 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6842 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6843 | `		if( pMethod && CallInvoke ){` |
|        - |  6844 | `			ph7_value sResult;` |
|        - |  6845 | `			sxi32 rc;` |
|        - |  6846 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6847 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6848 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6849 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6850 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6851 | `			}` |
|      ! 0 |  6852 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6853 | `		}` |
|    16234 |  6854 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6855 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6856 | `		if( pMap->nEntry == 2 ){` |
|        - |  6857 | `			ph7_class *pClass;` |
|        - |  6858 | `			ph7_value *pV;` |
|        - |  6859 | `			/* Extract the target class */` |
|       12 |  6860 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6861 | `			if( pV ){` |
|       12 |  6862 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6863 | `				if( pClass ){` |
|        - |  6864 | `					ph7_class_method *pMethod;` |
|        - |  6865 | `					/* Extract the target method */` |
|       10 |  6866 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6867 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6868 | `						/* Perform the lookup */` |
|       10 |  6869 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6870 | `						if( pMethod ){` |
|        - |  6871 | `							/* Method is callable */` |
|        5 |  6872 | `							res = 1;` |
|        2 |  6873 | `						}` |
|        4 |  6874 | `					}` |
|        4 |  6875 | `				}` |
|        5 |  6876 | `			}` |
|        7 |  6877 | `		}` |
|    16221 |  6878 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6879 | `		const char *zName;` |
|        - |  6880 | `		int nLen;` |
|        - |  6881 | `		/* Extract the name */` |
|     4750 |  6882 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6883 | `		/* Perform the lookup */` |
|     4765 |  6884 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6885 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6886 | `				/* Function is callable */` |
|     4732 |  6887 | `				res = 1;` |
|     2365 |  6888 | `		}` |
|     2374 |  6889 | `	}` |
|    16234 |  6890 | `	return res;` |
|        2 |  6891 |  |
|        - |  6892 | `/*` |
|        - |  6893 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6894 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6895 | ` * Parameters` |
|        - |  6896 | ` * $name` |
|        - |  6897 | ` *    The callback function to check` |
|        - |  6898 | ` * $syntax_only` |
|        - |  6899 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6900 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6901 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6902 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6903 | ` *    a string.` |
|        - |  6904 | ` * Return` |
|        - |  6905 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6906 | ` */` |
|       14 |  6907 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6908 |  |
|        - |  6909 | `	ph7_vm *pVm;` |
|        - |  6910 | `	int res;` |
|       15 |  6911 | `	if( nArg < 1 ){` |
|        - |  6912 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6913 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6914 | `		return SXRET_OK;` |
|        - |  6915 | `	}` |
|        - |  6916 | `	/* Point to the target VM */` |
|       15 |  6917 | `	pVm = pCtx->pVm;` |
|        - |  6918 | `	/* Perform the requested operation */` |
|       15 |  6919 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6920 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6921 | `	return SXRET_OK;` |
|        8 |  6922 |  |
|        - |  6923 | `/*` |
|        - |  6924 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6925 | ` * defined below.` |
|        - |  6926 | ` */` |
|     1082 |  6927 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6928 |  |
|     1083 |  6929 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6930 | `	ph7_value sName;` |
|        - |  6931 | `	sxi32 rc;` |
|        - |  6932 | `	/* Prepare the function name for insertion */` |
|     1083 |  6933 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6934 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6935 | `	/* Perform the insertion */` |
|     1083 |  6936 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6937 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6938 | `	return rc;` |
|        1 |  6939 |  |
|        - |  6940 | `/*` |
|        - |  6941 | ` * array get_defined_functions(void)` |
|        - |  6942 | ` *  Returns an array of all defined functions.` |
|        - |  6943 | ` * Parameter` |
|        - |  6944 | ` *  None.` |
|        - |  6945 | ` * Return` |
|        - |  6946 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6947 | ` *  both built-in (internal) and user-defined.` |
|        - |  6948 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6949 | ` *  defined ones using $arr["user"].` |
|        - |  6950 | ` * Note:` |
|        - |  6951 | ` *  NULL is returned on failure.` |
|        - |  6952 | ` */` |
|        2 |  6953 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6954 |  |
|        - |  6955 | `	ph7_value *pArray,*pEntry;` |
|        - |  6956 | `	/* NOTE:` |
|        - |  6957 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6958 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6959 | `	 */` |
|        3 |  6960 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6961 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6962 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6963 | `		SXUNUSED(apArg);` |
|        - |  6964 | `		/* Return NULL */` |
|      ! 0 |  6965 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6966 | `		return SXRET_OK;` |
|        - |  6967 | `	}` |
|        3 |  6968 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6969 | `	if( pEntry == 0 ){` |
|        - |  6970 | `		/* Return NULL */` |
|      ! 0 |  6971 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6972 | `		return SXRET_OK;` |
|        - |  6973 | `	}` |
|        - |  6974 | `	/* Fill with the appropriate information */` |
|        3 |  6975 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6976 | `	/* Create the 'internal' index */` |
|        3 |  6977 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6978 | `	/* Create the user-func array */` |
|        3 |  6979 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6980 | `	if( pEntry == 0 ){` |
|        - |  6981 | `		/* Return NULL */` |
|      ! 0 |  6982 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6983 | `		return SXRET_OK;` |
|        - |  6984 | `	}` |
|        - |  6985 | `	/* Fill with the appropriate information */` |
|        3 |  6986 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6987 | `	/* Create the 'user' index */` |
|        3 |  6988 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6989 | `	/* Return the multi-dimensional array */` |
|        3 |  6990 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6991 | `	return SXRET_OK;` |
|        2 |  6992 |  |
|        - |  6993 | `/*` |
|        - |  6994 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6995 | ` *  Register a function for execution on shutdown.` |
|        - |  6996 | ` * Note` |
|        - |  6997 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6998 | ` *  be called in the same order as they were registered.` |
|        - |  6999 | ` * Parameters` |
|        - |  7000 | ` *  $callback` |
|        - |  7001 | ` *   The shutdown callback to register.` |
|        - |  7002 | ` * $param` |
|        - |  7003 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  7004 | ` * Return` |
|        - |  7005 | ` *  Nothing.` |
|        - |  7006 | ` */` |
|        2 |  7007 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7008 |  |
|        - |  7009 | `	VmShutdownCB sEntry;` |
|        - |  7010 | `	int i,j;` |
|        3 |  7011 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7012 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  7013 | `		return PH7_OK;` |
|        - |  7014 | `	}` |
|        - |  7015 | `	/* Zero the Entry */` |
|        3 |  7016 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  7017 | `	/* Initialize fields */` |
|        3 |  7018 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  7019 | `	/* Save the callback name for later invocation name */` |
|        3 |  7020 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  7021 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  7022 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  7023 | `	}` |
|        - |  7024 | `	/* Copy arguments */` |
|        3 |  7025 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  7026 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  7027 | `			/* Limit reached */` |
|      ! 0 |  7028 | `			break;` |
|        - |  7029 | `		}` |
|      ! 0 |  7030 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  7031 | `	}` |
|        3 |  7032 | `	sEntry.nArg = j;` |
|        - |  7033 | `	/* Install the callback */` |
|        3 |  7034 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  7035 | `	return PH7_OK;` |
|        2 |  7036 |  |
|        - |  7037 | `/*` |
|        - |  7038 | ` * Section:` |
|        - |  7039 | ` *  Class handling functions.` |
|        - |  7040 | ` * Status:` |
|        - |  7041 | ` *    Stable.` |
|        - |  7042 | ` */` |
|        - |  7043 | `/*` |
|        - |  7044 | ` * Extract the top active class. NULL is returned` |
|        - |  7045 | ` * if the class stack is empty.` |
|        - |  7046 | ` */` |
|      550 |  7047 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  7048 |  |
|      552 |  7049 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  7050 | `	ph7_class **apClass;` |
|      552 |  7051 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  7052 | `		/* Empty stack,return NULL */` |
|       15 |  7053 | `		return 0;` |
|        - |  7054 | `	}` |
|        - |  7055 | `	/* Peek the last entry */` |
|      538 |  7056 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      538 |  7057 | `	return apClass[pSet->nUsed - 1];` |
|      277 |  7058 |  |
|        - |  7059 | `/*` |
|        - |  7060 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  7061 | ` *   Get the class that declared the currently executing method.` |
|        - |  7062 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  7063 | ` *` |
|        - |  7064 | ` * Parameters` |
|        - |  7065 | ` *   pVm: Target VM` |
|        - |  7066 | ` *` |
|        - |  7067 | ` * Return` |
|        - |  7068 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  7069 | ` *   - Not executing within a class method` |
|        - |  7070 | ` *` |
|        - |  7071 | ` * Note` |
|        - |  7072 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  7073 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  7074 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  7075 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  7076 | ` *   declaring class.` |
|        - |  7077 | ` */` |
|       52 |  7078 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  7079 |  |
|       54 |  7080 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  7081 | `	ph7_vm_func *pVmFunc;` |
|        - |  7082 |  |
|        - |  7083 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  7084 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  7085 |  |
|        - |  7086 | `	/* Check if we're in a method context */` |
|       54 |  7087 | `	if( pFrame->pParent ){` |
|       50 |  7088 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  7089 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  7090 | `			/* Return the declaring class */` |
|       50 |  7091 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  7092 | `		}` |
|      ! 0 |  7093 | `	}` |
|        - |  7094 |  |
|        5 |  7095 | `	return 0;` |
|       28 |  7096 |  |
|        - |  7097 |  |
|        - |  7098 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  7099 | `/*` |
|        - |  7100 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7101 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7102 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7103 | ` * return value indicates failure.` |
|        - |  7104 | ` */` |
|     1298 |  7105 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7106 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7107 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7108 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7109 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7110 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7111 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7112 | `	)` |
|        2 |  7113 |  |
|        - |  7114 | `	ph7_value *aStack;` |
|        - |  7115 | `	VmInstr aInstr[2];` |
|        - |  7116 | `	int iCursor;` |
|        - |  7117 | `	int i;` |
|        - |  7118 | `	/* Create a new operand stack */` |
|     1300 |  7119 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1300 |  7120 | `	if( aStack == 0 ){` |
|      ! 0 |  7121 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7122 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7123 | `		return SXERR_MEM;` |
|        - |  7124 | `	}` |
|        - |  7125 | `	/* Fill the operand stack with the given arguments */` |
|     1872 |  7126 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      574 |  7127 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7128 | `		/*` |
|        - |  7129 | `		 * Symisc eXtension:` |
|        - |  7130 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7131 | `		 */` |
|      574 |  7132 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      288 |  7133 | `	}` |
|     1300 |  7134 | `	iCursor = nArg + 1;` |
|     1300 |  7135 | `	if( pThis ){` |
|        - |  7136 | `		/*` |
|        - |  7137 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7138 | `		 */` |
|     1294 |  7139 | `		pThis->iRef++; /* Increment reference count */` |
|     1294 |  7140 | `		aStack[i].x.pOther = pThis;` |
|     1294 |  7141 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      646 |  7142 | `	}` |
|     1300 |  7143 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1300 |  7144 | `	i++;` |
|        - |  7145 | `	/* Push method name */` |
|     1300 |  7146 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1300 |  7147 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1300 |  7148 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1300 |  7149 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7150 | `	/* Emit the CALL istruction */` |
|     1300 |  7151 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1300 |  7152 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1300 |  7153 | `	aInstr[0].iP2 = 0;` |
|     1300 |  7154 | `	aInstr[0].p3  = 0;` |
|        - |  7155 | `	/* Emit the DONE instruction */` |
|     1300 |  7156 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1300 |  7157 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1300 |  7158 | `	aInstr[1].iP2 = 0;` |
|     1300 |  7159 | `	aInstr[1].p3  = 0;` |
|        - |  7160 | `	/* Execute the method body (if available) */` |
|     1300 |  7161 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7162 | `	/* Clean up the mess left behind */` |
|     1300 |  7163 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1300 |  7164 | `	return PH7_OK;` |
|      651 |  7165 |  |
|        - |  7166 | `/*` |
|        - |  7167 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7168 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7169 | ` * in the apArg[] array.` |
|        - |  7170 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7171 | ` * return value indicates failure.` |
|        - |  7172 | ` */` |
|      926 |  7173 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7174 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7175 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7176 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7177 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7178 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7179 | `	)` |
|        2 |  7180 |  |
|        - |  7181 | `	ph7_value *aStack;` |
|        - |  7182 | `	VmInstr aInstr[2];` |
|        - |  7183 | `	int i;` |
|      928 |  7184 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7185 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7186 | `		if( pResult ){` |
|        - |  7187 | `			/* Assume a null return value */` |
|      ! 0 |  7188 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7189 | `		}` |
|      471 |  7190 | `		return SXERR_INVALID;` |
|        - |  7191 | `	}` |
|      458 |  7192 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7193 | `		/* Class method */` |
|       11 |  7194 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7195 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7196 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7197 | `		ph7_class *pClass = 0;` |
|        - |  7198 | `		ph7_value *pValue;` |
|        - |  7199 | `		sxi32 rc;` |
|       11 |  7200 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7201 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7202 | `			if( pResult ){` |
|        - |  7203 | `				/* Assume a null return value */` |
|      ! 0 |  7204 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7205 | `			}` |
|      ! 0 |  7206 | `			return SXRET_OK;` |
|        - |  7207 | `		}` |
|        - |  7208 | `		/* Extract the class name or an instance of it */` |
|       11 |  7209 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7210 | `		if( pValue ){` |
|       11 |  7211 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7212 | `		}` |
|       11 |  7213 | `		if( pClass == 0 ){` |
|        - |  7214 | `			/* No such class,return NULL */` |
|      ! 0 |  7215 | `			if( pResult ){` |
|      ! 0 |  7216 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7217 | `			}` |
|      ! 0 |  7218 | `			return SXRET_OK;` |
|        - |  7219 | `		}` |
|       11 |  7220 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7221 | `			/* Point to the class instance */` |
|        5 |  7222 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7223 | `		}` |
|        - |  7224 | `		/* Try to extract the method */` |
|       11 |  7225 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7226 | `		if( pValue ){` |
|       11 |  7227 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7228 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7229 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7230 | `			}` |
|        5 |  7231 | `		}` |
|       11 |  7232 | `		if( pMethod == 0 ){` |
|        - |  7233 | `			/* No such method,return NULL */` |
|      ! 0 |  7234 | `			if( pResult ){` |
|      ! 0 |  7235 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7236 | `			}` |
|      ! 0 |  7237 | `			return SXRET_OK;` |
|        - |  7238 | `		}` |
|        - |  7239 | `		/* Call the class method */` |
|       11 |  7240 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7241 | `		return rc;` |
|        - |  7242 | `	}` |
|        - |  7243 | `	/* Create a new operand stack */` |
|      448 |  7244 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7245 | `	if( aStack == 0 ){` |
|      ! 0 |  7246 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7247 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7248 | `		if( pResult ){` |
|        - |  7249 | `			/* Assume a null return value */` |
|      ! 0 |  7250 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7251 | `		}` |
|      ! 0 |  7252 | `		return SXERR_MEM;` |
|        - |  7253 | `	}` |
|        - |  7254 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7255 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7256 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7257 | `		/*` |
|        - |  7258 | `		 * Symisc eXtension:` |
|        - |  7259 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7260 | `		 */` |
|     1024 |  7261 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7262 | `	}` |
|        - |  7263 | `	/* Push the function name */` |
|      448 |  7264 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7265 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7266 | `	/* Emit the CALL istruction */` |
|      448 |  7267 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7268 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7269 | `	aInstr[0].iP2 = 0;` |
|      448 |  7270 | `	aInstr[0].p3  = 0;` |
|        - |  7271 | `	/* Emit the DONE instruction */` |
|      448 |  7272 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7273 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7274 | `	aInstr[1].iP2 = 0;` |
|      448 |  7275 | `	aInstr[1].p3  = 0;` |
|        - |  7276 | `	/* Execute the function body (if available) */` |
|      448 |  7277 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7278 | `	/* Clean up the mess left behind */` |
|      448 |  7279 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7280 | `	return PH7_OK;` |
|      465 |  7281 |  |
|        - |  7282 | `/*` |
|        - |  7283 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7284 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7285 | ` * parameter.` |
|        - |  7286 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7287 | ` * return value indicates failure.` |
|        - |  7288 | ` */` |
|      236 |  7289 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7290 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7291 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7292 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7293 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7294 | `	)` |
|        1 |  7295 |  |
|        - |  7296 | `	ph7_value *pArg;` |
|        - |  7297 | `	SySet aArg;` |
|        - |  7298 | `	va_list ap;` |
|        - |  7299 | `	sxi32 rc;` |
|      237 |  7300 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7301 | `	/* Copy arguments one after one */` |
|      237 |  7302 | `	va_start(ap,pResult);` |
|      393 |  7303 | `	for(;;){` |
|      787 |  7304 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7305 | `		if( pArg == 0 ){` |
|      237 |  7306 | `			break;` |
|        - |  7307 | `		}` |
|      551 |  7308 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7309 | `	}` |
|        - |  7310 | `	/* Call the core routine */` |
|      237 |  7311 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7312 | `	/* Cleanup */` |
|      237 |  7313 | `	SySetRelease(&aArg);` |
|      237 |  7314 | `	return rc;` |
|        1 |  7315 |  |
|        - |  7316 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7317 | `/*` |
|        - |  7318 | ` * bool defined(string $name)` |
|        - |  7319 | ` *  Checks whether a given named constant exists.` |
|        - |  7320 | ` * Parameter:` |
|        - |  7321 | ` *  Name of the desired constant.` |
|        - |  7322 | ` * Return` |
|        - |  7323 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7324 | ` */` |
|       14 |  7325 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7326 |  |
|        - |  7327 | `	const char *zName;` |
|       16 |  7328 | `	int nLen = 0;` |
|       16 |  7329 | `	int res = 0;` |
|       16 |  7330 | `	if( nArg < 1 ){` |
|        - |  7331 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7332 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7333 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7334 | `		return SXRET_OK;` |
|        - |  7335 | `	}` |
|        - |  7336 | `	/* Extract constant name */` |
|       16 |  7337 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7338 | `	/* Perform the lookup */` |
|       16 |  7339 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7340 | `		/* Already defined */` |
|       10 |  7341 | `		res = 1;` |
|        4 |  7342 | `	}` |
|       16 |  7343 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7344 | `	return SXRET_OK;` |
|        9 |  7345 |  |
|        - |  7346 | `/*` |
|        - |  7347 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7348 | ` * below.` |
|        - |  7349 | ` */` |
|        8 |  7350 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7351 |  |
|       10 |  7352 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7353 | `	/* Expand constant value */` |
|       10 |  7354 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7355 |  |
|        - |  7356 | `/*` |
|        - |  7357 | ` * bool define(string $constant_name,expression value)` |
|        - |  7358 | ` *  Defines a named constant at runtime.` |
|        - |  7359 | ` * Parameter:` |
|        - |  7360 | ` *  $constant_name` |
|        - |  7361 | ` *   The name of the constant` |
|        - |  7362 | ` *  $value` |
|        - |  7363 | ` *   Constant value` |
|        - |  7364 | ` * Return:` |
|        - |  7365 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7366 | ` */` |
|       10 |  7367 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7368 |  |
|        - |  7369 | `	const char *zName;  /* Constant name */` |
|        - |  7370 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7371 | `	int nLen = 0;       /* Name length */` |
|        - |  7372 | `	sxi32 rc;` |
|       12 |  7373 | `	if( nArg < 2 ){` |
|        - |  7374 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7375 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7376 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7377 | `		return SXRET_OK;` |
|        - |  7378 | `	}` |
|       12 |  7379 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7380 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7381 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7382 | `		return SXRET_OK;` |
|        - |  7383 | `	}` |
|        - |  7384 | `	/* Extract constant name */` |
|       12 |  7385 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7386 | `	if( nLen < 1 ){` |
|      ! 0 |  7387 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7388 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7389 | `		return SXRET_OK;` |
|        - |  7390 | `	}` |
|        - |  7391 | `	/* Duplicate constant value */` |
|       12 |  7392 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7393 | `	if( pValue == 0 ){` |
|      ! 0 |  7394 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7395 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7396 | `		return SXRET_OK;` |
|        - |  7397 | `	}` |
|        - |  7398 | `	/* Initialize the memory object */` |
|       12 |  7399 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7400 | `	/* Register the constant */` |
|       12 |  7401 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7402 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7403 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7404 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7405 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7406 | `		return SXRET_OK;` |
|        - |  7407 | `	}` |
|        - |  7408 | `	/* Duplicate constant value */` |
|       12 |  7409 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7410 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7411 | `		/* Lower case the constant name */` |
|      ! 0 |  7412 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7413 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7414 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7415 | `				/* UTF-8 stream */` |
|      ! 0 |  7416 | `				zCur++;` |
|      ! 0 |  7417 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7418 | `					zCur++;` |
|      ! 0 |  7419 | `				}` |
|      ! 0 |  7420 | `				continue;` |
|        - |  7421 | `			}` |
|      ! 0 |  7422 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7423 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7424 | `				zCur[0] = (char)c;` |
|      ! 0 |  7425 | `			}` |
|      ! 0 |  7426 | `			zCur++;` |
|      ! 0 |  7427 | `		}` |
|        - |  7428 | `		/* Finally,register the constant */` |
|      ! 0 |  7429 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7430 | `	}` |
|        - |  7431 | `	/* All done,return TRUE */` |
|       12 |  7432 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7433 | `	return SXRET_OK;` |
|        7 |  7434 |  |
|        - |  7435 | `/*` |
|        - |  7436 | ` * value constant(string $name)` |
|        - |  7437 | ` *  Returns the value of a constant` |
|        - |  7438 | ` * Parameter` |
|        - |  7439 | ` *  $name` |
|        - |  7440 | ` *    Name of the constant.` |
|        - |  7441 | ` * Return` |
|        - |  7442 | ` *  Constant value or NULL if not defined.` |
|        - |  7443 | ` */` |
|        8 |  7444 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7445 |  |
|        - |  7446 | `	SyHashEntry *pEntry;` |
|        - |  7447 | `	ph7_constant *pCons;` |
|        - |  7448 | `	const char *zName; /* Constant name */` |
|        - |  7449 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7450 | `	int nLen;` |
|       10 |  7451 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7452 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7453 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7454 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7455 | `		return SXRET_OK;` |
|        - |  7456 | `	}` |
|        - |  7457 | `	/* Extract the constant name */` |
|       10 |  7458 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7459 | `	/* Perform the query */` |
|       10 |  7460 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7461 | `	if( pEntry == 0 ){` |
|        3 |  7462 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7463 | `		ph7_result_null(pCtx);` |
|        3 |  7464 | `		return SXRET_OK;` |
|        - |  7465 | `	}` |
|        8 |  7466 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7467 | `	/* Point to the structure that describe the constant */` |
|        8 |  7468 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7469 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7470 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7471 | `	/* Return that value */` |
|        8 |  7472 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7473 | `	/* Cleanup */` |
|        8 |  7474 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7475 | `	return SXRET_OK;` |
|        6 |  7476 |  |
|        - |  7477 | `/*` |
|        - |  7478 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7479 | ` * defined below.` |
|        - |  7480 | ` */` |
|      416 |  7481 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7482 |  |
|      417 |  7483 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7484 | `	ph7_value sName;` |
|        - |  7485 | `	sxi32 rc;` |
|        - |  7486 | `	/* Prepare the constant name for insertion */` |
|      417 |  7487 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7488 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7489 | `	/* Perform the insertion */` |
|      417 |  7490 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7491 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7492 | `	return rc;` |
|        1 |  7493 |  |
|        - |  7494 | `/*` |
|        - |  7495 | ` * array get_defined_constants(void)` |
|        - |  7496 | ` *  Returns an associative array with the names of all defined` |
|        - |  7497 | ` *  constants.` |
|        - |  7498 | ` * Parameters` |
|        - |  7499 | ` *  NONE.` |
|        - |  7500 | ` * Returns` |
|        - |  7501 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7502 | ` */` |
|        2 |  7503 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7504 |  |
|        - |  7505 | `	ph7_value *pArray;` |
|        - |  7506 | `	/* Create the array first*/` |
|        3 |  7507 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7508 | `	if( pArray == 0 ){` |
|      ! 0 |  7509 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7510 | `		SXUNUSED(apArg);` |
|        - |  7511 | `		/* Return NULL */` |
|      ! 0 |  7512 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7513 | `		return SXRET_OK;` |
|        - |  7514 | `	}` |
|        - |  7515 | `	/* Fill the array with the defined constants */` |
|        3 |  7516 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7517 | `	/* Return the created array */` |
|        3 |  7518 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7519 | `	return SXRET_OK;` |
|        2 |  7520 |  |
|        - |  7521 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7522 | `/*` |
|        - |  7523 | ` * Section:` |
|        - |  7524 | ` *  Random numbers/string generators.` |
|        - |  7525 | ` * Status:` |
|        - |  7526 | ` *    Stable.` |
|        - |  7527 | ` */` |
|        - |  7528 | `/*` |
|        - |  7529 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7530 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7531 | ` * used by te SQLite3 library.` |
|        - |  7532 | ` */` |
|     2363 |  7533 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7534 |  |
|        - |  7535 | `	sxu32 iNum;` |
|     2365 |  7536 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2365 |  7537 | `	return iNum;` |
|        2 |  7538 |  |
|        - |  7539 | `/*` |
|        - |  7540 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7541 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7542 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7543 | ` * by te SQLite3 library.` |
|        - |  7544 | ` */` |
|    74522 |  7545 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7546 |  |
|        - |  7547 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7548 | `	int i;` |
|        - |  7549 | `	/* Generate a binary string first */` |
|    74524 |  7550 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7551 | `	/* Turn the binary string into english based alphabet */` |
|   819912 |  7552 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   745390 |  7553 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   372696 |  7554 | `	 }` |
|    74524 |  7555 |  |
|        - |  7556 | `/*` |
|        - |  7557 | ` * int rand()` |
|        - |  7558 | ` * int mt_rand()` |
|        - |  7559 | ` * int rand(int $min,int $max)` |
|        - |  7560 | ` * int mt_rand(int $min,int $max)` |
|        - |  7561 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7562 | ` * Parameter` |
|        - |  7563 | ` *  $min` |
|        - |  7564 | ` *    The lowest value to return (default: 0)` |
|        - |  7565 | ` *  $max` |
|        - |  7566 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7567 | ` * Return` |
|        - |  7568 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7569 | ` * Note:` |
|        - |  7570 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7571 | ` *  by te SQLite3 library.` |
|        - |  7572 | ` */` |
|       20 |  7573 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7574 |  |
|        - |  7575 | `	sxu32 iNum;` |
|        - |  7576 | `	/* Generate the random number */` |
|       21 |  7577 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7578 | `	if( nArg > 1 ){` |
|        - |  7579 | `		sxu32 iMin,iMax;` |
|        3 |  7580 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7581 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7582 | `		if( iMin < iMax ){` |
|        3 |  7583 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7584 | `			if( iDiv > 0 ){` |
|        3 |  7585 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7586 | `			}` |
|        1 |  7587 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7588 | `			iNum %= iMax;` |
|      ! 0 |  7589 | `		}` |
|        1 |  7590 | `	}` |
|        - |  7591 | `	/* Return the number */` |
|       21 |  7592 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7593 | `	return SXRET_OK;` |
|        1 |  7594 |  |
|        - |  7595 | `/*` |
|        - |  7596 | ` * int getrandmax(void)` |
|        - |  7597 | ` * int mt_getrandmax(void)` |
|        - |  7598 | ` * int rc4_getrandmax(void)` |
|        - |  7599 | ` *   Show largest possible random value` |
|        - |  7600 | ` * Return` |
|        - |  7601 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7602 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7603 | ` * Note:` |
|        - |  7604 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7605 | ` *  by te SQLite3 library.` |
|        - |  7606 | ` */` |
|        4 |  7607 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7608 |  |
|        2 |  7609 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7610 | `	SXUNUSED(apArg);` |
|        5 |  7611 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7612 | `	return SXRET_OK;` |
|        1 |  7613 |  |
|        - |  7614 | `/*` |
|        - |  7615 | ` * string rand_str()` |
|        - |  7616 | ` * string rand_str(int $len)` |
|        - |  7617 | ` *  Generate a random string (English alphabet).` |
|        - |  7618 | ` * Parameter` |
|        - |  7619 | ` *  $len` |
|        - |  7620 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7621 | ` * Return` |
|        - |  7622 | ` *   A pseudo random string.` |
|        - |  7623 | ` * Note:` |
|        - |  7624 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7625 | ` *  by te SQLite3 library.` |
|        - |  7626 | ` *  This function is a symisc extension.` |
|        - |  7627 | ` */` |
|      120 |  7628 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7629 |  |
|        - |  7630 | `	char zString[1024];` |
|      122 |  7631 | `	int iLen = 0x10;` |
|      122 |  7632 | `	if( nArg > 0 ){` |
|        - |  7633 | `		/* Get the desired length */` |
|      122 |  7634 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7635 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7636 | `			/* Default length */` |
|        3 |  7637 | `			iLen = 0x10;` |
|        1 |  7638 | `		}` |
|       60 |  7639 | `	}` |
|        - |  7640 | `	/* Generate the random string */` |
|      122 |  7641 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7642 | `	/* Return the generated string */` |
|      122 |  7643 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7644 | `	return SXRET_OK;` |
|        2 |  7645 |  |
|        - |  7646 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7647 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7648 | `/* Unique ID private data */` |
|        - |  7649 | `struct unique_id_data` |
|        - |  7650 |  |
|        - |  7651 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7652 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7653 | `};` |
|        - |  7654 | `/*` |
|        - |  7655 | ` * Binary to hex consumer callback.` |
|        - |  7656 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7657 | ` * defined below.` |
|        - |  7658 | ` */` |
|      192 |  7659 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7660 |  |
|      193 |  7661 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7662 | `	sxu32 nBuflen;` |
|        - |  7663 | `	/* Extract result buffer length */` |
|      193 |  7664 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7665 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7666 | `			/*` |
|        - |  7667 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7668 | `			 * string will be 13 characters long` |
|        - |  7669 | `			 */` |
|       25 |  7670 | `		return SXERR_ABORT;` |
|        - |  7671 | `	}` |
|      169 |  7672 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7673 | `		return SXERR_ABORT;` |
|        - |  7674 | `	}` |
|        - |  7675 | `	/* Safely Consume the hex stream */` |
|      169 |  7676 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7677 | `	return SXRET_OK;` |
|       97 |  7678 |  |
|        - |  7679 | `/*` |
|        - |  7680 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7681 | ` *  Generate a unique ID` |
|        - |  7682 | ` * Parameter` |
|        - |  7683 | ` * $prefix` |
|        - |  7684 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7685 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7686 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7687 | ` * $more_entropy` |
|        - |  7688 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7689 | ` *  that the result will be unique.` |
|        - |  7690 | ` * Return` |
|        - |  7691 | ` *  Returns the unique identifier, as a string.` |
|        - |  7692 | ` */` |
|       24 |  7693 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7694 |  |
|        - |  7695 | `	struct unique_id_data sUniq;` |
|        - |  7696 | `	unsigned char zDigest[20];` |
|       25 |  7697 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7698 | `	const char *zPrefix;` |
|        - |  7699 | `	SHA1Context sCtx;` |
|        - |  7700 | `	char zRandom[7];` |
|        - |  7701 | `	int nPrefix;` |
|        - |  7702 | `	int entropy;` |
|        - |  7703 | `	/* Generate a random string first */` |
|       25 |  7704 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7705 | `	/* Initialize fields */` |
|       25 |  7706 | `	zPrefix = 0;` |
|       25 |  7707 | `	nPrefix = 0;` |
|       25 |  7708 | `	entropy = 0;` |
|       25 |  7709 | `	if( nArg > 0 ){` |
|        - |  7710 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7711 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7712 | `		if( nArg > 1 ){` |
|      ! 0 |  7713 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7714 | `		}` |
|      ! 0 |  7715 | `	}` |
|       25 |  7716 | `	SHA1Init(&sCtx);` |
|        - |  7717 | `	/* Generate the random ID */` |
|       25 |  7718 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7719 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7720 | `	}` |
|        - |  7721 | `	/* Append the random ID */` |
|       25 |  7722 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7723 | `	/* Append the random string */` |
|       25 |  7724 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7725 | `	/* Increment the number */` |
|       25 |  7726 | `	pVm->unique_id++;` |
|       25 |  7727 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7728 | `	/* Hexify the digest */` |
|       25 |  7729 | `	sUniq.pCtx = pCtx;` |
|       25 |  7730 | `	sUniq.entropy = entropy;` |
|       25 |  7731 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7732 | `	/* All done */` |
|       25 |  7733 | `	return PH7_OK;` |
|        1 |  7734 |  |
|        - |  7735 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7736 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7737 | `/*` |
|        - |  7738 | ` * Section:` |
|        - |  7739 | ` *  Language construct implementation as foreign functions.` |
|        - |  7740 | ` * Status:` |
|        - |  7741 | ` *    Stable.` |
|        - |  7742 | ` */` |
|        - |  7743 | `/*` |
|        - |  7744 | ` * void echo($string...)` |
|        - |  7745 | ` *  Output one or more messages.` |
|        - |  7746 | ` * Parameters` |
|        - |  7747 | ` *  $string` |
|        - |  7748 | ` *   Message to output.` |
|        - |  7749 | ` * Return` |
|        - |  7750 | ` *  NULL.` |
|        - |  7751 | ` */` |
|      ! 0 |  7752 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7753 |  |
|        - |  7754 | `	const char *zData;` |
|      ! 0 |  7755 | `	int nDataLen = 0;` |
|        - |  7756 | `	ph7_vm *pVm;` |
|        - |  7757 | `	int i,rc;` |
|        - |  7758 | `	/* Point to the target VM */` |
|      ! 0 |  7759 | `	pVm = pCtx->pVm;` |
|        - |  7760 | `	/* Output */` |
|      ! 0 |  7761 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7762 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7763 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7764 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7765 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7766 | `				/* Increment output length */` |
|      ! 0 |  7767 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7768 | `			}` |
|      ! 0 |  7769 | `			if( rc == SXERR_ABORT ){` |
|        - |  7770 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7771 | `				return PH7_ABORT;` |
|        - |  7772 | `			}` |
|      ! 0 |  7773 | `		}` |
|      ! 0 |  7774 | `	}` |
|      ! 0 |  7775 | `	return SXRET_OK;` |
|      ! 0 |  7776 |  |
|        - |  7777 | `/*` |
|        - |  7778 | ` * int print($string...)` |
|        - |  7779 | ` *  Output one or more messages.` |
|        - |  7780 | ` * Parameters` |
|        - |  7781 | ` *  $string` |
|        - |  7782 | ` *   Message to output.` |
|        - |  7783 | ` * Return` |
|        - |  7784 | ` *  1 always.` |
|        - |  7785 | ` */` |
|        2 |  7786 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7787 |  |
|        - |  7788 | `	const char *zData;` |
|        3 |  7789 | `	int nDataLen = 0;` |
|        - |  7790 | `	ph7_vm *pVm;` |
|        - |  7791 | `	int i,rc;` |
|        - |  7792 | `	/* Point to the target VM */` |
|        3 |  7793 | `	pVm = pCtx->pVm;` |
|        - |  7794 | `	/* Output */` |
|        5 |  7795 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7796 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7797 | `		if( nDataLen > 0 ){` |
|        3 |  7798 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7799 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7800 | `				/* Increment output length */` |
|        3 |  7801 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7802 | `			}` |
|        3 |  7803 | `			if( rc == SXERR_ABORT ){` |
|        - |  7804 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7805 | `				return PH7_ABORT;` |
|        - |  7806 | `			}` |
|        1 |  7807 | `		}` |
|        2 |  7808 | `	}` |
|        - |  7809 | `	/* Return 1 */` |
|        3 |  7810 | `	ph7_result_int(pCtx,1);` |
|        3 |  7811 | `	return SXRET_OK;` |
|        2 |  7812 |  |
|        - |  7813 | `/*` |
|        - |  7814 | ` * void exit(string $msg)` |
|        - |  7815 | ` * void exit(int $status)` |
|        - |  7816 | ` * void die(string $ms)` |
|        - |  7817 | ` * void die(int $status)` |
|        - |  7818 | ` *   Output a message and terminate program execution.` |
|        - |  7819 | ` * Parameter` |
|        - |  7820 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7821 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7822 | ` *  and not printed` |
|        - |  7823 | ` * Return` |
|        - |  7824 | ` *  NULL` |
|        - |  7825 | ` */` |
|      ! 0 |  7826 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7827 |  |
|      ! 0 |  7828 | `	if( nArg > 0 ){` |
|      ! 0 |  7829 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7830 | `			const char *zData;` |
|      ! 0 |  7831 | `			int iLen = 0;` |
|        - |  7832 | `			/* Print exit message */` |
|      ! 0 |  7833 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7834 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7835 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7836 | `			sxi32 iExitStatus;` |
|        - |  7837 | `			/* Record exit status code */` |
|      ! 0 |  7838 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7839 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7840 | `		}` |
|      ! 0 |  7841 | `	}` |
|        - |  7842 | `	/* Check if we are in an included file */` |
|      ! 0 |  7843 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7844 | `		/* Exit the entire process */` |
|      ! 0 |  7845 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7846 | `	}` |
|        - |  7847 | `	/* Abort processing immediately */` |
|      ! 0 |  7848 | `	return PH7_ABORT;` |
|      ! 0 |  7849 |  |
|        - |  7850 | `/*` |
|        - |  7851 | ` * bool isset($var,...)` |
|        - |  7852 | ` *  Finds out whether a variable is set.` |
|        - |  7853 | ` * Parameters` |
|        - |  7854 | ` *  One or more variable to check.` |
|        - |  7855 | ` * Return` |
|        - |  7856 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7857 | ` */` |
|    72216 |  7858 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7859 |  |
|        - |  7860 | `	ph7_value *pObj;` |
|    72218 |  7861 | `	int res = 0;` |
|        - |  7862 | `	int i;` |
|    72218 |  7863 | `	if( nArg < 1 ){` |
|        - |  7864 | `		/* Missing arguments,return false */` |
|      ! 0 |  7865 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7866 | `		return SXRET_OK;` |
|        - |  7867 | `	}` |
|        - |  7868 | `	/* Iterate over available arguments */` |
|    95276 |  7869 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    72218 |  7870 | `		pObj = apArg[i];` |
|    72218 |  7871 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    48652 |  7872 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7873 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7874 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7875 | `			}` |
|    24325 |  7876 | `		}` |
|    72218 |  7877 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    72218 |  7878 | `		if( !res ){` |
|        - |  7879 | `			/* Variable not set,return FALSE */` |
|    49160 |  7880 | `			ph7_result_bool(pCtx,0);` |
|    49160 |  7881 | `			return SXRET_OK;` |
|        - |  7882 | `		}` |
|    11531 |  7883 | `	}` |
|        - |  7884 | `	/* All given variable are set,return TRUE */` |
|    23060 |  7885 | `	ph7_result_bool(pCtx,1);` |
|    23060 |  7886 | `	return SXRET_OK;` |
|    36110 |  7887 |  |
|        - |  7888 | `/*` |
|        - |  7889 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7890 | ` * frame,the reference table and discard it's contents.` |
|        - |  7891 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7892 | ` */` |
|  2968290 |  7893 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7894 |  |
|        - |  7895 | `	ph7_value *pObj;` |
|        - |  7896 | `	VmRefObj *pRef;` |
|  2968292 |  7897 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2968292 |  7898 | `	if( pObj ){` |
|        - |  7899 | `		/* Release the object */` |
|  2968292 |  7900 | `		PH7_MemObjRelease(pObj);` |
|  1484145 |  7901 | `	}` |
|        - |  7902 | `	/* Remove old reference links */` |
|  2968292 |  7903 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2968292 |  7904 | `	if( pRef ){` |
|  2968286 |  7905 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7906 | `		/* Unlink from the reference table */` |
|  2968286 |  7907 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2968286 |  7908 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7909 | `			VmSlot sFree;` |
|        - |  7910 | `			/* Restore to the free list */` |
|  2968280 |  7911 | `			sFree.nIdx = nObjIdx;` |
|  2968280 |  7912 | `			sFree.pUserData = 0;` |
|  2968280 |  7913 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1484139 |  7914 | `		}` |
|  1484142 |  7915 | `	}` |
|  2968292 |  7916 | `	return SXRET_OK;` |
|        2 |  7917 |  |
|        - |  7918 | `/*` |
|        - |  7919 | ` * void unset($var,...)` |
|        - |  7920 | ` *   Unset one or more given variable.` |
|        - |  7921 | ` * Parameters` |
|        - |  7922 | ` *  One or more variable to unset.` |
|        - |  7923 | ` * Return` |
|        - |  7924 | ` *  Nothing.` |
|        - |  7925 | ` */` |
|     6518 |  7926 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7927 |  |
|        - |  7928 | `	ph7_value *pObj;` |
|        - |  7929 | `	ph7_vm *pVm;` |
|        - |  7930 | `	int i;` |
|        - |  7931 | `	/* Point to the target VM */` |
|     6520 |  7932 | `	pVm = pCtx->pVm;` |
|        - |  7933 | `	/* Iterate and unset */` |
|    13038 |  7934 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6520 |  7935 | `		pObj = apArg[i];` |
|     6520 |  7936 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7937 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7938 | `				/* Throw an error */` |
|      ! 0 |  7939 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7940 | `			}` |
|      ! 0 |  7941 | `		}else{` |
|     6520 |  7942 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7943 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6520 |  7944 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6514 |  7945 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3256 |  7946 | `			}` |
|        - |  7947 | `		}` |
|     3261 |  7948 | `	}` |
|     6520 |  7949 | `	return SXRET_OK;` |
|        2 |  7950 |  |
|        - |  7951 | `/*` |
|        - |  7952 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7953 | ` */` |
|      110 |  7954 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7955 |  |
|      111 |  7956 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7957 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7958 | `	ph7_value *pObj;` |
|        - |  7959 | `	sxu32 nIdx;` |
|        - |  7960 | `	/* Extract the memory object */` |
|      111 |  7961 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7962 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7963 | `	if( pObj ){` |
|      111 |  7964 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7965 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7966 | `				SyString sName;` |
|        - |  7967 | `				ph7_value sKey;` |
|        - |  7968 | `				/* Perform the insertion */` |
|      109 |  7969 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7970 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7971 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7972 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7973 | `			}` |
|       54 |  7974 | `		}` |
|       55 |  7975 | `	}` |
|      111 |  7976 | `	return SXRET_OK;` |
|        1 |  7977 |  |
|        - |  7978 | `/*` |
|        - |  7979 | ` * array get_defined_vars(void)` |
|        - |  7980 | ` *  Returns an array of all defined variables.` |
|        - |  7981 | ` * Parameter` |
|        - |  7982 | ` *  None` |
|        - |  7983 | ` * Return` |
|        - |  7984 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7985 | ` */` |
|        2 |  7986 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7987 |  |
|        3 |  7988 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7989 | `	ph7_value *pArray;` |
|        - |  7990 | `	/* Create a new array */` |
|        3 |  7991 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7992 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7993 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7994 | `		SXUNUSED(apArg);` |
|        - |  7995 | `		/* Return NULL */` |
|      ! 0 |  7996 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7997 | `		return SXRET_OK;` |
|        - |  7998 | `	}` |
|        - |  7999 | `	/* Superglobals first */` |
|        3 |  8000 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8001 | `	/* Then variable defined in the current frame */` |
|        3 |  8002 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8003 | `	/* Finally,return the created array */` |
|        3 |  8004 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8005 | `	return SXRET_OK;` |
|        2 |  8006 |  |
|        - |  8007 | `/*` |
|        - |  8008 | ` * bool gettype($var)` |
|        - |  8009 | ` *  Get the type of a variable` |
|        - |  8010 | ` * Parameters` |
|        - |  8011 | ` *   $var` |
|        - |  8012 | ` *    The variable being type checked.` |
|        - |  8013 | ` * Return` |
|        - |  8014 | ` *   String representation of the given variable type.` |
|        - |  8015 | ` */` |
|       32 |  8016 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8017 |  |
|       34 |  8018 | `	const char *zType = "Empty";` |
|       34 |  8019 | `	if( nArg > 0 ){` |
|       34 |  8020 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  8021 | `	}` |
|        - |  8022 | `	/* Return the variable type */` |
|       34 |  8023 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  8024 | `	return SXRET_OK;` |
|        2 |  8025 |  |
|        - |  8026 | `/*` |
|        - |  8027 | ` * string get_resource_type(resource $handle)` |
|        - |  8028 | ` *  This function gets the type of the given resource.` |
|        - |  8029 | ` * Parameters` |
|        - |  8030 | ` *  $handle` |
|        - |  8031 | ` *  The evaluated resource handle.` |
|        - |  8032 | ` * Return` |
|        - |  8033 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  8034 | ` *  representing its type. If the type is not identified by this function` |
|        - |  8035 | ` *  the return value will be the string Unknown.` |
|        - |  8036 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  8037 | ` *  is not a resource.` |
|        - |  8038 | ` */` |
|        2 |  8039 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8040 |  |
|        3 |  8041 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  8042 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  8043 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8044 | `		return PH7_OK;` |
|        - |  8045 | `	}` |
|        3 |  8046 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  8047 | `	return SXRET_OK;` |
|        2 |  8048 |  |
|        - |  8049 | `/*` |
|        - |  8050 | ` * void var_dump(expression,....)` |
|        - |  8051 | ` *   var_dump � Dumps information about a variable` |
|        - |  8052 | ` * Parameters` |
|        - |  8053 | ` *   One or more expression to dump.` |
|        - |  8054 | ` * Returns` |
|        - |  8055 | ` *  Nothing.` |
|        - |  8056 | ` */` |
|      218 |  8057 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8058 |  |
|        - |  8059 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8060 | `	int i;` |
|      220 |  8061 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8062 | `	/* Dump one or more expressions */` |
|      444 |  8063 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  8064 | `		ph7_value *pObj = apArg[i];` |
|        - |  8065 | `		/* Reset the working buffer */` |
|      226 |  8066 | `		SyBlobReset(&sDump);` |
|        - |  8067 | `		/* Dump the given expression */` |
|      226 |  8068 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8069 | `		/* Output */` |
|      226 |  8070 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  8071 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  8072 | `		}` |
|      114 |  8073 | `	}` |
|        - |  8074 | `	/* Release the working buffer */` |
|      220 |  8075 | `	SyBlobRelease(&sDump);` |
|      220 |  8076 | `	return SXRET_OK;` |
|        2 |  8077 |  |
|        - |  8078 | `/*` |
|        - |  8079 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8080 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8081 | ` * Parameters` |
|        - |  8082 | ` *   expression: Expression to dump` |
|        - |  8083 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8084 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8085 | ` *            print_r() will return the information rather than print it.` |
|        - |  8086 | ` * Return` |
|        - |  8087 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8088 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8089 | ` */` |
|       16 |  8090 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8091 |  |
|       17 |  8092 | `	int ret_string = 0;` |
|        - |  8093 | `	SyBlob sDump;` |
|       17 |  8094 | `	if( nArg < 1 ){` |
|        - |  8095 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8096 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8097 | `		return SXRET_OK;` |
|        - |  8098 | `	}` |
|       17 |  8099 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8100 | `	if ( nArg > 1 ){` |
|        - |  8101 | `		/* Where to redirect output */` |
|       11 |  8102 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8103 | `	}` |
|        - |  8104 | `	/* Generate dump */` |
|       17 |  8105 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8106 | `	if( !ret_string ){` |
|        - |  8107 | `		/* Output dump */` |
|        7 |  8108 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8109 | `		/* Return true */` |
|        7 |  8110 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8111 | `	}else{` |
|        - |  8112 | `		/* Generated dump as return value */` |
|       11 |  8113 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8114 | `	}` |
|        - |  8115 | `	/* Release the working buffer */` |
|       17 |  8116 | `	SyBlobRelease(&sDump);` |
|       17 |  8117 | `	return SXRET_OK;` |
|        9 |  8118 |  |
|        - |  8119 | `/*` |
|        - |  8120 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8121 | ` * Same job as print_r. (see coment above)` |
|        - |  8122 | ` */` |
|        2 |  8123 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8124 |  |
|        3 |  8125 | `	int ret_string = 0;` |
|        - |  8126 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8127 | `	if( nArg < 1 ){` |
|        - |  8128 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8129 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8130 | `		return SXRET_OK;` |
|        - |  8131 | `	}` |
|        3 |  8132 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8133 | `	if ( nArg > 1 ){` |
|        - |  8134 | `		/* Where to redirect output */` |
|        3 |  8135 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8136 | `	}` |
|        - |  8137 | `	/* Generate dump */` |
|        3 |  8138 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8139 | `	if( !ret_string ){` |
|        - |  8140 | `		/* Output dump */` |
|      ! 0 |  8141 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8142 | `		/* Return NULL */` |
|      ! 0 |  8143 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8144 | `	}else{` |
|        - |  8145 | `		/* Generated dump as return value */` |
|        3 |  8146 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8147 | `	}` |
|        - |  8148 | `	/* Release the working buffer */` |
|        3 |  8149 | `	SyBlobRelease(&sDump);` |
|        3 |  8150 | `	return SXRET_OK;` |
|        2 |  8151 |  |
|        - |  8152 | `/*` |
|        - |  8153 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8154 | ` *  Set/get the various assert flags.` |
|        - |  8155 | ` * Parameter` |
|        - |  8156 | ` * $what` |
|        - |  8157 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8158 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  8159 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8160 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  8161 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8162 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  8163 | ` * $value` |
|        - |  8164 | ` *   An optional new value for the option.` |
|        - |  8165 | ` * Return` |
|        - |  8166 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8167 | ` */` |
|       30 |  8168 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8169 |  |
|       32 |  8170 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8171 | `	int iOption;` |
|        - |  8172 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  8173 | `	if( nArg < 1 ){` |
|        3 |  8174 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8175 | `			"ArgumentCountError",` |
|        - |  8176 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  8177 | `			);` |
|        - |  8178 | `	}` |
|        - |  8179 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8180 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8181 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8182 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8183 | `			"TypeError",` |
|        - |  8184 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8185 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8186 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8187 | `			);` |
|        - |  8188 | `	}` |
|       30 |  8189 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8190 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8191 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8192 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8193 | `	switch( iOption ){` |
|        6 |  8194 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8195 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8196 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8197 | `		if( nArg > 1 ){` |
|        5 |  8198 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8199 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8200 | `			}else{` |
|        3 |  8201 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8202 | `			}` |
|        2 |  8203 | `		}` |
|       14 |  8204 | `		break;` |
|        1 |  8205 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8206 | `		/* Return old callback or null */` |
|        3 |  8207 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8208 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8209 | `		}else{` |
|        3 |  8210 | `			ph7_result_null(pCtx);` |
|        - |  8211 | `		}` |
|        3 |  8212 | `		if( nArg > 1 ){` |
|      ! 0 |  8213 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8214 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8215 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8216 | `			}else{` |
|      ! 0 |  8217 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8218 | `			}` |
|      ! 0 |  8219 | `		}` |
|        3 |  8220 | `		break;` |
|        5 |  8221 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8222 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8223 | `		if( nArg > 1 ){` |
|        5 |  8224 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8225 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8226 | `			}else{` |
|        3 |  8227 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8228 | `			}` |
|        2 |  8229 | `		}` |
|       11 |  8230 | `		break;` |
|      ! 0 |  8231 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8232 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8233 | `		break;` |
|        1 |  8234 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8235 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8236 | `		break;` |
|      ! 0 |  8237 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8238 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8239 | `		break;` |
|        1 |  8240 | `	default:` |
|        - |  8241 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8242 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8243 | `			"ValueError",` |
|        - |  8244 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8245 | `			);` |
|        - |  8246 | `	}` |
|       28 |  8247 | `	return PH7_OK;` |
|       17 |  8248 |  |
|        - |  8249 | `/*` |
|        - |  8250 | ` * bool assert(mixed $assertion)` |
|        - |  8251 | ` *  Checks if assertion is FALSE.` |
|        - |  8252 | ` * Parameter` |
|        - |  8253 | ` *  $assertion` |
|        - |  8254 | ` *    The assertion to test.` |
|        - |  8255 | ` * Return` |
|        - |  8256 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8257 | ` */` |
|       26 |  8258 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8259 |  |
|       28 |  8260 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8261 | `	int iFlags,iResult;` |
|        - |  8262 | `	const char *zDesc;` |
|        - |  8263 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8264 | `	if( nArg < 1 ){` |
|        3 |  8265 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8266 | `			"ArgumentCountError",` |
|        - |  8267 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8268 | `			);` |
|        - |  8269 | `	}` |
|       26 |  8270 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8271 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8272 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8273 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8274 | `		return PH7_OK;` |
|        - |  8275 | `	}` |
|        - |  8276 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8277 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8278 | `	if( !iResult ){` |
|        - |  8279 | `		/* Assertion failed */` |
|        - |  8280 | `		/* Extract optional description */` |
|       13 |  8281 | `		zDesc = 0;` |
|       13 |  8282 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8283 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8284 | `		}` |
|       13 |  8285 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8286 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8287 | `			ph7_value sFile,sLine;` |
|        - |  8288 | `			ph7_value *apCbArg[3];` |
|        - |  8289 | `			SyString *pFile;` |
|        - |  8290 | `			/* Extract the processed script */` |
|      ! 0 |  8291 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8292 | `			if( pFile == 0 ){` |
|      ! 0 |  8293 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8294 | `			}` |
|        - |  8295 | `			/* Invoke the callback */` |
|      ! 0 |  8296 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8297 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8298 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8299 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8300 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8301 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8302 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8303 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8304 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8305 | `		}` |
|       13 |  8306 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8307 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8308 | `			return PH7_ABORT;` |
|        - |  8309 | `		}` |
|        - |  8310 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8311 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8312 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8313 | `				"AssertionError",` |
|        - |  8314 | `				"%s",` |
|        1 |  8315 | `				zDesc` |
|        - |  8316 | `				);` |
|      ! 0 |  8317 | `		}else{` |
|       11 |  8318 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8319 | `				"AssertionError",` |
|        - |  8320 | `				"assert(false)"` |
|        - |  8321 | `				);` |
|        - |  8322 | `		}` |
|        - |  8323 | `	}` |
|        - |  8324 | `	/* Assertion passed */` |
|       14 |  8325 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8326 | `	return PH7_OK;` |
|       15 |  8327 |  |
|        - |  8328 | `/*` |
|        - |  8329 | ` * Section:` |
|        - |  8330 | ` *  Error reporting functions.` |
|        - |  8331 | ` * Status:` |
|        - |  8332 | ` *    Stable.` |
|        - |  8333 | ` */` |
|        - |  8334 | `/*` |
|        - |  8335 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8336 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8337 | ` * Parameters` |
|        - |  8338 | ` *  $error_msg` |
|        - |  8339 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8340 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8341 | ` * $error_type` |
|        - |  8342 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8343 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8344 | ` * Return` |
|        - |  8345 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8346 | ` */` |
|       12 |  8347 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8348 |  |
|       14 |  8349 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8350 | `	int rc = PH7_OK;` |
|       14 |  8351 | `	if( nArg > 0 ){` |
|        - |  8352 | `		const char *zErr;` |
|        - |  8353 | `		int nLen;` |
|        - |  8354 | `		/* Extract the error message */` |
|       12 |  8355 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8356 | `		if( nArg > 1 ){` |
|        - |  8357 | `			/* Extract the error type */` |
|       12 |  8358 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8359 | `			switch( nErr ){` |
|        1 |  8360 | `			case 1:   /* E_ERROR */` |
|        - |  8361 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8362 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8363 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8364 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8365 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8366 | `				break;` |
|        1 |  8367 | `			case 2:   /* E_WARNING */` |
|        - |  8368 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8369 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8370 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8371 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8372 | `				break;` |
|        3 |  8373 | `			default:` |
|        8 |  8374 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8375 | `				break;` |
|        - |  8376 | `			}` |
|        5 |  8377 | `		}` |
|        - |  8378 | `		/* Report error */` |
|       12 |  8379 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8380 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8381 | `			return rc;` |
|        - |  8382 | `		}` |
|        - |  8383 | `		/* Return true */` |
|       12 |  8384 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8385 | `	}else{` |
|        - |  8386 | `		/* Missing arguments,return FALSE */` |
|        3 |  8387 | `		ph7_result_bool(pCtx,0);` |
|        - |  8388 | `	}` |
|       14 |  8389 | `	return rc;` |
|        8 |  8390 |  |
|        - |  8391 | `/*` |
|        - |  8392 | ` * int error_reporting([int $level])` |
|        - |  8393 | ` *  Sets which PHP errors are reported.` |
|        - |  8394 | ` * Parameters` |
|        - |  8395 | ` *  $level` |
|        - |  8396 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8397 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8398 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8399 | ` *   levels will not always behave as expected.` |
|        - |  8400 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8401 | ` *   in the predefined constants.` |
|        - |  8402 | ` * Return` |
|        - |  8403 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8404 | ` *   parameter is given.` |
|        - |  8405 | ` */` |
|       40 |  8406 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8407 |  |
|       42 |  8408 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8409 | `	int nOld;` |
|        - |  8410 | `	/* Extract the old reporting level */` |
|       42 |  8411 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8412 | `	if( nArg > 0 ){` |
|        - |  8413 | `		int nNew;` |
|        - |  8414 | `		/* Extract the desired error reporting level */` |
|       34 |  8415 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8416 | `		if( !nNew ){` |
|        - |  8417 | `			/* Do not report errors at all */` |
|        5 |  8418 | `			pVm->bErrReport = 0;` |
|        3 |  8419 | `		}else{` |
|        - |  8420 | `			/* Report all errors */` |
|       30 |  8421 | `			pVm->bErrReport = 1;` |
|        - |  8422 | `		}` |
|       16 |  8423 | `	}` |
|        - |  8424 | `	/* Return the old level */` |
|       42 |  8425 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8426 | `	return PH7_OK;` |
|        2 |  8427 |  |
|        - |  8428 | `/*` |
|        - |  8429 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8430 | ` *  Send an error message somewhere.` |
|        - |  8431 | ` * Parameter` |
|        - |  8432 | ` *  $message` |
|        - |  8433 | ` *   The error message that should be logged.` |
|        - |  8434 | ` *  $message_type` |
|        - |  8435 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8436 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8437 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8438 | ` *       This is the default option.` |
|        - |  8439 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8440 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8441 | ` *    2  No longer an option.` |
|        - |  8442 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8443 | ` *       to the end of the message string.` |
|        - |  8444 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8445 | ` *  $destination` |
|        - |  8446 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8447 | ` *  $extra_headers` |
|        - |  8448 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8449 | ` * Return` |
|        - |  8450 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8451 | ` * NOTE:` |
|        - |  8452 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8453 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8454 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8455 | ` *  Otherwise this function is no-op.` |
|        - |  8456 | ` */` |
|        4 |  8457 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8458 |  |
|        - |  8459 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8460 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8461 | `	int iType = 0;` |
|        5 |  8462 | `	if( nArg < 1 ){` |
|        - |  8463 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8464 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8465 | `		return PH7_OK;` |
|        - |  8466 | `	}` |
|        5 |  8467 | `	if( pVm->xErrLog  ){` |
|        - |  8468 | `		/* Invoke the user callback */` |
|      ! 0 |  8469 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8470 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8471 | `		if( nArg > 1 ){` |
|      ! 0 |  8472 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8473 | `			if( nArg > 2 ){` |
|      ! 0 |  8474 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8475 | `				if( nArg > 3 ){` |
|      ! 0 |  8476 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8477 | `				}` |
|      ! 0 |  8478 | `			}` |
|      ! 0 |  8479 | `		}` |
|      ! 0 |  8480 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8481 | `	}` |
|        - |  8482 | `	/* Retun TRUE */` |
|        5 |  8483 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8484 | `	return PH7_OK;` |
|        3 |  8485 |  |
|        - |  8486 | `/*` |
|        - |  8487 | ` * bool restore_exception_handler(void)` |
|        - |  8488 | ` *  Restores the previously defined exception handler function.` |
|        - |  8489 | ` * Parameter` |
|        - |  8490 | ` *  None` |
|        - |  8491 | ` * Return` |
|        - |  8492 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8493 | ` */` |
|        4 |  8494 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8495 |  |
|        5 |  8496 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8497 | `	ph7_value *pOld,*pNew;` |
|        - |  8498 | `	/* Point to the old and the new handler */` |
|        5 |  8499 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8500 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8501 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8502 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8503 | `		SXUNUSED(apArg);` |
|        - |  8504 | `		/* No installed handler,return FALSE */` |
|        5 |  8505 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8506 | `		return PH7_OK;` |
|        - |  8507 | `	}` |
|        - |  8508 | `	/* Copy the old handler */` |
|      ! 0 |  8509 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8510 | `	PH7_MemObjRelease(pOld);` |
|        - |  8511 | `	/* Return TRUE */` |
|      ! 0 |  8512 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8513 | `	return PH7_OK;` |
|        3 |  8514 |  |
|        - |  8515 | `/*` |
|        - |  8516 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8517 | ` *  Sets a user-defined exception handler function.` |
|        - |  8518 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8519 | ` * NOTE` |
|        - |  8520 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8521 | ` *  the satndard PHP engine.` |
|        - |  8522 | ` * Parameters` |
|        - |  8523 | ` *  $exception_handler` |
|        - |  8524 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8525 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8526 | ` *   that was thrown.` |
|        - |  8527 | ` *  Note:` |
|        - |  8528 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8529 | ` * Return` |
|        - |  8530 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8531 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8532 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8533 | ` */` |
|        4 |  8534 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8535 |  |
|        6 |  8536 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8537 | `	ph7_value *pOld,*pNew;` |
|        - |  8538 | `	/* Point to the old and the new handler */` |
|        6 |  8539 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8540 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8541 | `	/* Return the old handler */` |
|        6 |  8542 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8543 | `	if( nArg > 0 ){` |
|        6 |  8544 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8545 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8546 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8547 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8548 | `		}else{` |
|        6 |  8549 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8550 | `			/* Install the new handler */` |
|        6 |  8551 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8552 | `		}` |
|        2 |  8553 | `	}` |
|        6 |  8554 | `	return PH7_OK;` |
|        2 |  8555 |  |
|        - |  8556 | `/*` |
|        - |  8557 | ` * bool restore_error_handler(void)` |
|        - |  8558 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8559 | ` * Parameters:` |
|        - |  8560 | ` *  None.` |
|        - |  8561 | ` * Return` |
|        - |  8562 | ` *  Always TRUE.` |
|        - |  8563 | ` */` |
|        4 |  8564 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8565 |  |
|        5 |  8566 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8567 | `	ph7_value *pOld,*pNew;` |
|        - |  8568 | `	/* Point to the old and the new handler */` |
|        5 |  8569 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8570 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8571 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8572 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8573 | `		SXUNUSED(apArg);` |
|        - |  8574 | `		/* No installed callback,return FALSE */` |
|        5 |  8575 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8576 | `		return PH7_OK;` |
|        - |  8577 | `	}` |
|        - |  8578 | `	/* Copy the old callback */` |
|      ! 0 |  8579 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8580 | `	PH7_MemObjRelease(pOld);` |
|        - |  8581 | `	/* Return TRUE */` |
|      ! 0 |  8582 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8583 | `	return PH7_OK;` |
|        3 |  8584 |  |
|        - |  8585 | `/*` |
|        - |  8586 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8587 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8588 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8589 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8590 | ` *  Sets a user-defined error handler function.` |
|        - |  8591 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8592 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8593 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8594 | ` *  conditions (using trigger_error()).` |
|        - |  8595 | ` * Parameters` |
|        - |  8596 | ` *  $error_handler` |
|        - |  8597 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8598 | ` *   describing the error.` |
|        - |  8599 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8600 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8601 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8602 | ` *   The function can be shown as:` |
|        - |  8603 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8604 | ` *     errno` |
|        - |  8605 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8606 | ` *   errstr` |
|        - |  8607 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8608 | ` *   errfile` |
|        - |  8609 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8610 | ` *     was raised in, as a string.` |
|        - |  8611 | ` *  Note:` |
|        - |  8612 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8613 | ` * Return` |
|        - |  8614 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8615 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8616 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8617 | ` */` |
|     8822 |  8618 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8619 |  |
|     8824 |  8620 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8621 | `	ph7_value *pOld,*pNew;` |
|        - |  8622 | `	/* Point to the old and the new handler */` |
|     8824 |  8623 | `	pOld = &pVm->aErrCB[0];` |
|     8824 |  8624 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8625 | `	/* Return the old handler */` |
|     8824 |  8626 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 |  8627 | `	if( nArg > 0 ){` |
|     8824 |  8628 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8629 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 |  8630 | `			PH7_MemObjRelease(pNew);` |
|     4411 |  8631 | `			ph7_result_bool(pCtx,1);` |
|     2206 |  8632 | `		}else{` |
|     4414 |  8633 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8634 | `			/* Install the new handler */` |
|     4414 |  8635 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8636 | `		}` |
|     4411 |  8637 | `	}` |
|     8824 |  8638 | `	return PH7_OK;` |
|        2 |  8639 |  |
|        - |  8640 | `/*` |
|        - |  8641 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8642 | ` *  Generates a backtrace.` |
|        - |  8643 | ` * Paramaeter` |
|        - |  8644 | ` *  $options` |
|        - |  8645 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8646 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8647 | ` *   all the function/method arguments, to save memory.` |
|        - |  8648 | ` * $limit` |
|        - |  8649 | ` *   (Not Used)` |
|        - |  8650 | ` * Return` |
|        - |  8651 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8652 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8653 | ` *          Name        Type      Description` |
|        - |  8654 | ` *          ------      ------     -----------` |
|        - |  8655 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8656 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8657 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8658 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8659 | ` *          object      object    The current object.` |
|        - |  8660 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8661 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8662 | ` */` |
|      504 |  8663 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8664 |  |
|      506 |  8665 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8666 | `	ph7_value *pArray;` |
|        - |  8667 | `	ph7_class *pClass;` |
|        - |  8668 | `	ph7_value *pValue;` |
|        - |  8669 | `	SyString *pFile;` |
|        - |  8670 | `	/* Create a new array */` |
|      506 |  8671 | `	pArray = ph7_context_new_array(pCtx);` |
|      506 |  8672 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      506 |  8673 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8674 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8675 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8676 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8677 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8678 | `		SXUNUSED(apArg);` |
|      ! 0 |  8679 | `		return PH7_OK;` |
|        - |  8680 | `	}` |
|        - |  8681 | `	/* Dump running function name and it's arguments  */` |
|      506 |  8682 | `	if( pVm->pFrame->pParent ){` |
|      506 |  8683 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8684 | `		ph7_vm_func *pFunc;` |
|        - |  8685 | `		ph7_value *pArg;` |
|      506 |  8686 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      506 |  8687 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      506 |  8688 | `		if( pFrame->pParent && pFunc ){` |
|      506 |  8689 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      506 |  8690 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      506 |  8691 | `			ph7_value_reset_string_cursor(pValue);` |
|      252 |  8692 | `		}` |
|        - |  8693 | `		/* Function arguments */` |
|      506 |  8694 | `		pArg = ph7_context_new_array(pCtx);` |
|      506 |  8695 | `		if( pArg  ){` |
|        - |  8696 | `			ph7_value *pObj;` |
|        - |  8697 | `			VmSlot *aSlot;` |
|        - |  8698 | `			sxu32 n;` |
|        - |  8699 | `			/* Start filling the array with the given arguments */` |
|      506 |  8700 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2010 |  8701 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1506 |  8702 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1506 |  8703 | `				if( pObj ){` |
|     1506 |  8704 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      752 |  8705 | `				}` |
|      754 |  8706 | `			}` |
|        - |  8707 | `			/* Save the array */` |
|      506 |  8708 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      252 |  8709 | `		}` |
|      252 |  8710 | `	}` |
|      506 |  8711 | `	ph7_value_int(pValue,1);` |
|        - |  8712 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8713 | `	 * line numbers at run-time. )` |
|        - |  8714 | `	 */` |
|      506 |  8715 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8716 | `	/* Current processed script */` |
|      506 |  8717 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      506 |  8718 | `	if( pFile ){` |
|      506 |  8719 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      506 |  8720 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      506 |  8721 | `		ph7_value_reset_string_cursor(pValue);` |
|      252 |  8722 | `	}` |
|        - |  8723 | `	/* Top class */` |
|      506 |  8724 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      506 |  8725 | `	if( pClass ){` |
|      502 |  8726 | `		ph7_value_reset_string_cursor(pValue);` |
|      502 |  8727 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      502 |  8728 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      250 |  8729 | `	}` |
|        - |  8730 | `	/* Return the freshly created array */` |
|      506 |  8731 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8732 | `	/*` |
|        - |  8733 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8734 | `	 * as soon we return from this function.` |
|        - |  8735 | `	 */` |
|      506 |  8736 | `	return PH7_OK;` |
|      254 |  8737 |  |
|        - |  8738 | `/*` |
|        - |  8739 | ` * Generate a small backtrace.` |
|        - |  8740 | ` * Store the generated dump in the given BLOB` |
|        - |  8741 | ` */` |
|        4 |  8742 | `static int VmMiniBacktrace(` |
|        - |  8743 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8744 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8745 | `	)` |
|        1 |  8746 |  |
|        5 |  8747 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8748 | `	ph7_vm_func *pFunc;` |
|        - |  8749 | `	ph7_class *pClass;` |
|        - |  8750 | `	SyString *pFile;` |
|        - |  8751 | `	/* Called function */` |
|        5 |  8752 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  8753 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8754 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8755 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8756 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8757 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8758 | `	}else{` |
|      ! 0 |  8759 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8760 | `	}` |
|        5 |  8761 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8762 | `	/* Current processed script */` |
|        5 |  8763 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8764 | `	if( pFile ){` |
|        5 |  8765 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8766 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8767 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8768 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8769 | `	}` |
|        - |  8770 | `	/* Top class */` |
|        5 |  8771 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8772 | `	if( pClass ){` |
|      ! 0 |  8773 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8774 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8775 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8776 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8777 | `	}` |
|        5 |  8778 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8779 | `	/* All done */` |
|        5 |  8780 | `	return SXRET_OK;` |
|        1 |  8781 |  |
|        - |  8782 | `/*` |
|        - |  8783 | ` * void debug_print_backtrace()` |
|        - |  8784 | ` *  Prints a backtrace` |
|        - |  8785 | ` * Parameters` |
|        - |  8786 | ` * None` |
|        - |  8787 | ` * Return` |
|        - |  8788 | ` * NULL` |
|        - |  8789 | ` */` |
|        2 |  8790 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8791 |  |
|        3 |  8792 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8793 | `	SyBlob sDump;` |
|        3 |  8794 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8795 | `	/* Generate the backtrace */` |
|        3 |  8796 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8797 | `	/* Output backtrace */` |
|        3 |  8798 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8799 | `	/* All done,cleanup */` |
|        3 |  8800 | `	SyBlobRelease(&sDump);` |
|        1 |  8801 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8802 | `	SXUNUSED(apArg);` |
|        3 |  8803 | `	return PH7_OK;` |
|        1 |  8804 |  |
|        - |  8805 | `/*` |
|        - |  8806 | ` * string debug_string_backtrace()` |
|        - |  8807 | ` *  Generate a backtrace` |
|        - |  8808 | ` * Parameters` |
|        - |  8809 | ` * None` |
|        - |  8810 | ` * Return` |
|        - |  8811 | ` *  A mini backtrace().` |
|        - |  8812 | ` * Note that this is a symisc extension.` |
|        - |  8813 | ` */` |
|        2 |  8814 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8815 |  |
|        3 |  8816 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8817 | `	SyBlob sDump;` |
|        3 |  8818 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8819 | `	/* Generate the backtrace */` |
|        3 |  8820 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8821 | `	/* Return the backtrace */` |
|        3 |  8822 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8823 | `	/* All done,cleanup */` |
|        3 |  8824 | `	SyBlobRelease(&sDump);` |
|        1 |  8825 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8826 | `	SXUNUSED(apArg);` |
|        3 |  8827 | `	return PH7_OK;` |
|        1 |  8828 |  |
|        - |  8829 | `/*` |
|        - |  8830 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8831 | ` * exception is triggered.` |
|        - |  8832 | ` */` |
|      472 |  8833 | `static sxi32 VmUncaughtException(` |
|        - |  8834 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8835 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8836 | `	)` |
|        1 |  8837 |  |
|        - |  8838 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8839 | `	int nArg = 1;` |
|        - |  8840 | `	sxi32 rc;` |
|      473 |  8841 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8842 | `		/* Nesting limit reached */` |
|      ! 0 |  8843 | `		return SXRET_OK;` |
|        - |  8844 | `	}` |
|        - |  8845 | `	/* Call any exception handler if available */` |
|      473 |  8846 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8847 | `	if( pThis ){` |
|        - |  8848 | `		/* Load the exception instance */` |
|      473 |  8849 | `		sArg.x.pOther = pThis;` |
|      473 |  8850 | `		pThis->iRef++;` |
|      473 |  8851 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8852 | `	}else{` |
|      ! 0 |  8853 | `		nArg = 0;` |
|        - |  8854 | `	}` |
|      473 |  8855 | `	apArg[0] = &sArg;` |
|        - |  8856 | `	/* Call the exception handler if available */` |
|      473 |  8857 | `	pVm->nExceptDepth++;` |
|      473 |  8858 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8859 | `	pVm->nExceptDepth--;` |
|      473 |  8860 | `	if( rc != SXRET_OK ){` |
|        - |  8861 | `		SyBlob sMsgBuf;` |
|      471 |  8862 | `		const char *zClass = "Exception";` |
|      471 |  8863 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8864 | `		const char *zMsg;` |
|        - |  8865 | `		sxu32 nMsg;` |
|        - |  8866 | `		const char *zFuncName;` |
|        - |  8867 | `		int nFuncLen;` |
|      471 |  8868 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8869 | `		if( pThis ){` |
|        - |  8870 | `			ph7_class_method *pGetMessage;` |
|        - |  8871 | `			ph7_value sMsg;` |
|        - |  8872 | `			const char *zTmp;` |
|        - |  8873 | `			int nTmp;` |
|      471 |  8874 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8875 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8876 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8877 | `			if( pGetMessage ){` |
|      471 |  8878 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8879 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8880 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8881 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8882 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8883 | `					}` |
|      235 |  8884 | `				}` |
|      471 |  8885 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8886 | `			}` |
|      235 |  8887 | `		}` |
|      471 |  8888 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8889 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8890 | `		}` |
|      471 |  8891 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8892 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8893 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8894 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8895 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8896 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8897 | `		rc = SXERR_ABORT;` |
|      235 |  8898 | `	}` |
|      473 |  8899 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8900 | `	return rc;` |
|      237 |  8901 |  |
|        - |  8902 | `/*` |
|        - |  8903 | ` * Throw a user exception.` |
|        - |  8904 | ` *` |
|        - |  8905 | ` * Exception dispatch follows this sequence:` |
|        - |  8906 | ` *` |
|        - |  8907 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - |  8908 | ` *    try/catch whose catch block matches the exception class.` |
|        - |  8909 | ` *` |
|        - |  8910 | ` * 2. If NO catch matches:` |
|        - |  8911 | ` *    a. Run finally (if present) for the current try block.` |
|        - |  8912 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - |  8913 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - |  8914 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - |  8915 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - |  8916 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - |  8917 | ` *    d. Otherwise, report as truly uncaught.` |
|        - |  8918 | ` *` |
|        - |  8919 | ` * 3. If a catch DOES match:` |
|        - |  8920 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - |  8921 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - |  8922 | ` *       inside the catch body from immediately propagating past our` |
|        - |  8923 | ` *       finally block.` |
|        - |  8924 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - |  8925 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - |  8926 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - |  8927 | ` *       in pPendingException (step 2c).` |
|        - |  8928 | ` *    c. Restore outer handlers from the saved copy.` |
|        - |  8929 | ` *    d. Run finally (if present).` |
|        - |  8930 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - |  8931 | ` *       that handlers are restored and finally has run.` |
|        - |  8932 | ` */` |
|      508 |  8933 | `static sxi32 VmThrowException(` |
|        - |  8934 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8935 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8936 | `	)` |
|        2 |  8937 |  |
|        - |  8938 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8939 | `	ph7_exception **apException;` |
|        - |  8940 | `	ph7_exception *pException;` |
|        - |  8941 | `	/* Point to the stack of loaded exceptions */` |
|      510 |  8942 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      510 |  8943 | `	pException = 0;` |
|      510 |  8944 | `	pCatch = 0;` |
|      510 |  8945 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8946 | `		ph7_exception_block *aCatch;` |
|        - |  8947 | `		ph7_class *pClass;` |
|        - |  8948 | `		sxu32 j;` |
|        - |  8949 | `		/* Locate the appropriate block to execute */` |
|       34 |  8950 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       34 |  8951 | `		(void)SySetPop(&pVm->aException);` |
|       34 |  8952 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       34 |  8953 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       32 |  8954 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8955 | `			/* Extract the target class */` |
|       32 |  8956 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       32 |  8957 | `			if( pClass == 0 ){` |
|        - |  8958 | `				/* No such class */` |
|      ! 0 |  8959 | `				continue;` |
|        - |  8960 | `			}` |
|       32 |  8961 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8962 | `				/* Catch block found,break immeditaley */` |
|       32 |  8963 | `				pCatch = &aCatch[j];` |
|       32 |  8964 | `				break;` |
|        - |  8965 | `			}` |
|      ! 0 |  8966 | `		}` |
|       16 |  8967 | `	}` |
|        - |  8968 | `	/* Execute the cached block if available */` |
|      510 |  8969 | `	if( pCatch == 0 ){` |
|        - |  8970 | `		sxi32 rc;` |
|        - |  8971 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  8972 | `		if( pException && pException->iHasFinally ){` |
|        3 |  8973 | `			pException->iFinallyDone = 1;` |
|        3 |  8974 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  8975 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8976 | `				return SXERR_ABORT;` |
|        - |  8977 | `			}` |
|        1 |  8978 | `		}` |
|        - |  8979 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  8980 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8981 | `			/* Re-throw to the outer handler */` |
|        3 |  8982 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  8983 | `		}` |
|        - |  8984 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  8985 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  8986 | `		 * exception instead of reporting it uncaught.` |
|        - |  8987 | `		 */` |
|      478 |  8988 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  8989 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  8990 | `			 * by looking for a catch frame on the stack.` |
|        - |  8991 | `			 */` |
|      478 |  8992 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  8993 | `			int inCatch = 0;` |
|      956 |  8994 | `			while( pF ){` |
|      484 |  8995 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  8996 | `					inCatch = 1;` |
|        6 |  8997 | `					break;` |
|        - |  8998 | `				}` |
|      479 |  8999 | `				pF = pF->pParent;` |
|        1 |  9000 | `			}` |
|      478 |  9001 | `			if( inCatch ){` |
|        - |  9002 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  9003 | `				pThis->iRef++;` |
|        6 |  9004 | `				pVm->pPendingException = pThis;` |
|        6 |  9005 | `				return SXRET_OK;` |
|        - |  9006 | `			}` |
|      236 |  9007 | `		}` |
|        - |  9008 | `		/* Truly uncaught */` |
|      473 |  9009 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  9010 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9011 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9012 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 |  9013 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  9014 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9015 | `			}` |
|      ! 0 |  9016 | `		}` |
|      473 |  9017 | `		return rc;` |
|      ! 0 |  9018 | `	}else{` |
|       32 |  9019 | `		VmFrame *pFrame = pVm->pFrame;` |
|       32 |  9020 | `		ph7_exception **apSaved = 0;` |
|        - |  9021 | `		sxu32 nSavedCount;` |
|        - |  9022 | `		sxi32 rc;` |
|       32 |  9023 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       32 |  9024 | `		if( pException->pFrame == pFrame ){` |
|       24 |  9025 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 |  9026 | `		}` |
|        - |  9027 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  9028 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  9029 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  9030 | `		 */` |
|       32 |  9031 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       32 |  9032 | `		if( nSavedCount > 0 ){` |
|       11 |  9033 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  9034 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  9035 | `			if( apSaved ){` |
|       11 |  9036 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  9037 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  9038 | `				SySetReset(&pVm->aException);` |
|        3 |  9039 | `			}` |
|        3 |  9040 | `		}` |
|        - |  9041 | `		/* Create a private frame first */` |
|       32 |  9042 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       32 |  9043 | `		if( rc == SXRET_OK ){` |
|       32 |  9044 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       32 |  9045 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       32 |  9046 | `			if( pObj ){` |
|       32 |  9047 | `				pThis->iRef++;` |
|       32 |  9048 | `				pObj->x.pOther = pThis;` |
|       32 |  9049 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       15 |  9050 | `			}` |
|        - |  9051 | `			/* Execute the catch block */` |
|       32 |  9052 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9053 | `			/* Leave the frame */` |
|       32 |  9054 | `			VmLeaveFrame(&(*pVm));` |
|       15 |  9055 | `		}` |
|        - |  9056 | `		/* Restore the outer exception handlers */` |
|       32 |  9057 | `		if( apSaved ){` |
|        - |  9058 | `			sxu32 k;` |
|        - |  9059 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  9060 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  9061 | `			 * Restore the original outer entries.` |
|        - |  9062 | `			 */` |
|        8 |  9063 | `			SySetReset(&pVm->aException);` |
|       14 |  9064 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  9065 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  9066 | `			}` |
|        8 |  9067 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  9068 | `		}` |
|        - |  9069 | `		/* Execute the finally block after catch */` |
|       32 |  9070 | `		if( pException->iHasFinally ){` |
|       11 |  9071 | `			pException->iFinallyDone = 1;` |
|        - |  9072 | `			{` |
|       11 |  9073 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 |  9074 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  9075 | `					return SXERR_ABORT;` |
|        - |  9076 | `				}` |
|        - |  9077 | `			}` |
|        5 |  9078 | `		}` |
|       32 |  9079 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9080 | `			return SXERR_ABORT;` |
|        - |  9081 | `		}` |
|        - |  9082 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  9083 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  9084 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  9085 | `		 */` |
|       32 |  9086 | `		if( pVm->pPendingException ){` |
|        6 |  9087 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  9088 | `			pVm->pPendingException = 0;` |
|        6 |  9089 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  9090 | `		}` |
|        - |  9091 | `	}` |
|        - |  9092 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9093 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9094 | `	 */` |
|       28 |  9095 | `	return SXRET_OK;` |
|      256 |  9096 |  |
|        - |  9097 | `/*` |
|        - |  9098 | ` * Section:` |
|        - |  9099 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9100 | ` * Status:` |
|        - |  9101 | ` *    Stable.` |
|        - |  9102 | ` */` |
|        - |  9103 | `/*` |
|        - |  9104 | ` * string ph7version(void)` |
|        - |  9105 | ` *  Returns the running version of the PH7 version.` |
|        - |  9106 | ` * Parameters` |
|        - |  9107 | ` *  None` |
|        - |  9108 | ` * Return` |
|        - |  9109 | ` * Current PH7 version.` |
|        - |  9110 | ` */` |
|        2 |  9111 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9112 |  |
|        1 |  9113 | `	SXUNUSED(nArg);` |
|        1 |  9114 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9115 | `	/* Current engine version */` |
|        3 |  9116 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9117 | `	return PH7_OK;` |
|        1 |  9118 |  |
|        - |  9119 | `/*` |
|        - |  9120 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9121 | ` */` |
|        - |  9122 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9123 | ` "<html><head>"\` |
|        - |  9124 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9125 | ` "<style type=\"text/css\">"\` |
|        - |  9126 | ` "div {"\` |
|        - |  9127 | `     "border: 1px solid #cccccc;"\` |
|        - |  9128 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9129 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9130 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9131 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9132 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9133 | `     "-o-border-radius: 10px;"\` |
|        - |  9134 | `     "border-radius: 10px;"\` |
|        - |  9135 | `     "padding-left: 2em;"\` |
|        - |  9136 | `     "background-color: white;"\` |
|        - |  9137 | `     "margin-left: auto;"\` |
|        - |  9138 | `     "font-family: verdana;"\` |
|        - |  9139 | `     "padding-right: 2em;"\` |
|        - |  9140 | `     "margin-right: auto;"\` |
|        - |  9141 | `     "}"\` |
|        - |  9142 | `     "body {"\` |
|        - |  9143 | `     "padding: 0.2em;"\` |
|        - |  9144 | `     "font-style: normal;"\` |
|        - |  9145 | `     "font-size: medium;"\` |
|        - |  9146 | `     "background-color: #f2f2f2;"\` |
|        - |  9147 | `     "}"\` |
|        - |  9148 | `     "hr {"\` |
|        - |  9149 | `     "border-style: solid none none;"\` |
|        - |  9150 | `     "border-width: 1px medium medium;"\` |
|        - |  9151 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9152 | `     "height: 1px;"\` |
|        - |  9153 | `     "}"\` |
|        - |  9154 | `     "a {"\` |
|        - |  9155 | `     "color: #3366cc;"\` |
|        - |  9156 | `     "text-decoration: none;"\` |
|        - |  9157 | `     "}"\` |
|        - |  9158 | `     "a:hover {"\` |
|        - |  9159 | `     "color: #999999;"\` |
|        - |  9160 | `     "}"\` |
|        - |  9161 | `     "a:active {"\` |
|        - |  9162 | `     "color: #663399;"\` |
|        - |  9163 | `     "}"\` |
|        - |  9164 | `     "h1 {"\` |
|        - |  9165 | `     "margin: 0;"\` |
|        - |  9166 | `     "padding: 0;"\` |
|        - |  9167 | `     "font-family: Verdana;"\` |
|        - |  9168 | `     "font-weight: bold;"\` |
|        - |  9169 | `     "font-style: normal;"\` |
|        - |  9170 | `     "font-size: medium;"\` |
|        - |  9171 | `     "text-transform: capitalize;"\` |
|        - |  9172 | `     "color: #0a328c;"\` |
|        - |  9173 | `     "}"\` |
|        - |  9174 | `     "p {"\` |
|        - |  9175 | `     "margin: 0 auto;"\` |
|        - |  9176 | `     "font-size: medium;"\` |
|        - |  9177 | `     "font-style: normal;"\` |
|        - |  9178 | `     "font-family: verdana;"\` |
|        - |  9179 | `     "}"\` |
|        - |  9180 | `"</style></head><body>"\` |
|        - |  9181 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9182 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9183 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9184 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9185 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9186 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9187 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9188 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9189 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9190 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9191 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9192 |  |
|        - |  9193 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9194 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9195 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9196 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9197 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9198 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9199 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9200 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9201 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9202 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9203 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9204 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9205 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9206 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9207 |  |
|        - |  9208 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9209 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9210 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9211 | `"&nbsp;*<br>"\` |
|        - |  9212 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9213 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9214 | `"&nbsp;* are met:<br>"\` |
|        - |  9215 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9216 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9217 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9218 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9219 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9220 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9221 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9222 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9223 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9224 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9225 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9226 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9227 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9228 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9229 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9230 | `"&nbsp;*<br>"\` |
|        - |  9231 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9232 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9233 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9234 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9235 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9236 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9237 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9238 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9239 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9240 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9241 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9242 | `"&nbsp;*/<br>"\` |
|        - |  9243 | `"</span></small></small></p>"\` |
|        - |  9244 | `"</div></body></html>"` |
|        - |  9245 | `/*` |
|        - |  9246 | ` * bool ph7credits(void)` |
|        - |  9247 | ` * bool ph7info(void)` |
|        - |  9248 | ` * bool ph7copyright(void)` |
|        - |  9249 | ` *  Prints out the credits for PH7 engine` |
|        - |  9250 | ` * Parameters` |
|        - |  9251 | ` *  None` |
|        - |  9252 | ` * Return` |
|        - |  9253 | ` *  Always TRUE` |
|        - |  9254 | ` */` |
|        2 |  9255 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9256 |  |
|        3 |  9257 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9258 | `	/* Expand the HTML page above*/` |
|        3 |  9259 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9260 | `	ph7_context_output_format(` |
|        1 |  9261 | `		pCtx,` |
|        - |  9262 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9263 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9264 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9265 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9266 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9267 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9268 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9269 | `#ifdef __WINNT__` |
|        - |  9270 | `		"Windows NT"` |
|        - |  9271 | `#elif defined(__UNIXES__)` |
|        - |  9272 | `		"UNIX-Like"` |
|        - |  9273 | `#else` |
|        - |  9274 | `		"Other OS"` |
|        - |  9275 | `#endif` |
|        - |  9276 | `		);` |
|        3 |  9277 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9278 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9279 | `	SXUNUSED(apArg);` |
|        - |  9280 | `	/* Return TRUE */` |
|        - |  9281 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9282 | `	return PH7_OK;` |
|        1 |  9283 |  |
|        - |  9284 | `/*` |
|        - |  9285 | ` * Section:` |
|        - |  9286 | ` *    URL related routines.` |
|        - |  9287 | ` * Status:` |
|        - |  9288 | ` *    Stable.` |
|        - |  9289 | ` */` |
|        - |  9290 | `/*` |
|        - |  9291 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9292 | ` *  Parse a URL and return its fields.` |
|        - |  9293 | ` * Parameters` |
|        - |  9294 | ` *  $url` |
|        - |  9295 | ` *   The URL to parse.` |
|        - |  9296 | ` * $component` |
|        - |  9297 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9298 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9299 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9300 | ` *  in which case the return value will be an integer).` |
|        - |  9301 | ` * Return` |
|        - |  9302 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9303 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9304 | ` *  this array are:` |
|        - |  9305 | ` *   scheme - e.g. http` |
|        - |  9306 | ` *   host` |
|        - |  9307 | ` *   port` |
|        - |  9308 | ` *   user` |
|        - |  9309 | ` *   pass` |
|        - |  9310 | ` *   path` |
|        - |  9311 | ` *   query - after the question mark ?` |
|        - |  9312 | ` *   fragment - after the hashmark #` |
|        - |  9313 | ` * Note:` |
|        - |  9314 | ` *  FALSE is returned on failure.` |
|        - |  9315 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9316 | ` *  with the standard PHP engine.` |
|        - |  9317 | ` */` |
|       28 |  9318 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9319 |  |
|        - |  9320 | `	const char *zStr; /* Input string */` |
|        - |  9321 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9322 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9323 | `	int nLen;` |
|        - |  9324 | `	sxi32 rc;` |
|       29 |  9325 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9326 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9327 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9328 | `		return PH7_OK;` |
|        - |  9329 | `	}` |
|        - |  9330 | `	/* Extract the given URI */` |
|       29 |  9331 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9332 | `	if( nLen < 1 ){` |
|        - |  9333 | `		/* Nothing to process,return FALSE */` |
|        3 |  9334 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9335 | `		return PH7_OK;` |
|        - |  9336 | `	}` |
|        - |  9337 | `	/* Get a parse */` |
|       27 |  9338 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9339 | `	if( rc != SXRET_OK ){` |
|        - |  9340 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9341 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9342 | `		return PH7_OK;` |
|        - |  9343 | `	}` |
|       27 |  9344 | `	if( nArg > 1 ){` |
|      ! 0 |  9345 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9346 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9347 | `		switch(nComponent){` |
|      ! 0 |  9348 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9349 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9350 | `			if( pComp->nByte < 1 ){` |
|        - |  9351 | `				/* No available value,return NULL */` |
|      ! 0 |  9352 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9353 | `			}else{` |
|      ! 0 |  9354 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9355 | `			}` |
|      ! 0 |  9356 | `			break;` |
|      ! 0 |  9357 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9358 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9359 | `			if( pComp->nByte < 1 ){` |
|        - |  9360 | `				/* No available value,return NULL */` |
|      ! 0 |  9361 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9362 | `			}else{` |
|      ! 0 |  9363 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9364 | `			}` |
|      ! 0 |  9365 | `			break;` |
|      ! 0 |  9366 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9367 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9368 | `			if( pComp->nByte < 1 ){` |
|        - |  9369 | `				/* No available value,return NULL */` |
|      ! 0 |  9370 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9371 | `			}else{` |
|      ! 0 |  9372 | `				int iPort = 0;` |
|        - |  9373 | `				/* Cast the value to integer */` |
|      ! 0 |  9374 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9375 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9376 | `			}` |
|      ! 0 |  9377 | `			break;` |
|      ! 0 |  9378 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9379 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9380 | `			if( pComp->nByte < 1 ){` |
|        - |  9381 | `				/* No available value,return NULL */` |
|      ! 0 |  9382 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9383 | `			}else{` |
|      ! 0 |  9384 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9385 | `			}` |
|      ! 0 |  9386 | `			break;` |
|      ! 0 |  9387 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9388 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9389 | `			if( pComp->nByte < 1 ){` |
|        - |  9390 | `				/* No available value,return NULL */` |
|      ! 0 |  9391 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9392 | `			}else{` |
|      ! 0 |  9393 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9394 | `			}` |
|      ! 0 |  9395 | `			break;` |
|      ! 0 |  9396 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9397 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9398 | `			if( pComp->nByte < 1 ){` |
|        - |  9399 | `				/* No available value,return NULL */` |
|      ! 0 |  9400 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9401 | `			}else{` |
|      ! 0 |  9402 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9403 | `			}` |
|      ! 0 |  9404 | `			break;` |
|      ! 0 |  9405 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9406 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9407 | `			if( pComp->nByte < 1 ){` |
|        - |  9408 | `				/* No available value,return NULL */` |
|      ! 0 |  9409 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9410 | `			}else{` |
|      ! 0 |  9411 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9412 | `			}` |
|      ! 0 |  9413 | `			break;` |
|      ! 0 |  9414 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9415 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9416 | `			if( pComp->nByte < 1 ){` |
|        - |  9417 | `				/* No available value,return NULL */` |
|      ! 0 |  9418 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9419 | `			}else{` |
|      ! 0 |  9420 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9421 | `			}` |
|      ! 0 |  9422 | `			break;` |
|      ! 0 |  9423 | `		default:` |
|        - |  9424 | `			/* No such entry,return NULL */` |
|      ! 0 |  9425 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9426 | `			break;` |
|        - |  9427 | `		}` |
|      ! 0 |  9428 | `	}else{` |
|        - |  9429 | `		ph7_value *pArray,*pValue;` |
|        - |  9430 | `		/* Return an associative array */` |
|       27 |  9431 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9432 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9433 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9434 | `			/* Out of memory */` |
|      ! 0 |  9435 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9436 | `			/* Return false */` |
|      ! 0 |  9437 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9438 | `			return PH7_OK;` |
|        - |  9439 | `		}` |
|        - |  9440 | `		/* Fill the array */` |
|       27 |  9441 | `		pComp = &sURI.sScheme;` |
|       27 |  9442 | `		if( pComp->nByte > 0 ){` |
|       19 |  9443 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9444 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9445 | `		}` |
|        - |  9446 | `		/* Reset the string cursor */` |
|       27 |  9447 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9448 | `		pComp = &sURI.sHost;` |
|       27 |  9449 | `		if( pComp->nByte > 0 ){` |
|       25 |  9450 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9451 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9452 | `		}` |
|        - |  9453 | `		/* Reset the string cursor */` |
|       27 |  9454 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9455 | `		pComp = &sURI.sPort;` |
|       27 |  9456 | `		if( pComp->nByte > 0 ){` |
|       11 |  9457 | `			int iPort = 0;/* cc warning */` |
|        - |  9458 | `			/* Convert to integer */` |
|       11 |  9459 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9460 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9461 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9462 | `		}` |
|        - |  9463 | `		/* Reset the string cursor */` |
|       27 |  9464 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9465 | `		pComp = &sURI.sUser;` |
|       27 |  9466 | `		if( pComp->nByte > 0 ){` |
|        7 |  9467 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9468 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9469 | `		}` |
|        - |  9470 | `		/* Reset the string cursor */` |
|       27 |  9471 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9472 | `		pComp = &sURI.sPass;` |
|       27 |  9473 | `		if( pComp->nByte > 0 ){` |
|        7 |  9474 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9475 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9476 | `		}` |
|        - |  9477 | `		/* Reset the string cursor */` |
|       27 |  9478 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9479 | `		pComp = &sURI.sPath;` |
|       27 |  9480 | `		if( pComp->nByte > 0 ){` |
|       17 |  9481 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9482 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9483 | `		}` |
|        - |  9484 | `		/* Reset the string cursor */` |
|       27 |  9485 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9486 | `		pComp = &sURI.sQuery;` |
|       27 |  9487 | `		if( pComp->nByte > 0 ){` |
|        5 |  9488 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9489 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9490 | `		}` |
|        - |  9491 | `		/* Reset the string cursor */` |
|       27 |  9492 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9493 | `		pComp = &sURI.sFragment;` |
|       27 |  9494 | `		if( pComp->nByte > 0 ){` |
|        5 |  9495 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9496 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9497 | `		}` |
|        - |  9498 | `		/* Return the created array */` |
|       27 |  9499 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9500 | `		/* NOTE:` |
|        - |  9501 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9502 | `		 * automatically as soon we return from this function.` |
|        - |  9503 | `		 */` |
|        - |  9504 | `	}` |
|        - |  9505 | `	/* All done */` |
|       27 |  9506 | `	return PH7_OK;` |
|       15 |  9507 |  |
|        - |  9508 | `/*` |
|        - |  9509 | ` * Section:` |
|        - |  9510 | ` *   Array related routines.` |
|        - |  9511 | ` * Status:` |
|        - |  9512 | ` *    Stable.` |
|        - |  9513 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9514 | ` *  Array related functions that need access to the underlying` |
|        - |  9515 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9516 | ` */` |
|        - |  9517 | `/*` |
|        - |  9518 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9519 | ` * of the following structure.` |
|        - |  9520 | ` */` |
|        - |  9521 | `struct compact_data` |
|        - |  9522 |  |
|        - |  9523 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9524 | `	int nRecCount;      /* Recursion count */` |
|        - |  9525 | `};` |
|        - |  9526 | `/*` |
|        - |  9527 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9528 | ` */` |
|      ! 0 |  9529 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9530 |  |
|      ! 0 |  9531 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9532 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9533 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9534 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9535 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9536 | `		SyString sVar;` |
|      ! 0 |  9537 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9538 | `		if( sVar.nByte > 0 ){` |
|        - |  9539 | `			/* Query the current frame */` |
|      ! 0 |  9540 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9541 | `			/* ^` |
|        - |  9542 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9543 | `			 */` |
|      ! 0 |  9544 | `			if( pKey ){` |
|        - |  9545 | `				/* Perform the insertion */` |
|      ! 0 |  9546 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9547 | `			}` |
|      ! 0 |  9548 | `		}` |
|      ! 0 |  9549 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9550 | `		int rc;` |
|        - |  9551 | `		/* Recursively traverse this array */` |
|      ! 0 |  9552 | `		pData->nRecCount++;` |
|      ! 0 |  9553 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9554 | `		pData->nRecCount--;` |
|      ! 0 |  9555 | `		return rc;` |
|        - |  9556 | `	}` |
|      ! 0 |  9557 | `	return SXRET_OK;` |
|      ! 0 |  9558 |  |
|        - |  9559 | `/*` |
|        - |  9560 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9561 | ` *  Create array containing variables and their values.` |
|        - |  9562 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9563 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9564 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9565 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9566 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9567 | ` * Parameters` |
|        - |  9568 | ` *  $varname` |
|        - |  9569 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9570 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9571 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9572 | ` *   it recursively.` |
|        - |  9573 | ` * Return` |
|        - |  9574 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9575 | ` */` |
|        2 |  9576 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9577 |  |
|        - |  9578 | `	ph7_value *pArray,*pObj;` |
|        3 |  9579 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9580 | `	const char *zName;` |
|        - |  9581 | `	SyString sVar;` |
|        - |  9582 | `	int i,nLen;` |
|        3 |  9583 | `	if( nArg < 1 ){` |
|        - |  9584 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9585 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9586 | `		return PH7_OK;` |
|        - |  9587 | `	}` |
|        - |  9588 | `	/* Create the array */` |
|        3 |  9589 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9590 | `	if( pArray == 0 ){` |
|        - |  9591 | `		/* Out of memory */` |
|      ! 0 |  9592 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9593 | `		/* Return NULL */` |
|      ! 0 |  9594 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9595 | `		return PH7_OK;` |
|        - |  9596 | `	}` |
|        - |  9597 | `	/* Perform the requested operation */` |
|        7 |  9598 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9599 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9600 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9601 | `				struct compact_data sData;` |
|      ! 0 |  9602 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9603 | `				/* Recursively walk the array */` |
|      ! 0 |  9604 | `				sData.nRecCount = 0;` |
|      ! 0 |  9605 | `				sData.pArray = pArray;` |
|      ! 0 |  9606 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9607 | `			}` |
|      ! 0 |  9608 | `		}else{` |
|        - |  9609 | `			/* Extract variable name */` |
|        5 |  9610 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9611 | `			if( nLen > 0 ){` |
|        5 |  9612 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9613 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9614 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9615 | `				if( pObj ){` |
|        5 |  9616 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9617 | `				}` |
|        2 |  9618 | `			}` |
|        - |  9619 | `		}` |
|        3 |  9620 | `	}` |
|        - |  9621 | `	/* Return the array */` |
|        3 |  9622 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9623 | `	return PH7_OK;` |
|        2 |  9624 |  |
|        - |  9625 | `/*` |
|        - |  9626 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9627 | ` * of the following structure.` |
|        - |  9628 | ` */` |
|        - |  9629 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9630 | `struct extract_aux_data` |
|        - |  9631 |  |
|        - |  9632 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9633 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9634 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9635 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9636 | `	int iFlags;           /* Control flags */` |
|        - |  9637 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9638 | `};` |
|        - |  9639 | `/* Forward declaration */` |
|        - |  9640 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9641 | `/*` |
|        - |  9642 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9643 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9644 | ` * Parameters` |
|        - |  9645 | ` * $var_array` |
|        - |  9646 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9647 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9648 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9649 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9650 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9651 | ` * $extract_type` |
|        - |  9652 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9653 | ` *  It can be one of the following values:` |
|        - |  9654 | ` *   EXTR_OVERWRITE` |
|        - |  9655 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9656 | ` *   EXTR_SKIP` |
|        - |  9657 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9658 | ` *   EXTR_PREFIX_SAME` |
|        - |  9659 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9660 | ` *   EXTR_PREFIX_ALL` |
|        - |  9661 | ` *       Prefix all variable names with prefix.` |
|        - |  9662 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9663 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9664 | ` *   EXTR_IF_EXISTS` |
|        - |  9665 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9666 | ` *       otherwise do nothing.` |
|        - |  9667 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9668 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9669 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9670 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9671 | ` *      the current symbol table.` |
|        - |  9672 | ` * $prefix` |
|        - |  9673 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9674 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9675 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9676 | ` *  underscore character.` |
|        - |  9677 | ` * Return` |
|        - |  9678 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9679 | ` */` |
|        4 |  9680 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9681 |  |
|        - |  9682 | `	extract_aux_data sAux;` |
|        - |  9683 | `	ph7_hashmap *pMap;` |
|        5 |  9684 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9685 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9686 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9687 | `		return PH7_OK;` |
|        - |  9688 | `	}` |
|        - |  9689 | `	/* Point to the target hashmap */` |
|        5 |  9690 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9691 | `	if( pMap->nEntry < 1 ){` |
|        - |  9692 | `		/* Empty map,return  0 */` |
|      ! 0 |  9693 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9694 | `		return PH7_OK;` |
|        - |  9695 | `	}` |
|        - |  9696 | `	/* Prepare the aux data */` |
|        5 |  9697 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9698 | `	if( nArg > 1 ){` |
|        3 |  9699 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9700 | `		if( nArg > 2 ){` |
|      ! 0 |  9701 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9702 | `		}` |
|        1 |  9703 | `	}` |
|        5 |  9704 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9705 | `	/* Invoke the worker callback */` |
|        5 |  9706 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9707 | `	/* Number of variables successfully imported */` |
|        5 |  9708 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9709 | `	return PH7_OK;` |
|        3 |  9710 |  |
|        - |  9711 | `/*` |
|        - |  9712 | ` * Worker callback for the [extract()] function defined` |
|        - |  9713 | ` * below.` |
|        - |  9714 | ` */` |
|        8 |  9715 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9716 |  |
|        9 |  9717 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9718 | `	int iFlags = pAux->iFlags;` |
|        9 |  9719 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9720 | `	ph7_value *pObj;` |
|        - |  9721 | `	SyString sVar;` |
|        9 |  9722 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9723 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9724 | `	}` |
|        - |  9725 | `	/* Perform a string cast */` |
|        9 |  9726 | `	PH7_MemObjToString(pKey);` |
|        9 |  9727 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9728 | `		/* Unavailable variable name */` |
|      ! 0 |  9729 | `		return SXRET_OK;` |
|        - |  9730 | `	}` |
|        9 |  9731 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9732 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9733 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9734 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9735 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9736 | `			);` |
|      ! 0 |  9737 | `	}else{` |
|       13 |  9738 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9739 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9740 | `	}` |
|        9 |  9741 | `	sVar.zString = pAux->zWorker;` |
|        - |  9742 | `	/* Try to extract the variable */` |
|        9 |  9743 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9744 | `	if( pObj ){` |
|        - |  9745 | `		/* Collision */` |
|        5 |  9746 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9747 | `			return SXRET_OK;` |
|        - |  9748 | `		}` |
|        5 |  9749 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9750 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9751 | `				/* Already prefixed */` |
|      ! 0 |  9752 | `				return SXRET_OK;` |
|        - |  9753 | `			}` |
|      ! 0 |  9754 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9755 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9756 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9757 | `				);` |
|      ! 0 |  9758 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9759 | `		}` |
|        3 |  9760 | `	}else{` |
|        - |  9761 | `		/* Create the variable */` |
|        5 |  9762 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9763 | `	}` |
|        9 |  9764 | `	if( pObj ){` |
|        - |  9765 | `		/* Overwrite the old value */` |
|        9 |  9766 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9767 | `		/* Increment counter */` |
|        9 |  9768 | `		pAux->iCount++;` |
|        4 |  9769 | `	}` |
|        9 |  9770 | `	return SXRET_OK;` |
|        5 |  9771 |  |
|        - |  9772 | `/*` |
|        - |  9773 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9774 | ` * defined below.` |
|        - |  9775 | ` */` |
|        2 |  9776 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9777 |  |
|        3 |  9778 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9779 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9780 | `	ph7_value *pObj;` |
|        - |  9781 | `	SyString sVar;` |
|        - |  9782 | `	/* Perform a string cast */` |
|        3 |  9783 | `	PH7_MemObjToString(pKey);` |
|        3 |  9784 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9785 | `		/* Unavailable variable name */` |
|      ! 0 |  9786 | `		return SXRET_OK;` |
|        - |  9787 | `	}` |
|        3 |  9788 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9789 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9790 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9791 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9792 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9793 | `			);` |
|        2 |  9794 | `	}else{` |
|      ! 0 |  9795 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9796 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9797 | `	}` |
|        3 |  9798 | `	sVar.zString = pAux->zWorker;` |
|        - |  9799 | `	/* Extract the variable */` |
|        3 |  9800 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9801 | `	if( pObj ){` |
|        3 |  9802 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9803 | `	}` |
|        3 |  9804 | `	return SXRET_OK;` |
|        2 |  9805 |  |
|        - |  9806 | `/*` |
|        - |  9807 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9808 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9809 | ` * Parameters` |
|        - |  9810 | ` * $types` |
|        - |  9811 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9812 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9813 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9814 | ` *  POST includes the POST uploaded file information.` |
|        - |  9815 | ` *  Note:` |
|        - |  9816 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9817 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9818 | ` * $prefix` |
|        - |  9819 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9820 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9821 | ` *  variable named $pref_userid.` |
|        - |  9822 | ` * Return` |
|        - |  9823 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9824 | ` */` |
|        2 |  9825 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9826 |  |
|        - |  9827 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9828 | `	extract_aux_data sAux;` |
|        - |  9829 | `	int nLen,nPrefixLen;` |
|        - |  9830 | `	ph7_value *pSuper;` |
|        - |  9831 | `	ph7_vm *pVm;` |
|        - |  9832 | `	/* By default import only $_GET variables  */` |
|        3 |  9833 | `	zImport = "G";` |
|        3 |  9834 | `	nLen = (int)sizeof(char);` |
|        3 |  9835 | `	zPrefix = 0;` |
|        3 |  9836 | `	nPrefixLen = 0;` |
|        3 |  9837 | `	if( nArg > 0 ){` |
|        3 |  9838 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9839 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9840 | `		}` |
|        3 |  9841 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9842 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9843 | `		}` |
|        1 |  9844 | `	}` |
|        - |  9845 | `	/* Point to the underlying VM */` |
|        3 |  9846 | `	pVm = pCtx->pVm;` |
|        - |  9847 | `	/* Initialize the aux data */` |
|        3 |  9848 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9849 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9850 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9851 | `	sAux.pVm = pVm;` |
|        - |  9852 | `	/* Extract */` |
|        3 |  9853 | `	zEnd = &zImport[nLen];` |
|        5 |  9854 | `	while( zImport < zEnd ){` |
|        3 |  9855 | `		int c = zImport[0];` |
|        3 |  9856 | `		pSuper = 0;` |
|        3 |  9857 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9858 | `			/* Import $_GET variables */` |
|        3 |  9859 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9860 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9861 | `			/* Import $_POST variables */` |
|      ! 0 |  9862 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9863 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9864 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9865 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9866 | `		}` |
|        3 |  9867 | `		if( pSuper ){` |
|        - |  9868 | `			/* Iterate throw array entries */` |
|        3 |  9869 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9870 | `		}` |
|        - |  9871 | `		/* Advance the cursor */` |
|        3 |  9872 | `		zImport++;` |
|        1 |  9873 | `	}` |
|        - |  9874 | `	/* All done,return TRUE*/` |
|        3 |  9875 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9876 | `	return PH7_OK;` |
|        1 |  9877 |  |
|        - |  9878 | `/*` |
|        - |  9879 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9880 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9881 | ` * information.` |
|        - |  9882 | ` */` |
|    10200 |  9883 | `static sxi32 VmEvalChunk(` |
|        - |  9884 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9885 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9886 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9887 | `	int iFlags,         /* Compile flag */` |
|        - |  9888 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9889 | `	)` |
|        2 |  9890 |  |
|        - |  9891 | `	SySet *pByteCode,aByteCode;` |
|        - |  9892 | `	SyBlob sSavedNs;` |
|    10202 |  9893 | `	ProcConsumer xErr = 0;` |
|    10202 |  9894 | `	void *pErrData = 0;` |
|        - |  9895 | `	/* Initialize bytecode container */` |
|    10202 |  9896 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10202 |  9897 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9898 | `	/* Reset the code generator */` |
|    10202 |  9899 | `	if( bTrueReturn ){` |
|        - |  9900 | `		/* Included file,log compile-time errors */` |
|     7637 |  9901 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 |  9902 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 |  9903 | `	}` |
|    10202 |  9904 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9905 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9906 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9907 | `	 * the caller's namespace is restored. */` |
|    10202 |  9908 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10202 |  9909 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10202 |  9910 | `	if( bTrueReturn ){` |
|        - |  9911 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 |  9912 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 |  9913 | `	}` |
|        - |  9914 | `	/* Swap bytecode container */` |
|    10202 |  9915 | `	pByteCode = pVm->pByteContainer;` |
|    10202 |  9916 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9917 | `	/* Compile the chunk */` |
|    10202 |  9918 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15302 |  9919 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9920 | `		/* Compilation error,return false */` |
|        3 |  9921 | `		if( pCtx ){` |
|        3 |  9922 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9923 | `		}` |
|        2 |  9924 | `	}else{` |
|        - |  9925 | `		/* Mount any newly defined classes */` |
|        - |  9926 | `		SyHashEntry *pEntry;` |
|        - |  9927 | `		ph7_class *pClass;` |
|        - |  9928 | `		ph7_value sResult; /* Return value */` |
|        - |  9929 | `		sxi32 rc;` |
|    10200 |  9930 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   281497 |  9931 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   266200 |  9932 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9933 | `			/* Only mount classes that haven't been mounted yet */` |
|   266200 |  9934 | `			if( !pClass->bMounted ){` |
|    63208 |  9935 | `				rc = VmMountUserClass(pVm,pClass);` |
|    63208 |  9936 | `				if( rc != SXRET_OK ){` |
|        - |  9937 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9938 | `					if( pCtx ){` |
|      ! 0 |  9939 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9940 | `					}` |
|      ! 0 |  9941 | `					goto Cleanup;` |
|        - |  9942 | `				}` |
|    31603 |  9943 | `			}` |
|        2 |  9944 | `		}` |
|    10200 |  9945 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9946 | `			/* Out of memory */` |
|      ! 0 |  9947 | `			if( pCtx ){` |
|      ! 0 |  9948 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9949 | `			}` |
|      ! 0 |  9950 | `			goto Cleanup;` |
|        - |  9951 | `		}` |
|    10200 |  9952 | `		if( bTrueReturn ){` |
|        - |  9953 | `			/* Assume a boolean true return value */` |
|     7637 |  9954 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 |  9955 | `		}else{` |
|        - |  9956 | `			/* Assume a null return value */` |
|     2564 |  9957 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9958 | `		}` |
|        - |  9959 | `		/* Execute the compiled chunk */` |
|    10200 |  9960 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10200 |  9961 | `		if( pCtx ){` |
|        - |  9962 | `			/* Set the execution result */` |
|     7650 |  9963 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 |  9964 | `		}` |
|    10200 |  9965 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9966 | `	}` |
|     5100 |  9967 | `Cleanup:` |
|        - |  9968 | `	/* Cleanup the mess left behind */` |
|    10202 |  9969 | `	pVm->pByteContainer = pByteCode;` |
|    10202 |  9970 | `	SySetRelease(&aByteCode);` |
|        - |  9971 | `	/* Restore caller's namespace state */` |
|    10202 |  9972 | `	SyBlobReset(&pVm->sNamespace);` |
|    10202 |  9973 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10202 |  9974 | `	SyBlobRelease(&sSavedNs);` |
|    10202 |  9975 | `	return SXRET_OK;` |
|        2 |  9976 |  |
|        - |  9977 | `/*` |
|        - |  9978 | ` * value eval(string $code)` |
|        - |  9979 | ` *   Evaluate a string as PHP code.` |
|        - |  9980 | ` * Parameter` |
|        - |  9981 | ` *  code: PHP code to evaluate.` |
|        - |  9982 | ` * Return` |
|        - |  9983 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9984 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9985 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9986 | ` */` |
|       16 |  9987 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9988 |  |
|        - |  9989 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9990 | `	if( nArg < 1 ){` |
|        - |  9991 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9992 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9993 | `		return SXRET_OK;` |
|        - |  9994 | `	}` |
|        - |  9995 | `	/* Chunk to evaluate */` |
|       18 |  9996 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9997 | `	if( sChunk.nByte < 1 ){` |
|        - |  9998 | `		/* Empty string,return NULL */` |
|        3 |  9999 | `		ph7_result_null(pCtx);` |
|        3 | 10000 | `		return SXRET_OK;` |
|        - | 10001 | `	}` |
|        - | 10002 | `	/* Eval the chunk */` |
|       16 | 10003 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10004 | `	return SXRET_OK;` |
|       10 | 10005 |  |
|        - | 10006 | `/*` |
|        - | 10007 | ` * Check if a file path is already included.` |
|        - | 10008 | ` */` |
|    15268 | 10009 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10010 |  |
|        - | 10011 | `	SyString *aEntries;` |
|        - | 10012 | `	sxu32 n;` |
|    15269 | 10013 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10014 | `	/* Perform a linear search */` |
| 58267061 | 10015 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 10016 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10017 | `			/* Already included */` |
|        7 | 10018 | `			return TRUE;` |
|        - | 10019 | `		}` |
| 29125897 | 10020 | `	}` |
|    15263 | 10021 | `	return FALSE;` |
|     7635 | 10022 |  |
|        - | 10023 | `/*` |
|        - | 10024 | ` * Push a file path in the appropriate VM container.` |
|        - | 10025 | ` */` |
|    17810 | 10026 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10027 |  |
|        - | 10028 | `	SyString sPath;` |
|        - | 10029 | `	char *zDup;` |
|        - | 10030 | `#ifdef __WINNT__` |
|        - | 10031 | `	char *zCur;` |
|        - | 10032 | `#endif` |
|        - | 10033 | `	sxi32 rc;` |
|    17812 | 10034 | `	if( nLen < 0 ){` |
|     2544 | 10035 | `		nLen = SyStrlen(zPath);` |
|     1271 | 10036 | `	}` |
|        - | 10037 | `	/* Duplicate the file path first */` |
|    17812 | 10038 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17812 | 10039 | `	if( zDup == 0 ){` |
|      ! 0 | 10040 | `		return SXERR_MEM;` |
|        - | 10041 | `	}` |
|        - | 10042 | `#ifdef __WINNT__` |
|        - | 10043 | `	/* Normalize path on windows` |
|        - | 10044 | `	 * Example:` |
|        - | 10045 | `	 *    Path/To/File.php` |
|        - | 10046 | `	 * becomes` |
|        - | 10047 | `	 *   path\to\file.php` |
|        - | 10048 | `	 */` |
|        2 | 10049 | `	zCur = zDup;` |
|        2 | 10050 | `	while( zCur[0] != 0 ){` |
|        2 | 10051 | `		if( zCur[0] == '/' ){` |
|        2 | 10052 | `			zCur[0] = '\\';` |
|        2 | 10053 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10054 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10055 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10056 | `		}` |
|        2 | 10057 | `		zCur++;` |
|        2 | 10058 | `	}` |
|        - | 10059 | `#endif` |
|        - | 10060 | `	/* Install the file path */` |
|    17812 | 10061 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17812 | 10062 | `	if( !bMain ){` |
|    15269 | 10063 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10064 | `			/* Already included */` |
|        7 | 10065 | `			*pNew = 0;` |
|        4 | 10066 | `		}else{` |
|        - | 10067 | `			/* Insert in the corresponding container */` |
|    15263 | 10068 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 10069 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10070 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10071 | `				return rc;` |
|        - | 10072 | `			}` |
|    15263 | 10073 | `			*pNew = 1;` |
|        - | 10074 | `		}` |
|     7634 | 10075 | `	}` |
|    17812 | 10076 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17812 | 10077 | `	return SXRET_OK;` |
|     8907 | 10078 |  |
|        - | 10079 | `/*` |
|        - | 10080 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10081 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10082 | ` * indicates failure.` |
|        - | 10083 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10084 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10085 | ` * operations.` |
|        - | 10086 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10087 | ` * this function is a no-op.` |
|        - | 10088 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10089 | ` * constructs for more information.` |
|        - | 10090 | ` */` |
|     7642 | 10091 | `static sxi32 VmExecIncludedFile(` |
|        - | 10092 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10093 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10094 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10095 | `	 )` |
|        2 | 10096 |  |
|        - | 10097 | `	sxi32 rc;` |
|        - | 10098 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10099 | `	const ph7_io_stream *pStream;` |
|        - | 10100 | `	SyBlob sContents;` |
|        - | 10101 | `	void *pHandle;` |
|        - | 10102 | `	ph7_vm *pVm;` |
|        - | 10103 | `	int isNew;` |
|        - | 10104 | `	/* Initialize fields */` |
|     7644 | 10105 | `	pVm = pCtx->pVm;` |
|     7644 | 10106 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 10107 | `	isNew = 0;` |
|        - | 10108 | `	/* Extract the associated stream */` |
|     7644 | 10109 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10110 | `	/*` |
|        - | 10111 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10112 | `	 * in a read-only mode.` |
|        - | 10113 | `	 */` |
|     7644 | 10114 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 10115 | `	if( pHandle == 0 ){` |
|        3 | 10116 | `		return SXERR_IO;` |
|        - | 10117 | `	}` |
|     7641 | 10118 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 10119 | `	if( IncludeOnce && !isNew ){` |
|        - | 10120 | `		/* Already included */` |
|        5 | 10121 | `		rc = SXERR_EXISTS;` |
|        3 | 10122 | `	}else{` |
|        - | 10123 | `		/* Read the whole file contents */` |
|     7637 | 10124 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 10125 | `		if( rc == SXRET_OK ){` |
|        - | 10126 | `			SyString sScript;` |
|        - | 10127 | `			/* Compile and execute the script */` |
|     7637 | 10128 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 10129 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 10130 | `		}` |
|        - | 10131 | `	}` |
|        - | 10132 | `	/* Pop from the set of included file */` |
|     7641 | 10133 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10134 | `	/* Close the handle */` |
|     7641 | 10135 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10136 | `	/* Release the working buffer */` |
|     7641 | 10137 | `	SyBlobRelease(&sContents);` |
|        - | 10138 | `#else` |
|        - | 10139 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10140 | `	SXUNUSED(pPath);` |
|        - | 10141 | `	SXUNUSED(IncludeOnce);` |
|        - | 10142 | `	rc = SXERR_IO;` |
|        - | 10143 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 10144 | `	return rc;` |
|     3823 | 10145 |  |
|        - | 10146 | `/*` |
|        - | 10147 | ` * string get_include_path(void)` |
|        - | 10148 | ` *  Gets the current include_path configuration option.` |
|        - | 10149 | ` * Parameter` |
|        - | 10150 | ` *  None` |
|        - | 10151 | ` * Return` |
|        - | 10152 | ` *  Included paths as a string` |
|        - | 10153 | ` */` |
|        2 | 10154 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10155 |  |
|        3 | 10156 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10157 | `	SyString *aEntry;` |
|        - | 10158 | `	int dir_sep;` |
|        - | 10159 | `	sxu32 n;` |
|        - | 10160 | `#ifdef __WINNT__` |
|        1 | 10161 | `	dir_sep = ';';` |
|        - | 10162 | `#else` |
|        - | 10163 | `	/* Assume UNIX path separator */` |
|        2 | 10164 | `	dir_sep = ':';` |
|        - | 10165 | `#endif` |
|        1 | 10166 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10167 | `	SXUNUSED(apArg);` |
|        - | 10168 | `	/* Point to the list of import paths */` |
|        3 | 10169 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10170 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10171 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10172 | `		if( n > 0 ){` |
|        - | 10173 | `			/* Append dir seprator */` |
|      ! 0 | 10174 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10175 | `		}` |
|        - | 10176 | `		/* Append path */` |
|        3 | 10177 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10178 | `	}` |
|        3 | 10179 | `	return PH7_OK;` |
|        1 | 10180 |  |
|        - | 10181 | `/*` |
|        - | 10182 | ` * string get_get_included_files(void)` |
|        - | 10183 | ` *  Gets the current include_path configuration option.` |
|        - | 10184 | ` * Parameter` |
|        - | 10185 | ` *  None` |
|        - | 10186 | ` * Return` |
|        - | 10187 | ` *  Included paths as a string` |
|        - | 10188 | ` */` |
|        2 | 10189 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10190 |  |
|        3 | 10191 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10192 | `	ph7_value *pArray,*pWorker;` |
|        - | 10193 | `	SyString *pEntry;` |
|        - | 10194 | `	int c,d;` |
|        - | 10195 | `	/* Create an array and a working value */` |
|        3 | 10196 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10197 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10198 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10199 | `		/* Out of memory,return null */` |
|      ! 0 | 10200 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10201 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10202 | `		SXUNUSED(apArg);` |
|      ! 0 | 10203 | `		return PH7_OK;` |
|        - | 10204 | `	}` |
|        3 | 10205 | `	c = d = '/';` |
|        - | 10206 | `#ifdef __WINNT__` |
|        1 | 10207 | `	d = '\\';` |
|        - | 10208 | `#endif` |
|        - | 10209 | `	/* Iterate throw entries */` |
|        3 | 10210 | `	SySetResetCursor(pFiles);` |
|     3689 | 10211 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10212 | `		const char *zBase,*zEnd;` |
|        - | 10213 | `		int iLen;` |
|        - | 10214 | `		/* reset the string cursor */` |
|     3687 | 10215 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10216 | `		/* Extract base name */` |
|     3687 | 10217 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10218 | `		/* Ignore trailing '/' */` |
|     5530 | 10219 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10220 | `			zEnd--;` |
|      ! 0 | 10221 | `		}` |
|     3687 | 10222 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 10223 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 10224 | `			zEnd--;` |
|        1 | 10225 | `		}` |
|     3687 | 10226 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 10227 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10228 | `		/* Copy entry name */` |
|     3687 | 10229 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10230 | `		/* Perform the insertion */` |
|     3687 | 10231 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10232 | `	}` |
|        - | 10233 | `	/* All done,return the created array */` |
|        3 | 10234 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10235 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10236 | `	 * by the engine as soon we return from this foreign` |
|        - | 10237 | `	 * function.` |
|        - | 10238 | `	 */` |
|        3 | 10239 | `	return PH7_OK;` |
|        2 | 10240 |  |
|        - | 10241 | `/*` |
|        - | 10242 | ` * include:` |
|        - | 10243 | ` * According to the PHP reference manual.` |
|        - | 10244 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10245 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10246 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10247 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10248 | ` *  and the current working directory before failing. The include()` |
|        - | 10249 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10250 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10251 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10252 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10253 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10254 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10255 | ` *  directory to find the requested file.` |
|        - | 10256 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10257 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10258 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10259 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10260 | ` */` |
|     7630 | 10261 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10262 |  |
|        - | 10263 | `	SyString sFile;` |
|        - | 10264 | `	sxi32 rc;` |
|     7632 | 10265 | `	if( nArg < 1 ){` |
|        - | 10266 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10267 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10268 | `		return SXRET_OK;` |
|        - | 10269 | `	}` |
|        - | 10270 | `	/* File to include */` |
|     7632 | 10271 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 10272 | `	if( sFile.nByte < 1 ){` |
|        - | 10273 | `		/* Empty string,return NULL */` |
|      ! 0 | 10274 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10275 | `		return SXRET_OK;` |
|        - | 10276 | `	}` |
|        - | 10277 | `	/* Open,compile and execute the desired script */` |
|     7632 | 10278 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 10279 | `	if( rc != SXRET_OK ){` |
|        - | 10280 | `		/* Emit a warning and return false */` |
|        3 | 10281 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10282 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10283 | `	}` |
|     7632 | 10284 | `	return SXRET_OK;` |
|     3817 | 10285 |  |
|        - | 10286 | `/*` |
|        - | 10287 | ` * include_once:` |
|        - | 10288 | ` *  According to the PHP reference manual.` |
|        - | 10289 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10290 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10291 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10292 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10293 | ` *   just once.` |
|        - | 10294 | ` */` |
|        4 | 10295 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10296 |  |
|        - | 10297 | `	SyString sFile;` |
|        - | 10298 | `	sxi32 rc;` |
|        5 | 10299 | `	if( nArg < 1 ){` |
|        - | 10300 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10301 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10302 | `		return SXRET_OK;` |
|        - | 10303 | `	}` |
|        - | 10304 | `	/* File to include */` |
|        5 | 10305 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10306 | `	if( sFile.nByte < 1 ){` |
|        - | 10307 | `		/* Empty string,return NULL */` |
|      ! 0 | 10308 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10309 | `		return SXRET_OK;` |
|        - | 10310 | `	}` |
|        - | 10311 | `	/* Open,compile and execute the desired script */` |
|        5 | 10312 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10313 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10314 | `		/* File already included,return TRUE */` |
|        3 | 10315 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10316 | `		return SXRET_OK;` |
|        - | 10317 | `	}` |
|        3 | 10318 | `	if( rc != SXRET_OK ){` |
|        - | 10319 | `		/* Emit a warning and return false */` |
|      ! 0 | 10320 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10321 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10322 | ` 	}` |
|        3 | 10323 | `	return SXRET_OK;` |
|        3 | 10324 |  |
|        - | 10325 | `/*` |
|        - | 10326 | ` * require.` |
|        - | 10327 | ` *  According to the PHP reference manual.` |
|        - | 10328 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10329 | ` *   also produce a fatal level error.` |
|        - | 10330 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10331 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10332 | ` */` |
|        4 | 10333 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10334 |  |
|        - | 10335 | `	SyString sFile;` |
|        - | 10336 | `	sxi32 rc;` |
|        5 | 10337 | `	if( nArg < 1 ){` |
|        - | 10338 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10339 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10340 | `		return SXRET_OK;` |
|        - | 10341 | `	}` |
|        - | 10342 | `	/* File to include */` |
|        5 | 10343 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10344 | `	if( sFile.nByte < 1 ){` |
|        - | 10345 | `		/* Empty string,return NULL */` |
|      ! 0 | 10346 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10347 | `		return SXRET_OK;` |
|        - | 10348 | `	}` |
|        - | 10349 | `	/* Open,compile and execute the desired script */` |
|        5 | 10350 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10351 | `	if( rc != SXRET_OK ){` |
|        - | 10352 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10353 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10354 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10355 | `		return PH7_ABORT;` |
|        - | 10356 | `	}` |
|        5 | 10357 | `	return SXRET_OK;` |
|        3 | 10358 |  |
|        - | 10359 | `/*` |
|        - | 10360 | ` * require_once:` |
|        - | 10361 | ` *  According to the PHP reference manual.` |
|        - | 10362 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10363 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10364 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10365 | ` *   and how it differs from its non _once siblings.` |
|        - | 10366 | ` */` |
|        4 | 10367 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10368 |  |
|        - | 10369 | `	SyString sFile;` |
|        - | 10370 | `	sxi32 rc;` |
|        5 | 10371 | `	if( nArg < 1 ){` |
|        - | 10372 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10373 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10374 | `		return SXRET_OK;` |
|        - | 10375 | `	}` |
|        - | 10376 | `	/* File to include */` |
|        5 | 10377 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10378 | `	if( sFile.nByte < 1 ){` |
|        - | 10379 | `		/* Empty string,return NULL */` |
|      ! 0 | 10380 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10381 | `		return SXRET_OK;` |
|        - | 10382 | `	}` |
|        - | 10383 | `	/* Open,compile and execute the desired script */` |
|        5 | 10384 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10385 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10386 | `		/* File already included,return TRUE */` |
|        3 | 10387 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10388 | `		return SXRET_OK;` |
|        - | 10389 | `	}` |
|        3 | 10390 | `	if( rc != SXRET_OK ){` |
|        - | 10391 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10392 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10393 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10394 | `		return PH7_ABORT;` |
|        - | 10395 | `	}` |
|        3 | 10396 | `	return SXRET_OK;` |
|        3 | 10397 |  |
|        - | 10398 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10399 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10400 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10401 | `/* Table of built-in VM functions. */` |
|        - | 10402 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10403 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10404 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10405 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10406 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10407 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10408 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10409 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10410 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10411 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10412 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10413 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10414 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10415 | `	    /* Constants management */` |
|        - | 10416 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10417 | `	{ "define",   vm_builtin_define               },` |
|        - | 10418 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10419 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10420 | `	   /* Class/Object functions */` |
|        - | 10421 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10422 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10423 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10424 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10425 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10426 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10427 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10428 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10429 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10430 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10431 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10432 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10433 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10434 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10435 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10436 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10437 | `	   /* Random numbers/strings generators */` |
|        - | 10438 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10439 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10440 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10441 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10442 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10443 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10444 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10445 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10446 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10447 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10448 | `	   /* Language constructs functions */` |
|        - | 10449 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10450 | `	{ "print", vm_builtin_print                   },` |
|        - | 10451 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10452 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10453 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10454 | `	  /* Variable handling functions */` |
|        - | 10455 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10456 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10457 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10458 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10459 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10460 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10461 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10462 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10463 | `	  /* Ouput control functions */` |
|        - | 10464 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10465 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10466 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10467 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10468 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10469 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10470 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10471 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10472 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10473 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10474 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10475 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10476 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10477 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10478 | `	  /* Assertion functions */` |
|        - | 10479 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10480 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10481 | `	  /* Error reporting functions */` |
|        - | 10482 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10483 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10484 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10485 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10486 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10487 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10488 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10489 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10490 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10491 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10492 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10493 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10494 | `	  /* Release info */` |
|        - | 10495 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10496 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10497 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10498 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10499 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10500 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10501 | `	  /* hashmap */` |
|        - | 10502 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10503 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10504 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10505 | `	  /* URL related function */` |
|        - | 10506 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10507 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10508 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10509 | `	   /* XML processing functions */` |
|        - | 10510 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10511 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10512 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10513 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10514 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10515 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10516 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10517 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10518 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10519 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10520 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10521 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10522 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10523 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10524 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10525 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10526 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10527 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10528 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10529 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10530 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10531 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10532 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10533 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10534 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10535 | `	   /* Command line processing */` |
|        - | 10536 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10537 | `	   /* JSON encoding/decoding */` |
|        - | 10538 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10539 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10540 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10541 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10542 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10543 | `	   /* Files/URI inclusion facility */` |
|        - | 10544 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10545 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10546 | `	{ "include",      vm_builtin_include          },` |
|        - | 10547 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10548 | `	{ "require",      vm_builtin_require          },` |
|        - | 10549 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10550 | `};` |
|        - | 10551 | `/*` |
|        - | 10552 | ` * Register the built-in VM functions defined above.` |
|        - | 10553 | ` */` |
|     2288 | 10554 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10555 |  |
|        - | 10556 | `	sxi32 rc;` |
|        - | 10557 | `	sxu32 n;` |
|   286002 | 10558 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10559 | `		/* Note that these special functions have access` |
|        - | 10560 | `		 * to the underlying virtual machine as their` |
|        - | 10561 | `		 * private data.` |
|        - | 10562 | `		 */` |
|   283714 | 10563 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   283714 | 10564 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10565 | `			return rc;` |
|        - | 10566 | `		}` |
|   141858 | 10567 | `	}` |
|     2290 | 10568 | `	return SXRET_OK;` |
|     1146 | 10569 |  |
|        - | 10570 | `/*` |
|        - | 10571 | ` * Check if the given name refer to an installed class.` |
|        - | 10572 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10573 | ` */` |
|    16740 | 10574 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10575 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10576 | `	const char *zName,  /* Name of the target class */` |
|        - | 10577 | `	sxu32 nByte,        /* zName length */` |
|        - | 10578 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10579 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10580 | `						 */` |
|        - | 10581 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10582 | `	)` |
|        2 | 10583 |  |
|        - | 10584 | `	SyHashEntry *pEntry;` |
|        - | 10585 | `	ph7_class *pClass;` |
|     8370 | 10586 | `	SXUNUSED(iNest);` |
|        - | 10587 | `	/* Exact class lookup.` |
|        - | 10588 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10589 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    16742 | 10590 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    16742 | 10591 | `	if( pEntry == 0 ){` |
|       10 | 10592 | `		return 0;` |
|        - | 10593 | `	}` |
|    16734 | 10594 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    16734 | 10595 | `	if( !iLoadable ){` |
|    15592 | 10596 | `		return pClass;` |
|        - | 10597 | `	}` |
|        - | 10598 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1144 | 10599 | `	while(pClass){` |
|     1144 | 10600 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1144 | 10601 | `			return pClass;` |
|        - | 10602 | `		}` |
|      ! 0 | 10603 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10604 | `	}` |
|      ! 0 | 10605 | `	return 0;` |
|     8372 | 10606 |  |
|        - | 10607 | `/*` |
|        - | 10608 | ` * Reference Table Implementation` |
|        - | 10609 | ` * Status: stable <chm@symisc.net>` |
|        - | 10610 | ` * Intro` |
|        - | 10611 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10612 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10613 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10614 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10615 | ` *  Refer to the official for more information on this powerful` |
|        - | 10616 | ` *  extension.` |
|        - | 10617 | ` */` |
|        - | 10618 | `/*` |
|        - | 10619 | ` * Allocate a new reference entry.` |
|        - | 10620 | ` */` |
|  3000800 | 10621 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10622 |  |
|        - | 10623 | `	VmRefObj *pRef;` |
|        - | 10624 | `	/* Allocate a new instance */` |
|  3000802 | 10625 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3000802 | 10626 | `	if( pRef == 0 ){` |
|      ! 0 | 10627 | `		return 0;` |
|        - | 10628 | `	}` |
|        - | 10629 | `	/* Zero the structure */` |
|  3000802 | 10630 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10631 | `	/* Initialize fields */` |
|  3000802 | 10632 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3000802 | 10633 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3000802 | 10634 | `	pRef->nIdx = nIdx;` |
|  3000802 | 10635 | `	return pRef;` |
|  1500402 | 10636 |  |
|        - | 10637 | `/*` |
|        - | 10638 | ` * Default hash function used by the reference table` |
|        - | 10639 | ` * for lookup/insertion operations.` |
|        - | 10640 | ` */` |
| 16644913 | 10641 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10642 |  |
|        - | 10643 | `	/* Calculate the hash based on the memory object index */` |
| 16644915 | 10644 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10645 |  |
|        - | 10646 | `/*` |
|        - | 10647 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10648 | ` * in the reference table.` |
|        - | 10649 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10650 | ` * otherwise.` |
|        - | 10651 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10652 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10653 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10654 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10655 | ` * Refer to the official for more information on this powerful` |
|        - | 10656 | ` * extension.` |
|        - | 10657 | ` */` |
|  8957196 | 10658 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10659 |  |
|        - | 10660 | `	VmRefObj *pRef;` |
|        - | 10661 | `	sxu32 nBucket;` |
|        - | 10662 | `	/* Point to the appropriate bucket */` |
|  8957198 | 10663 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10664 | `	/* Perform the lookup */` |
|  8957198 | 10665 | `	pRef = pVm->apRefObj[nBucket];` |
| 19261996 | 10666 | `	for(;;){` |
| 38512678 | 10667 | `		if( pRef == 0 ){` |
|  3077432 | 10668 | `			break;` |
|        - | 10669 | `		}` |
| 35435248 | 10670 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10671 | `			/* Entry found */` |
|  5879768 | 10672 | `			return pRef;` |
|        - | 10673 | `		}` |
|        - | 10674 | `		/* Point to the next entry */` |
| 29555482 | 10675 | `		pRef = pRef->pNextCollide;` |
|        2 | 10676 | `	}` |
|        - | 10677 | `	/* No such entry,return NULL */` |
|  3077432 | 10678 | `	return 0;` |
|  4478600 | 10679 |  |
|        - | 10680 | `/*` |
|        - | 10681 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10682 | ` *` |
|        - | 10683 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10684 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10685 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10686 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10687 | ` * Refer to the official for more information on this powerful` |
|        - | 10688 | ` * extension.` |
|        - | 10689 | ` */` |
|  3000800 | 10690 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10691 |  |
|        - | 10692 | `	sxu32 nBucket;` |
|  3000802 | 10693 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10694 | `		VmRefObj **apNew;` |
|        - | 10695 | `		sxu32 nNew;` |
|        - | 10696 | `		/* Allocate a larger table */` |
|     3906 | 10697 | `		nNew = pVm->nRefSize << 1;` |
|     3906 | 10698 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3906 | 10699 | `		if( apNew ){` |
|     3906 | 10700 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10701 | `			sxu32 n;` |
|        - | 10702 | `			/* Zero the structure */` |
|     3906 | 10703 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10704 | `			/* Rehash all referenced entries */` |
|  2839788 | 10705 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10706 | `				/* Remove old collision links */` |
|  2835884 | 10707 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10708 | `				/* Point to the appropriate bucket */` |
|  2835884 | 10709 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10710 | `				/* Insert the entry  */` |
|  2835884 | 10711 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2835884 | 10712 | `				if( apNew[nBucket] ){` |
|  2298896 | 10713 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10714 | `				}` |
|  2835884 | 10715 | `				apNew[nBucket] = pEntry;` |
|        - | 10716 | `				/* Point to the next entry */` |
|  2835884 | 10717 | `				pEntry = pEntry->pNext;` |
|  1417943 | 10718 | `			}` |
|        - | 10719 | `			/* Release the old table */` |
|     3906 | 10720 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10721 | `			/* Install the new one */` |
|     3906 | 10722 | `			pVm->apRefObj = apNew;` |
|     3906 | 10723 | `			pVm->nRefSize = nNew;` |
|     1952 | 10724 | `		}` |
|     1952 | 10725 | `	}` |
|        - | 10726 | `	/* Point to the appropriate bucket */` |
|  3000802 | 10727 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10728 | `	/* Insert the entry */` |
|  3000802 | 10729 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3000802 | 10730 | `	if( pVm->apRefObj[nBucket] ){` |
|  2486704 | 10731 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1243448 | 10732 | `	}` |
|  3000802 | 10733 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3000802 | 10734 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3000802 | 10735 | `	pVm->nRefUsed++;` |
|  3000802 | 10736 | `	return SXRET_OK;` |
|        2 | 10737 |  |
|        - | 10738 | `/*` |
|        - | 10739 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10740 | ` * the reference table.` |
|        - | 10741 | ` * This function is invoked when the user perform an unset` |
|        - | 10742 | ` * call [i.e: unset($var); ].` |
|        - | 10743 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10744 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10745 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10746 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10747 | ` * Refer to the official for more information on this powerful` |
|        - | 10748 | ` * extension.` |
|        - | 10749 | ` */` |
|  2968284 | 10750 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10751 |  |
|        - | 10752 | `	ph7_hashmap_node **apNode;` |
|        - | 10753 | `	SyHashEntry **apEntry;` |
|        - | 10754 | `	sxu32 n;` |
|        - | 10755 | `	/* Point to the reference table */` |
|  2968286 | 10756 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2968286 | 10757 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10758 | `	/* Unlink the entry from the reference table */` |
|  3050622 | 10759 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    82338 | 10760 | `		if( apEntry[n] ){` |
|    82288 | 10761 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    41143 | 10762 | `		}` |
|    41170 | 10763 | `	}` |
|  5856802 | 10764 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2888518 | 10765 | `		if( apNode[n] ){` |
|     6634 | 10766 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3316 | 10767 | `		}` |
|  1444260 | 10768 | `	}` |
|  2968286 | 10769 | `	if( pRef->pPrevCollide ){` |
|  1117251 | 10770 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   559018 | 10771 | `	}else{` |
|  1851037 | 10772 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10773 | `	}` |
|  2968286 | 10774 | `	if( pRef->pNextCollide ){` |
|  1676716 | 10775 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   838393 | 10776 | `	}` |
|  2968286 | 10777 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10778 | `	/* Release the node */` |
|  2968286 | 10779 | `	SySetRelease(&pRef->aReference);` |
|  2968286 | 10780 | `	SySetRelease(&pRef->aArrEntries);` |
|  2968286 | 10781 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2968286 | 10782 | `	pVm->nRefUsed--;` |
|  2968286 | 10783 | `	return SXRET_OK;` |
|        2 | 10784 |  |
|        - | 10785 | `/*` |
|        - | 10786 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10787 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10788 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10789 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10790 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10791 | ` * Refer to the official for more information on this powerful` |
|        - | 10792 | ` * extension.` |
|        - | 10793 | ` */` |
|  3030336 | 10794 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10795 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10796 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10797 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10798 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10799 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10800 | `	)` |
|        2 | 10801 |  |
|  3030338 | 10802 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10803 | `	VmRefObj *pRef;` |
|        - | 10804 | `	/* Check if the referenced object already exists */` |
|  3030338 | 10805 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3030338 | 10806 | `	if( pRef == 0 ){` |
|        - | 10807 | `		/* Create a new entry */` |
|  3000802 | 10808 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3000802 | 10809 | `		if( pRef == 0 ){` |
|      ! 0 | 10810 | `			return SXERR_MEM;` |
|        - | 10811 | `		}` |
|  3000802 | 10812 | `		pRef->iFlags = iFlags;` |
|        - | 10813 | `		/* Install the entry */` |
|  3000802 | 10814 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1500400 | 10815 | `	}` |
|  3030338 | 10816 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3030338 | 10817 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10818 | `		VmSlot sRef;` |
|        - | 10819 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10820 | `		 * be deleted when we leave this frame.` |
|        - | 10821 | `		 */` |
|    76680 | 10822 | `		sRef.nIdx = nIdx;` |
|    76680 | 10823 | `		sRef.pUserData = pEntry;` |
|    76680 | 10824 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10825 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10826 | `		}` |
|    38339 | 10827 | `	}` |
|  3030338 | 10828 | `	if( pEntry ){` |
|        - | 10829 | `		/* Address of the hash-entry */` |
|   106024 | 10830 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    53011 | 10831 | `	}` |
|  3030338 | 10832 | `	if( pMapEntry ){` |
|        - | 10833 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2919444 | 10834 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1459721 | 10835 | `	}` |
|  3030338 | 10836 | `	return SXRET_OK;` |
|  1515170 | 10837 |  |
|        - | 10838 | `/*` |
|        - | 10839 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10840 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10841 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10842 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10843 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10844 | ` * Refer to the official for more information on this powerful` |
|        - | 10845 | ` * extension.` |
|        - | 10846 | ` */` |
|  2958570 | 10847 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10848 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10849 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10850 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10851 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10852 | `	)` |
|        2 | 10853 |  |
|        - | 10854 | `	VmRefObj *pRef;` |
|        - | 10855 | `	sxu32 n;` |
|        - | 10856 | `	/* Check if the referenced object already exists */` |
|  2958572 | 10857 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2958572 | 10858 | `	if( pRef == 0 ){` |
|        - | 10859 | `		/* Not such entry */` |
|    76626 | 10860 | `		return SXERR_NOTFOUND;` |
|        - | 10861 | `	}` |
|        - | 10862 | `	/* Remove the desired entry */` |
|  2881948 | 10863 | `	if( pEntry ){` |
|        - | 10864 | `		SyHashEntry **apEntry;` |
|       56 | 10865 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10866 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10867 | `			if( apEntry[n] == pEntry ){` |
|        - | 10868 | `				/* Nullify the entry */` |
|       56 | 10869 | `				apEntry[n] = 0;` |
|        - | 10870 | `				/*` |
|        - | 10871 | `				 * NOTE:` |
|        - | 10872 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10873 | `				 * we avoid wasting spaces.` |
|        - | 10874 | `				 */` |
|       27 | 10875 | `			}` |
|       79 | 10876 | `		}` |
|       27 | 10877 | `	}` |
|  2881948 | 10878 | `	if( pMapEntry ){` |
|        - | 10879 | `		ph7_hashmap_node **apNode;` |
|  2881894 | 10880 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5763880 | 10881 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2881988 | 10882 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10883 | `				/* nullify the entry */` |
|  2881894 | 10884 | `				apNode[n] = 0;` |
|  1440946 | 10885 | `			}` |
|  1440995 | 10886 | `		}` |
|  1440946 | 10887 | `	}` |
|  2881948 | 10888 | `	return SXRET_OK;` |
|  1479287 | 10889 |  |
|        - | 10890 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10891 | `/*` |
|        - | 10892 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10893 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10894 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10895 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10896 | ` * For more information on how to register IO stream devices,please` |
|        - | 10897 | ` * refer to the official documentation.` |
|        - | 10898 | ` */` |
|    23346 | 10899 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10900 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10901 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10902 | `	int nByte              /* *pzDevice length*/` |
|        - | 10903 | `	)` |
|        2 | 10904 |  |
|        - | 10905 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10906 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10907 | `	SyString sDev,sCur;` |
|        - | 10908 | `	sxu32 n,nEntry;` |
|        - | 10909 | `	int rc;` |
|        - | 10910 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23348 | 10911 | `	zNext = zCur = zIn = *pzDevice;` |
|    23348 | 10912 | `	zEnd = &zIn[nByte];` |
|  1491721 | 10913 | `	while( zIn < zEnd ){` |
|  1468377 | 10914 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10915 | `			/* Got one */` |
|        3 | 10916 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10917 | `			break;` |
|        - | 10918 | `		}` |
|        - | 10919 | `		/* Advance the cursor */` |
|  1468375 | 10920 | `		zIn++;` |
|        2 | 10921 | `	}` |
|    23348 | 10922 | `	if( zIn >= zEnd ){` |
|        - | 10923 | `		/* No such scheme,return the default stream */` |
|    23346 | 10924 | `		return pVm->pDefStream;` |
|        - | 10925 | `	}` |
|        3 | 10926 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10927 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10928 | `	SyStringFullTrim(&sDev);` |
|        - | 10929 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10930 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10931 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10932 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10933 | `		pStream = apStream[n];` |
|        3 | 10934 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10935 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10936 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10937 | `		if( rc == 0 ){` |
|        - | 10938 | `			/* Stream device found */` |
|        3 | 10939 | `			*pzDevice = zNext;` |
|        3 | 10940 | `			return pStream;` |
|        - | 10941 | `		}` |
|      ! 0 | 10942 | `	}` |
|        - | 10943 | `	/* No such stream,return NULL */` |
|      ! 0 | 10944 | `	return 0;` |
|    11675 | 10945 |  |
|        - | 10946 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10947 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10948 |  |
