# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4707/6283 lines (74.92%)

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
|   778986 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   778988 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   778958 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   778950 |    94 | `	return FALSE;` |
|   389517 |    95 |  |
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
|   502646 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   502648 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   502648 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   502644 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   502644 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   502644 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   502644 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   502644 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   502644 |   142 | `	pCons->xExpand = xExpand;` |
|   502644 |   143 | `	pCons->pUserData = pUserData;` |
|   502644 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   502644 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   502644 |   151 | `	return SXRET_OK;` |
|   251325 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|  1146376 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1146378 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1146378 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|  1146378 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1146378 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|  1146378 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|  1146378 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1146378 |   185 | `	pFunc->pVm   = pVm;` |
|  1146378 |   186 | `	pFunc->xFunc = xFunc;` |
|  1146378 |   187 | `	pFunc->pUserData = pUserData;` |
|  1146378 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|  1146378 |   190 | `	*ppOut = pFunc;` |
|  1146378 |   191 | `	return SXRET_OK;` |
|   573190 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|  1148852 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1148854 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1148854 |   213 | `	if( pEntry ){` |
|     2478 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2478 |   215 | `		pFunc->pUserData = pUserData;` |
|     2478 |   216 | `		pFunc->xFunc = xFunc;` |
|     2478 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2478 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|  1146378 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1146378 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|  1146378 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1146378 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|  1146378 |   233 | `	return SXRET_OK;` |
|   574428 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   167876 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   167878 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   167878 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   167878 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   167878 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   167878 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   167878 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   167878 |   260 | `	pFunc->iFlags = iFlags;` |
|   167878 |   261 | `	pFunc->pUserData = pUserData;` |
|   167878 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   167878 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   568160 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   568162 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    36116 |   283 | `		pName = &pFunc->sName;` |
|    18057 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   568162 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   568162 |   287 | `	if( pEntry ){` |
|   422434 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   422434 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   422434 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|   145730 |   297 | `	pFunc->pNextName = 0;` |
|   145730 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   145730 |   299 | `	return rc;` |
|   284082 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    41498 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    41500 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    41500 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    41500 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    41470 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    41470 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    41470 |   324 | `	return rc;` |
|    20751 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  3373782 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3373784 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  3373784 |   342 | `	sInstr.iP1 = iP1;` |
|  3373784 |   343 | `	sInstr.iP2 = iP2;` |
|  3373784 |   344 | `	sInstr.p3  = p3;` |
|  3373784 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   194982 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    97490 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  3373784 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3373784 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  3373784 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   402004 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   402006 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   402006 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   402006 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   201002 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   201004 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   192168 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   192170 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   192170 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|  1014268 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|  1014270 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   182762 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   182764 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   655166 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   655168 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    28060 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    28062 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    28062 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    28062 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    28062 |   417 | `	return &aInstr[n - 2];` |
|    14032 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    16012 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    16014 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16014 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    16014 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    16014 |   437 | `	pFrame->pUserData = pUserData;` |
|    16014 |   438 | `	pFrame->pThis = pThis;` |
|    16014 |   439 | `	pFrame->pVm = pVm;` |
|    16014 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16014 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16014 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16014 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16014 |   444 | `	return pFrame;` |
|     8008 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    15970 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    15972 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15972 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    15972 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    15972 |   466 | `	pVm->pFrame = pFrame;` |
|    15972 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    13236 |   469 | `		*ppFrame = pFrame;` |
|     6617 |   470 | `	}` |
|    15972 |   471 | `	return SXRET_OK;` |
|     7987 |   472 |  |
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
|    13234 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    13236 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13236 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    13236 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13236 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13172 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    91826 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    78656 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39329 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    13172 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    91882 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    78712 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39357 |   535 | `			}` |
|     6585 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    13236 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13236 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    13236 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13236 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    13236 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6617 |   544 | `	}` |
|    13236 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6293208 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6293486 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6293210 |   556 | `	return pFrame;` |
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
|   110950 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|   110952 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   454603 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   288178 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   288178 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   110952 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    48082 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    62872 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|     5498 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5498 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|     2748 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    62872 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   626359 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   532054 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   532054 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   532048 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   532048 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   266023 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    62872 |   738 | `	pClass->bMounted = TRUE;` |
|    62872 |   739 | `	return SXRET_OK;` |
|    55477 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1184 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1186 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1186 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4874 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3690 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3690 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3690 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3690 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3690 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3684 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3684 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3684 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3684 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1188 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      593 |   777 | `			}` |
|     3684 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3684 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3684 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1843 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1186 |   800 | `	return SXRET_OK;` |
|      594 |   801 |  |
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
|   385168 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   385170 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   376962 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   188480 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   385170 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   385170 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   385170 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   385170 |   830 | `	return pObj;` |
|   192586 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2144402 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2144404 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2144404 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1072201 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2144404 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2144404 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2144404 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2144404 |   853 | `	return pObj;` |
|  1072203 |   854 |  |
|        - |   855 | `/* Forward declaration */` |
|        - |   856 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   857 | `/* Forward declarations for Fiber C functions */` |
|        - |   858 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   859 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   860 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   861 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   862 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   863 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   864 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   865 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   866 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   867 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   868 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   869 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   870 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   871 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   872 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   873 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   874 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   875 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   876 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   877 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   878 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   879 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   880 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   881 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   882 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   883 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   884 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   885 | `/*` |
|        - |   886 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   887 | ` * directly as foreign functions.` |
|        - |   888 | ` */` |
|        - |   889 | `#define PH7_BUILTIN_LIB \` |
|        - |   890 | `	"class Exception { "\` |
|        - |   891 | `    "protected $message = 'Unknown exception';"\` |
|        - |   892 | `    "protected $code = 0;"\` |
|        - |   893 | `    "protected $file;"\` |
|        - |   894 | `    "protected $line;"\` |
|        - |   895 | `    "protected $trace;"\` |
|        - |   896 | `    "protected $previous;"\` |
|        - |   897 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   898 | `	"   if( isset($message) ){"\` |
|        - |   899 | `	"	  $this->message = $message;"\` |
|        - |   900 | `	"   }"\` |
|        - |   901 | `	"   $this->code = $code;"\` |
|        - |   902 | `	"   $this->file = __FILE__;"\` |
|        - |   903 | `	"   $this->line = __LINE__;"\` |
|        - |   904 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   905 | `	"   if( isset($previous) ){"\` |
|        - |   906 | `	"     $this->previous = $previous;"\` |
|        - |   907 | `	"   }"\` |
|        - |   908 | `	"}"\` |
|        - |   909 | `	"public function getMessage(){"\` |
|        - |   910 | `	"   return $this->message;"\` |
|        - |   911 | `	"}"\` |
|        - |   912 | `	" public function getCode(){"\` |
|        - |   913 | `	"  return $this->code;"\` |
|        - |   914 | `	"}"\` |
|        - |   915 | `	"public function getFile(){"\` |
|        - |   916 | `	"  return $this->file;"\` |
|        - |   917 | `	"}"\` |
|        - |   918 | `	"public function getLine(){"\` |
|        - |   919 | `	"  return $this->line;"\` |
|        - |   920 | `	"}"\` |
|        - |   921 | `	"public function getTrace(){"\` |
|        - |   922 | `	"   return $this->trace;"\` |
|        - |   923 | `	"}"\` |
|        - |   924 | `	"public function getTraceAsString(){"\` |
|        - |   925 | `	"  return debug_string_backtrace();"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"public function getPrevious(){"\` |
|        - |   928 | `	"    return $this->previous;"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"public function __toString(){"\` |
|        - |   931 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   932 | `    "}"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"class Error extends Exception { }"\` |
|        - |   935 | `	"class TypeError extends Error { }"\` |
|        - |   936 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   937 | `	"class ValueError extends Error { }"\` |
|        - |   938 | `	"class FiberError extends Error { }"\` |
|        - |   939 | `	"class AssertionError extends Error { }"\` |
|        - |   940 | `	"class ErrorException extends Exception { "\` |
|        - |   941 | `	"protected $severity;"\` |
|        - |   942 | `	"public function __construct(string $message = null,"\` |
|        - |   943 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   944 | `	"   if( isset($message) ){"\` |
|        - |   945 | `	"	  $this->message = $message;"\` |
|        - |   946 | `	"   }"\` |
|        - |   947 | `	"   $this->severity = $severity;"\` |
|        - |   948 | `	"   $this->code = $code;"\` |
|        - |   949 | `	"   $this->file = $filename;"\` |
|        - |   950 | `	"   $this->line = $lineno;"\` |
|        - |   951 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   952 | `	"   if( isset($previous) ){"\` |
|        - |   953 | `	"     $this->previous = $previous;"\` |
|        - |   954 | `	"   }"\` |
|        - |   955 | `	"}"\` |
|        - |   956 | `	"public function getSeverity(){"\` |
|        - |   957 | `	"   return $this->severity;"\` |
|        - |   958 | `    "}"\` |
|        - |   959 | `	"}"\` |
|        - |   960 | `	"interface Iterator {"\` |
|        - |   961 | `	"public function current();"\` |
|        - |   962 | `	"public function key();"\` |
|        - |   963 | `	"public function next();"\` |
|        - |   964 | `	"public function rewind();"\` |
|        - |   965 | `	"public function valid();"\` |
|        - |   966 | `	"}"\` |
|        - |   967 | `	"interface IteratorAggregate {"\` |
|        - |   968 | `	"public function getIterator();"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"interface Serializable {"\` |
|        - |   971 | `	"public function serialize();"\` |
|        - |   972 | `	"public function unserialize(string $serialized);"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"/* Directory releated IO */"\` |
|        - |   975 | `	"class Directory {"\` |
|        - |   976 | `	"public $handle = null;"\` |
|        - |   977 | `	"public $path  = null;"\` |
|        - |   978 | `	"public function __construct(string $path)"\` |
|        - |   979 | `	"{"\` |
|        - |   980 | `	"   $this->handle = opendir($path);"\` |
|        - |   981 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   982 | `	"      $this->path = $path;"\` |
|        - |   983 | `	"   }"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	"public function __destruct()"\` |
|        - |   986 | `	"{"\` |
|        - |   987 | `	"  if( $this->handle != null ){"\` |
|        - |   988 | `	"       closedir($this->handle);"\` |
|        - |   989 | `	"  }"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"public function read()"\` |
|        - |   992 | `	"{"\` |
|        - |   993 | `	"    return readdir($this->handle);"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"public function rewind()"\` |
|        - |   996 | `	"{"\` |
|        - |   997 | `	"    rewinddir($this->handle);"\` |
|        - |   998 | `	"}"\` |
|        - |   999 | `	"public function close()"\` |
|        - |  1000 | `	"{"\` |
|        - |  1001 | `	"    closedir($this->handle);"\` |
|        - |  1002 | `	"    $this->handle = null;"\` |
|        - |  1003 | `	"}"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"class Fiber {"\` |
|        - |  1006 | `	"  private $__ctx;"\` |
|        - |  1007 | `	"  private $__callable;"\` |
|        - |  1008 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1009 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1010 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1011 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1012 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1013 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1014 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1015 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1016 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1017 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1018 | `	"}"\` |
|        - |  1019 | `	"class Generator implements Iterator {"\` |
|        - |  1020 | `	"  private $__ctx;"\` |
|        - |  1021 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1022 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1023 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1024 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1025 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1026 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1027 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1028 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1029 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"class stdClass{"\` |
|        - |  1032 | `	"  public $value;"\` |
|        - |  1033 | `	" /* Magic methods */"\` |
|        - |  1034 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1035 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1036 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1037 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1038 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1039 | `	"}"\` |
|        - |  1040 | `	"function dir(string $path){"\` |
|        - |  1041 | `	"   return new Directory($path);"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"function Dir(string $path){"\` |
|        - |  1044 | `	"   return new Directory($path);"\` |
|        - |  1045 | `	"}"\` |
|        - |  1046 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1047 | `    "{"\` |
|        - |  1048 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1049 | `	"  $aDir = array();"\` |
|        - |  1050 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1051 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1052 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1053 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1054 | `	"   }"\` |
|        - |  1055 | `	"  closedir($pHandle);"\` |
|        - |  1056 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1057 | `	"      rsort($aDir);"\` |
|        - |  1058 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1059 | `	"      sort($aDir);"\` |
|        - |  1060 | `	"  }"\` |
|        - |  1061 | `	"  return $aDir;"\` |
|        - |  1062 | `	"}"\` |
|        - |  1063 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1064 | `	"/* Open the target directory */"\` |
|        - |  1065 | `	"$zDir = dirname($pattern);"\` |
|        - |  1066 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1067 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1068 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1069 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1070 | `	"	return FALSE;"\` |
|        - |  1071 | `	"}"\` |
|        - |  1072 | `	"$pattern = basename($pattern);"\` |
|        - |  1073 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1074 | `	"/* Loop throw available entries */"\` |
|        - |  1075 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1076 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1077 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1078 | `	"	if( $rc ){"\` |
|        - |  1079 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1080 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1081 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1082 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1083 | `	"		  }"\` |
|        - |  1084 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1085 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1086 | `	"		 continue;"\` |
|        - |  1087 | `	"	   }"\` |
|        - |  1088 | `	"	   /* Add the entry */"\` |
|        - |  1089 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1090 | `	"	}"\` |
|        - |  1091 | `	" }"\` |
|        - |  1092 | `	"/* Close the handle */"\` |
|        - |  1093 | `	"closedir($pHandle);"\` |
|        - |  1094 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1095 | `	"  /* Sort the array */"\` |
|        - |  1096 | `	"  sort($pArray);"\` |
|        - |  1097 | `	"}"\` |
|        - |  1098 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1099 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1100 | `	"  $pArray[] = $pattern;"\` |
|        - |  1101 | `	"}"\` |
|        - |  1102 | `	"/* Return the created array */"\` |
|        - |  1103 | `	"return $pArray;"\` |
|        - |  1104 | `   "}"\` |
|        - |  1105 | `   "/* Creates a temporary file */"\` |
|        - |  1106 | `   "function tmpfile(){"\` |
|        - |  1107 | `   "  /* Extract the temp directory */"\` |
|        - |  1108 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1109 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1110 | `   "    /* Use the current dir */"\` |
|        - |  1111 | `   "    $zTempDir = '.';"\` |
|        - |  1112 | `   "  }"\` |
|        - |  1113 | `   "  /* Create the file */"\` |
|        - |  1114 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1115 | `   "  return $pHandle;"\` |
|        - |  1116 | `   "}"\` |
|        - |  1117 | `   "/* Creates a temporary filename */"\` |
|        - |  1118 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1119 | `   "{"\` |
|        - |  1120 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1121 | `   "}"\` |
|        - |  1122 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1123 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1124 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1125 | `   "/* Copy arguments */"\` |
|        - |  1126 | `   "$nArgs = func_num_args();"\` |
|        - |  1127 | `   "$pNew = array();"\` |
|        - |  1128 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1129 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1130 | `    "}"\` |
|        - |  1131 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1132 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1133 | `	"/* Erase */"\` |
|        - |  1134 | `	"array_erase($pArray);"\` |
|        - |  1135 | `	"/* Unshift */"\` |
|        - |  1136 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1137 | `	"return sizeof($pArray);"\` |
|        - |  1138 | `    "}"\` |
|        - |  1139 | `	"function array_merge_recursive(){"\` |
|        - |  1140 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1141 | `    "$arrays = func_get_args();"\` |
|        - |  1142 | `    "$narrays = count($arrays);"\` |
|        - |  1143 | `    "$ret = array();"\` |
|        - |  1144 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1145 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1146 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1147 | `	 " }"\` |
|        - |  1148 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1149 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1150 | `     "  if( $keyIsInt ) {"\` |
|        - |  1151 | `     "   $ret[] = $value;"\` |
|        - |  1152 | `     "  } else {"\` |
|        - |  1153 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1154 | `     "    $cur = $ret[$key];"\` |
|        - |  1155 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1156 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1157 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1158 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1159 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1160 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1161 | `     "    } else {"\` |
|        - |  1162 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1163 | `     "    }"\` |
|        - |  1164 | `     "   } else {"\` |
|        - |  1165 | `     "    $ret[$key] = $value;"\` |
|        - |  1166 | `     "   }"\` |
|        - |  1167 | `     "  }"\` |
|        - |  1168 | `     " }"\` |
|        - |  1169 | `	 " }"\` |
|        - |  1170 | `	 " return $ret;"\` |
|        - |  1171 | `    "}"\` |
|        - |  1172 | `	"function max(){"\` |
|        - |  1173 | `    "  $pArgs = func_get_args();"\` |
|        - |  1174 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1175 | `	"  return null;"\` |
|        - |  1176 | `    " }"\` |
|        - |  1177 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1178 | `    " $pArg = $pArgs[0];"\` |
|        - |  1179 | `	" if( !is_array($pArg) ){"\` |
|        - |  1180 | `	"   return $pArg; "\` |
|        - |  1181 | `	" }"\` |
|        - |  1182 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1183 | `	"   return null;"\` |
|        - |  1184 | `	" }"\` |
|        - |  1185 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1186 | `	" reset($pArg);"\` |
|        - |  1187 | `	" $max = current($pArg);"\` |
|        - |  1188 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1189 | `	"   if( $val > $max ){"\` |
|        - |  1190 | `	"     $max = $val;"\` |
|        - |  1191 | `    " }"\` |
|        - |  1192 | `	" }"\` |
|        - |  1193 | `	" return $max;"\` |
|        - |  1194 | `    " }"\` |
|        - |  1195 | `    " $max = $pArgs[0];"\` |
|        - |  1196 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1197 | `    " $val = $pArgs[$i];"\` |
|        - |  1198 | `	"if( $val > $max ){"\` |
|        - |  1199 | `	" $max = $val;"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `    " }"\` |
|        - |  1202 | `	" return $max;"\` |
|        - |  1203 | `    "}"\` |
|        - |  1204 | `	"function min(){"\` |
|        - |  1205 | `    "  $pArgs = func_get_args();"\` |
|        - |  1206 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1207 | `	"  return null;"\` |
|        - |  1208 | `    " }"\` |
|        - |  1209 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1210 | `    " $pArg = $pArgs[0];"\` |
|        - |  1211 | `	" if( !is_array($pArg) ){"\` |
|        - |  1212 | `	"   return $pArg; "\` |
|        - |  1213 | `	" }"\` |
|        - |  1214 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1215 | `	"   return null;"\` |
|        - |  1216 | `	" }"\` |
|        - |  1217 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1218 | `	" reset($pArg);"\` |
|        - |  1219 | `	" $min = current($pArg);"\` |
|        - |  1220 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1221 | `	"   if( $val < $min ){"\` |
|        - |  1222 | `	"     $min = $val;"\` |
|        - |  1223 | `    " }"\` |
|        - |  1224 | `	" }"\` |
|        - |  1225 | `	" return $min;"\` |
|        - |  1226 | `    " }"\` |
|        - |  1227 | `    " $min = $pArgs[0];"\` |
|        - |  1228 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1229 | `    " $val = $pArgs[$i];"\` |
|        - |  1230 | `	"if( $val < $min ){"\` |
|        - |  1231 | `	" $min = $val;"\` |
|        - |  1232 | `	" }"\` |
|        - |  1233 | `    " }"\` |
|        - |  1234 | `	" return $min;"\` |
|        - |  1235 | `	"}"\` |
|        - |  1236 | `	"function fileowner(string $file){"\` |
|        - |  1237 | `    " $a = stat($file);"\` |
|        - |  1238 | `	" if( !is_array($a) ){"\` |
|        - |  1239 | `	"	return false;"\` |
|        - |  1240 | `	" }"\` |
|        - |  1241 | `	" return $a['uid'];"\` |
|        - |  1242 | `    "}"\` |
|        - |  1243 | `    "function filegroup(string $file){"\` |
|        - |  1244 | `	" $a = stat($file);"\` |
|        - |  1245 | `	" if( !is_array($a) ){"\` |
|        - |  1246 | `	"	return false;"\` |
|        - |  1247 | `	" }"\` |
|        - |  1248 | `	" return $a['gid'];"\` |
|        - |  1249 | `    "}"\` |
|        - |  1250 | `	 "function fileinode(string $file){"\` |
|        - |  1251 | `	" $a = stat($file);"\` |
|        - |  1252 | `	" if( !is_array($a) ){"\` |
|        - |  1253 | `	"	return false;"\` |
|        - |  1254 | `	" }"\` |
|        - |  1255 | `	" return $a['ino'];"\` |
|        - |  1256 | `    "}"` |
|        - |  1257 |  |
|        - |  1258 | `/*` |
|        - |  1259 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1260 | ` * start compiling the target PHP program.` |
|        - |  1261 | ` */` |
|     2736 |  1262 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1263 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1264 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1265 | `	 )` |
|        2 |  1266 |  |
|        - |  1267 | `	SyString sBuiltin;` |
|        - |  1268 | `	ph7_value *pObj;` |
|        - |  1269 | `	sxi32 rc;` |
|        - |  1270 | `	/* Zero the structure */` |
|     2738 |  1271 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1272 | `	/* Initialize VM fields */` |
|     2738 |  1273 | `	pVm->pEngine = &(*pEngine);` |
|     2738 |  1274 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1275 | `	/* Instructions containers */` |
|     2738 |  1276 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2738 |  1277 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2738 |  1278 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1279 | `	/* Object containers */` |
|     2738 |  1280 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2738 |  1281 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1282 | `	/* Virtual machine internal containers */` |
|     2738 |  1283 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2738 |  1284 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2738 |  1285 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2738 |  1286 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2738 |  1287 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2738 |  1288 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2738 |  1289 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2738 |  1290 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2738 |  1291 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2738 |  1292 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2738 |  1293 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2738 |  1294 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2738 |  1295 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2738 |  1296 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2738 |  1297 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2738 |  1298 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2738 |  1299 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2738 |  1300 | `	pVm->pPendingException = 0;` |
|        - |  1301 | `	/* Configuration containers */` |
|     2738 |  1302 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2738 |  1303 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2738 |  1304 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2738 |  1305 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2738 |  1306 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2738 |  1307 | `	pVm->iResponseStatus = 200;` |
|     2738 |  1308 | `	pVm->bHeadersSent = 0;` |
|     2738 |  1309 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1310 | `	/* Error callbacks containers */` |
|     2738 |  1311 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2738 |  1312 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2738 |  1313 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2738 |  1314 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2738 |  1315 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1316 | `	/* Set a default recursion limit */` |
|        - |  1317 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2738 |  1318 | `	pVm->nMaxDepth = 32;` |
|        - |  1319 | `#else` |
|        - |  1320 | `	pVm->nMaxDepth = 16;` |
|        - |  1321 | `#endif` |
|        - |  1322 | `	/* Default assertion flags */` |
|     2738 |  1323 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1324 | `	/* JSON return status */` |
|     2738 |  1325 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1326 | `	/* PRNG context */` |
|     2738 |  1327 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1328 | `	/* Install the null constant */` |
|     2738 |  1329 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2738 |  1330 | `	if( pObj == 0 ){` |
|      ! 0 |  1331 | `		rc = SXERR_MEM;` |
|      ! 0 |  1332 | `		goto Err;` |
|        - |  1333 | `	}` |
|     2738 |  1334 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1335 | `	/* Install the boolean TRUE constant */` |
|     2738 |  1336 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2738 |  1337 | `	if( pObj == 0 ){` |
|      ! 0 |  1338 | `		rc = SXERR_MEM;` |
|      ! 0 |  1339 | `		goto Err;` |
|        - |  1340 | `	}` |
|     2738 |  1341 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1342 | `	/* Install the boolean FALSE constant */` |
|     2738 |  1343 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2738 |  1344 | `	if( pObj == 0 ){` |
|      ! 0 |  1345 | `		rc = SXERR_MEM;` |
|      ! 0 |  1346 | `		goto Err;` |
|        - |  1347 | `	}` |
|     2738 |  1348 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1349 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1350 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1351 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2738 |  1352 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2738 |  1353 | `	if( pObj == 0 ){` |
|      ! 0 |  1354 | `		rc = SXERR_MEM;` |
|      ! 0 |  1355 | `		goto Err;` |
|        - |  1356 | `	}` |
|     2738 |  1357 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1358 | `	/* Create the global frame */` |
|     2738 |  1359 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2738 |  1360 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|        - |  1363 | `	/* Initialize the code generator */` |
|     2738 |  1364 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2738 |  1365 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1366 | `		goto Err;` |
|        - |  1367 | `	}` |
|        - |  1368 | `	/* VM correctly initialized,set the magic number */` |
|     2738 |  1369 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2738 |  1370 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1371 | `	/* Compile the built-in library */` |
|     2738 |  1372 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1373 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2738 |  1374 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1375 | `	/* Register Fiber internal C functions */` |
|     2738 |  1376 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2738 |  1377 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2738 |  1378 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2738 |  1379 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2738 |  1380 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2738 |  1381 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2738 |  1382 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2738 |  1383 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2738 |  1384 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2738 |  1385 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1386 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2738 |  1387 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2738 |  1388 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2738 |  1389 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2738 |  1390 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2738 |  1391 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2738 |  1392 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2738 |  1393 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2738 |  1394 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2738 |  1395 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2738 |  1396 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1397 | `	/* Reset the code generator */` |
|     2738 |  1398 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2738 |  1399 | `	return SXRET_OK;` |
|      ! 0 |  1400 | `Err:` |
|      ! 0 |  1401 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1402 | `	return rc;` |
|     1370 |  1403 |  |
|        - |  1404 | `/*` |
|        - |  1405 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1406 | ` * routine which store the output in an internal blob.` |
|        - |  1407 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1408 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1409 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1410 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1411 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1412 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1413 | ` * to finish executing and extracting the output.` |
|        - |  1414 | ` */` |
|       38 |  1415 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1416 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1417 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1418 | `	void *pUserData     /* User private data */` |
|        - |  1419 | `	)` |
|      ! 0 |  1420 |  |
|        - |  1421 | `	 sxi32 rc;` |
|        - |  1422 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1423 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1424 | `	 return rc;` |
|      ! 0 |  1425 |  |
|        - |  1426 | `/*` |
|        - |  1427 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1428 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1429 | ` */` |
|    13378 |  1430 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1431 |  |
|    13380 |  1432 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13380 |  1433 | `	if( xCons != VmObConsumer ){` |
|     6468 |  1434 | `		pVm->nOutputLen += nLen;` |
|     6468 |  1435 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      896 |  1436 | `			pVm->bHeadersSent = 1;` |
|      447 |  1437 | `		}` |
|     3233 |  1438 | `	}` |
|    13380 |  1439 |  |
|        - |  1440 | `#define VM_STACK_GUARD 16` |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1443 | ` * our compiled PHP program.` |
|        - |  1444 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1445 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1446 | ` */` |
|    32526 |  1447 | `static ph7_value * VmNewOperandStack(` |
|        - |  1448 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1449 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1450 | `	)` |
|        2 |  1451 |  |
|        - |  1452 | `	ph7_value *pStack;` |
|        - |  1453 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1454 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1455 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1456 | `  ** on the maximum stack depth required.` |
|        - |  1457 | `  **` |
|        - |  1458 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1459 | `  */` |
|    32528 |  1460 | `	nInstr += VM_STACK_GUARD;` |
|    32528 |  1461 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32528 |  1462 | `	if( pStack == 0 ){` |
|      ! 0 |  1463 | `		return 0;` |
|        - |  1464 | `	}` |
|        - |  1465 | `	/* Initialize the operand stack */` |
|  2038516 |  1466 | `	while( nInstr > 0 ){` |
|  2005990 |  1467 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2005990 |  1468 | `		--nInstr;` |
|        2 |  1469 | `	}` |
|        - |  1470 | `	/* Ready for bytecode execution */` |
|    32528 |  1471 | `	return pStack;` |
|    16265 |  1472 |  |
|        - |  1473 | `/* Forward declaration */` |
|        - |  1474 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1475 | `/*` |
|        - |  1476 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1477 | ` * This routine gets called by the PH7 engine after` |
|        - |  1478 | ` * successful compilation of the target PHP program.` |
|        - |  1479 | ` */` |
|     2476 |  1480 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1481 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1482 | `	)` |
|        2 |  1483 |  |
|        - |  1484 | `	SyHashEntry *pEntry;` |
|        - |  1485 | `	sxi32 rc;` |
|     2478 |  1486 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1487 | `		/* Initialize your VM first */` |
|      ! 0 |  1488 | `		return SXERR_CORRUPT;` |
|        - |  1489 | `	}` |
|        - |  1490 | `	/* Mark the VM ready for byte-code execution */` |
|     2478 |  1491 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1492 | `	/* Release the code generator now we have compiled our program */` |
|     2478 |  1493 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1494 | `	/* Emit the DONE instruction */` |
|     2478 |  1495 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2478 |  1496 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1497 | `		return SXERR_MEM;` |
|        - |  1498 | `	}` |
|        - |  1499 | `	/* Script return value */` |
|     2478 |  1500 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1501 | `	/* Allocate a new operand stack */` |
|     2478 |  1502 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2478 |  1503 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1504 | `		return SXERR_MEM;` |
|        - |  1505 | `	}` |
|        - |  1506 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1507 | `	 * private data. */` |
|     2478 |  1508 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2478 |  1509 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1510 | `	/* Allocate the reference table */` |
|     2478 |  1511 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2478 |  1512 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2478 |  1513 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1514 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1515 | `		return SXERR_MEM;` |
|        - |  1516 | `	}` |
|        - |  1517 | `	/* Zero the reference table */` |
|     2478 |  1518 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1519 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2478 |  1520 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2478 |  1521 | `	if( rc != SXRET_OK ){` |
|        - |  1522 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1523 | `		return rc;` |
|        - |  1524 | `	}` |
|        - |  1525 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2478 |  1526 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2478 |  1527 | `	if( rc != SXRET_OK ){` |
|        - |  1528 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1529 | `		return rc;` |
|        - |  1530 | `	}` |
|        - |  1531 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2478 |  1532 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1533 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2478 |  1534 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1535 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2478 |  1536 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1537 | `	/* Initialize and install static and constants class attributes */` |
|     2478 |  1538 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    39782 |  1539 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    37306 |  1540 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    37306 |  1541 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1542 | `			return rc;` |
|        - |  1543 | `		}` |
|        2 |  1544 | `	}` |
|        - |  1545 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2478 |  1546 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1547 | `	/* VM is ready for bytecode execution */` |
|     2478 |  1548 | `	return SXRET_OK;` |
|     1240 |  1549 |  |
|        - |  1550 | `/*` |
|        - |  1551 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1552 | ` */` |
|      ! 0 |  1553 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1554 |  |
|      ! 0 |  1555 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1556 | `		return SXERR_CORRUPT;` |
|        - |  1557 | `	}` |
|        - |  1558 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1559 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1560 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1561 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1562 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1563 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1564 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1565 | `	pVm->bHttpContext = 0;` |
|        - |  1566 | `	/* Set the ready flag */` |
|      ! 0 |  1567 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1568 | `	return SXRET_OK;` |
|      ! 0 |  1569 |  |
|        - |  1570 | `/*` |
|        - |  1571 | ` * Release a Virtual Machine.` |
|        - |  1572 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1573 | ` */` |
|     2468 |  1574 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1575 |  |
|        - |  1576 | `	/* Set the stale magic number */` |
|     2470 |  1577 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1578 | `	/* Release the private memory subsystem */` |
|     2470 |  1579 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2470 |  1580 | `	return SXRET_OK;` |
|        2 |  1581 |  |
|        - |  1582 | `/*` |
|        - |  1583 | ` * Initialize a foreign function call context.` |
|        - |  1584 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1585 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1586 | ` * functions.` |
|        - |  1587 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1588 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1589 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1590 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1591 | ` */` |
|   566338 |  1592 | `static sxi32 VmInitCallContext(` |
|        - |  1593 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1594 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1595 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1596 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1597 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1598 | `	)` |
|        2 |  1599 |  |
|   566340 |  1600 | `	pOut->pFunc = pFunc;` |
|   566340 |  1601 | `	pOut->pVm   = pVm;` |
|   566340 |  1602 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   566340 |  1603 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1604 | `	/* Assume a null return value */` |
|   566340 |  1605 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   566340 |  1606 | `	pOut->pRet = pRet;` |
|   566340 |  1607 | `	pOut->iFlags = iFlags;` |
|   566340 |  1608 | `	return SXRET_OK;` |
|        2 |  1609 |  |
|        - |  1610 | `/*` |
|        - |  1611 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1612 | ` * left behind.` |
|        - |  1613 | ` */` |
|   566338 |  1614 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1615 |  |
|        - |  1616 | `	sxu32 n;` |
|   566340 |  1617 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6866 |  1618 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19584 |  1619 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12720 |  1620 | `			if( apObj[n] == 0 ){` |
|        - |  1621 | `				/* Already released */` |
|      250 |  1622 | `				continue;` |
|        - |  1623 | `			}` |
|    12472 |  1624 | `			PH7_MemObjRelease(apObj[n]);` |
|    12472 |  1625 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6237 |  1626 | `		}` |
|     6866 |  1627 | `		SySetRelease(&pCtx->sVar);` |
|     3432 |  1628 | `	}` |
|   566340 |  1629 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1630 | `		ph7_aux_data *aAux;` |
|        - |  1631 | `		void *pChunk;` |
|        - |  1632 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1633 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1634 | `		 */` |
|        9 |  1635 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1636 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1637 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1638 | `			/* Release the chunk */` |
|       25 |  1639 | `			if( pChunk ){` |
|       25 |  1640 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1641 | `			}` |
|       13 |  1642 | `		}` |
|        9 |  1643 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1644 | `	}` |
|   566340 |  1645 |  |
|        - |  1646 | `/*` |
|        - |  1647 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1648 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1649 | ` */` |
|      248 |  1650 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1651 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1652 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1653 | `	)` |
|        2 |  1654 |  |
|      250 |  1655 | `	if( pValue == 0 ){` |
|        - |  1656 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1657 | `		return;` |
|        - |  1658 | `	}` |
|      250 |  1659 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1660 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1661 | `		sxu32 n;` |
|      936 |  1662 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1663 | `			if( apObj[n] == pValue ){` |
|      250 |  1664 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1665 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1666 | `				/* Mark as released */` |
|      250 |  1667 | `				apObj[n] = 0;` |
|      250 |  1668 | `				break;` |
|        - |  1669 | `			}` |
|      345 |  1670 | `		}` |
|      124 |  1671 | `	}` |
|      126 |  1672 |  |
|        - |  1673 | `/*` |
|        - |  1674 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1675 | ` */` |
|  3303668 |  1676 | `static void VmPopOperand(` |
|        - |  1677 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1678 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1679 | `	)` |
|        2 |  1680 |  |
|  3303670 |  1681 | `	ph7_value *pTos = *ppTos;` |
|  7017872 |  1682 | `	while( nPop > 0 ){` |
|  3714204 |  1683 | `		PH7_MemObjRelease(pTos);` |
|  3714204 |  1684 | `		pTos--;` |
|  3714204 |  1685 | `		nPop--;` |
|        2 |  1686 | `	}` |
|        - |  1687 | `	/* Top of the stack */` |
|  3303670 |  1688 | `	*ppTos = pTos;` |
|  3303670 |  1689 |  |
|        - |  1690 | `/*` |
|        - |  1691 | ` * Reserve a memory object.` |
|        - |  1692 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1693 | ` */` |
|  3016800 |  1694 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1695 |  |
|  3016802 |  1696 | `	ph7_value *pObj = 0;` |
|        - |  1697 | `	VmSlot *pSlot;` |
|        - |  1698 | `	sxu32 nIdx;` |
|        - |  1699 | `	/* Check for a free slot */` |
|  3016802 |  1700 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3016802 |  1701 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3016802 |  1702 | `	if( pSlot ){` |
|   872400 |  1703 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   872400 |  1704 | `		nIdx = pSlot->nIdx;` |
|   436199 |  1705 | `	}` |
|  3016802 |  1706 | `	if( pObj == 0 ){` |
|        - |  1707 | `		/* Reserve a new memory object */` |
|  2144404 |  1708 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2144404 |  1709 | `		if( pObj == 0 ){` |
|      ! 0 |  1710 | `			return 0;` |
|        - |  1711 | `		}` |
|  1072201 |  1712 | `	}` |
|        - |  1713 | `	/* Set a null default value */` |
|  3016802 |  1714 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3016802 |  1715 | `	pObj->nIdx = nIdx;` |
|  3016802 |  1716 | `	return pObj;` |
|  1508402 |  1717 |  |
|        - |  1718 | `/*` |
|        - |  1719 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1720 | ` */` |
|    31658 |  1721 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1722 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1723 | `	const char *zKey,  /* Entry key */` |
|        - |  1724 | `	sxu32 nByte,       /* Key length */` |
|        - |  1725 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1726 | `	)` |
|        2 |  1727 |  |
|        - |  1728 | `	ph7_value sKey;` |
|        - |  1729 | `	sxi32 rc;` |
|    31660 |  1730 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    31660 |  1731 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1732 | `	/* Perform the insertion */` |
|    31660 |  1733 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    31660 |  1734 | `	PH7_MemObjRelease(&sKey);` |
|    31660 |  1735 | `	return rc;` |
|        2 |  1736 |  |
|        - |  1737 | `/*` |
|        - |  1738 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1739 | ` * Return a pointer to the variable value on success.` |
|        - |  1740 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1741 | ` */` |
|  3084520 |  1742 | `static ph7_value * VmExtractMemObj(` |
|        - |  1743 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1744 | `	const SyString *pName, /* Variable name */` |
|        - |  1745 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1746 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1747 | `	)` |
|        2 |  1748 |  |
|  3084522 |  1749 | `	int bNullify = FALSE;` |
|        - |  1750 | `	SyHashEntry *pEntry;` |
|        - |  1751 | `	VmFrame *pFrame;` |
|        - |  1752 | `	ph7_value *pObj;` |
|        - |  1753 | `	sxu32 nIdx;` |
|        - |  1754 | `	sxi32 rc;` |
|        - |  1755 | `	/* Point to the top active frame */` |
|  3084522 |  1756 | `	pFrame = pVm->pFrame;` |
|  3084522 |  1757 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1758 | `	/* Perform the lookup */` |
|  3084522 |  1759 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1760 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1761 | `		pName = &sAnnon;` |
|        - |  1762 | `		/* Always nullify the object */` |
|      ! 0 |  1763 | `		bNullify = TRUE;` |
|      ! 0 |  1764 | `		bDup = FALSE;` |
|      ! 0 |  1765 | `	}` |
|        - |  1766 | `	/* Check the superglobals table first */` |
|  3084522 |  1767 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3084522 |  1768 | `	if( pEntry == 0 ){` |
|        - |  1769 | `		/* Query the top active frame */` |
|  3084482 |  1770 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3084482 |  1771 | `		if( pEntry == 0 ){` |
|    85600 |  1772 | `			char *zName = (char *)pName->zString;` |
|        - |  1773 | `			VmSlot sLocal;` |
|    85600 |  1774 | `			if( !bCreate ){` |
|        - |  1775 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1776 | `				return 0;` |
|        - |  1777 | `			}` |
|        - |  1778 | `			/* No such variable,automatically create a new one and install` |
|        - |  1779 | `			 * it in the current frame.` |
|        - |  1780 | `			 */` |
|    85564 |  1781 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    85564 |  1782 | `			if( pObj == 0 ){` |
|      ! 0 |  1783 | `				return 0;` |
|        - |  1784 | `			}` |
|    85564 |  1785 | `			nIdx = pObj->nIdx;` |
|    85564 |  1786 | `			if( bDup ){` |
|        - |  1787 | `				/* Duplicate name */` |
|      168 |  1788 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1789 | `				if( zName == 0 ){` |
|      ! 0 |  1790 | `					return 0;` |
|        - |  1791 | `				}` |
|       83 |  1792 | `			}` |
|        - |  1793 | `			/* Link to the top active VM frame */` |
|    85564 |  1794 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    85564 |  1795 | `			if( rc != SXRET_OK ){` |
|        - |  1796 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1797 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1798 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1799 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1800 | `				return 0;` |
|        - |  1801 | `			}` |
|    85564 |  1802 | `			if( pFrame->pParent != 0 ){` |
|        - |  1803 | `				/* Local variable */` |
|    78692 |  1804 | `				sLocal.nIdx = nIdx;` |
|    78692 |  1805 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39347 |  1806 | `			}else{` |
|        - |  1807 | `				/* Register in the $GLOBALS array */` |
|     6874 |  1808 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1809 | `			}` |
|        - |  1810 | `			/* Install in the reference table */` |
|    85564 |  1811 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1812 | `			/* Save object index */` |
|    85564 |  1813 | `			pObj->nIdx = nIdx;` |
|    42783 |  1814 | `		}else{` |
|        - |  1815 | `			/* Extract variable contents */` |
|  2998884 |  1816 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2998884 |  1817 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2998884 |  1818 | `			if( bNullify && pObj ){` |
|      ! 0 |  1819 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1820 | `			}` |
|        - |  1821 | `		}` |
|  1542334 |  1822 | `	}else{` |
|        - |  1823 | `		/* Superglobal */` |
|       42 |  1824 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1825 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1826 | `	}` |
|  3084486 |  1827 | `	return pObj;` |
|  1542372 |  1828 |  |
|        - |  1829 | `/*` |
|        - |  1830 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1831 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1832 | ` */` |
|     2780 |  1833 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1834 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1835 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1836 | `	sxu32 nByte        /* zName length */` |
|        - |  1837 | `	)` |
|        2 |  1838 |  |
|        - |  1839 | `	SyHashEntry *pEntry;` |
|        - |  1840 | `	ph7_value *pValue;` |
|        - |  1841 | `	sxu32 nIdx;` |
|        - |  1842 | `	/* Query the superglobal table */` |
|     2782 |  1843 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2782 |  1844 | `	if( pEntry == 0 ){` |
|        - |  1845 | `		/* No such entry */` |
|      ! 0 |  1846 | `		return 0;` |
|        - |  1847 | `	}` |
|        - |  1848 | `	/* Extract the superglobal index in the global object pool */` |
|     2782 |  1849 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1850 | `	/* Extract the variable value  */` |
|     2782 |  1851 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2782 |  1852 | `	return pValue;` |
|     1392 |  1853 |  |
|        - |  1854 | `/*` |
|        - |  1855 | ` * Perform a raw hashmap insertion.` |
|        - |  1856 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1857 | ` */` |
|     2810 |  1858 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1859 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1860 | `	const char *zKey,   /* Entry key */` |
|        - |  1861 | `	int nKeylen,        /* zKey length*/` |
|        - |  1862 | `	const char *zData,  /* Entry data */` |
|        - |  1863 | `	int nLen            /* zData length */` |
|        - |  1864 | `	)` |
|        2 |  1865 |  |
|        - |  1866 | `	ph7_value sKey,sValue;` |
|        - |  1867 | `	sxi32 rc;` |
|     2812 |  1868 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2812 |  1869 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2812 |  1870 | `	if( zKey ){` |
|     2790 |  1871 | `		if( nKeylen < 0 ){` |
|     2738 |  1872 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1368 |  1873 | `		}` |
|     2790 |  1874 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1394 |  1875 | `	}` |
|     2812 |  1876 | `	if( zData ){` |
|     2812 |  1877 | `		if( nLen < 0 ){` |
|        - |  1878 | `			/* Compute length automatically */` |
|      144 |  1879 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1880 | `		}` |
|     2812 |  1881 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1405 |  1882 | `	}` |
|        - |  1883 | `	/* Perform the insertion */` |
|     2812 |  1884 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2812 |  1885 | `	PH7_MemObjRelease(&sKey);` |
|     2812 |  1886 | `	PH7_MemObjRelease(&sValue);` |
|     2812 |  1887 | `	return rc;` |
|        2 |  1888 |  |
|        - |  1889 | `/*` |
|        - |  1890 | ` * Configure a working virtual machine instance.` |
|        - |  1891 | ` *` |
|        - |  1892 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1893 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1894 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1895 | ` * The second argument to this function is an integer configuration option` |
|        - |  1896 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1897 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1898 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1899 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1900 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1901 | ` */` |
|    39946 |  1902 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1903 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1904 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1905 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1906 | `	)` |
|        2 |  1907 |  |
|    39948 |  1908 | `	sxi32 rc = SXRET_OK;` |
|    39948 |  1909 | `	switch(nOp){` |
|     1230 |  1910 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2462 |  1911 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2462 |  1912 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1913 | `		/* VM output consumer callback */` |
|        - |  1914 | `#ifdef UNTRUST` |
|        - |  1915 | `		if( xConsumer == 0 ){` |
|        - |  1916 | `			rc = SXERR_CORRUPT;` |
|        - |  1917 | `			break;` |
|        - |  1918 | `		}` |
|        - |  1919 | `#endif` |
|        - |  1920 | `		/* Install the output consumer */` |
|     2462 |  1921 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2462 |  1922 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2462 |  1923 | `		break;` |
|        - |  1924 | `							   }` |
|     1238 |  1925 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1926 | `		/* Import path */` |
|        - |  1927 | `		  const char *zPath;` |
|        - |  1928 | `		  SyString sPath;` |
|     2478 |  1929 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1930 | `#if defined(UNTRUST)` |
|        - |  1931 | `		  if( zPath == 0 ){` |
|        - |  1932 | `			  rc = SXERR_EMPTY;` |
|        - |  1933 | `			  break;` |
|        - |  1934 | `		  }` |
|        - |  1935 | `#endif` |
|     2478 |  1936 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1937 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1938 | `#ifdef __WINNT__` |
|        2 |  1939 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1940 | `#endif` |
|     4954 |  1941 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1942 | `		  /* Remove leading and trailing white spaces */` |
|     2478 |  1943 | `		  SyStringFullTrim(&sPath);` |
|     2478 |  1944 | `		  if( sPath.nByte > 0 ){` |
|        - |  1945 | `			  /* Store the path in the corresponding conatiner */` |
|     2478 |  1946 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1238 |  1947 | `		  }` |
|     2478 |  1948 | `		  break;` |
|        - |  1949 | `									 }` |
|     1238 |  1950 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1951 | `		/* Run-Time Error report */` |
|     2478 |  1952 | `		pVm->bErrReport = 1;` |
|     2478 |  1953 | `		break;` |
|      ! 0 |  1954 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1955 | `		/* Recursion depth */` |
|      ! 0 |  1956 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1957 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1958 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1959 | `		}` |
|      ! 0 |  1960 | `		break;` |
|        - |  1961 | `									   }` |
|      ! 0 |  1962 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1963 | `		/* VM output length in bytes */` |
|      ! 0 |  1964 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1965 | `#ifdef UNTRUST` |
|        - |  1966 | `		if( pOut == 0 ){` |
|        - |  1967 | `			rc = SXERR_CORRUPT;` |
|        - |  1968 | `			break;` |
|        - |  1969 | `		}` |
|        - |  1970 | `#endif` |
|      ! 0 |  1971 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1972 | `		break;` |
|        - |  1973 | `							   }` |
|        - |  1974 |  |
|    12380 |  1975 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1976 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1977 | `		/* Create a new superglobal/global variable */` |
|    24762 |  1978 | `		const char *zName = va_arg(ap,const char *);` |
|    24762 |  1979 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1980 | `		SyHashEntry *pEntry;` |
|        - |  1981 | `		ph7_value *pObj;` |
|        - |  1982 | `		sxu32 nByte;` |
|        - |  1983 | `		sxu32 nIdx;` |
|        - |  1984 | `#ifdef UNTRUST` |
|        - |  1985 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1986 | `			rc = SXERR_CORRUPT;` |
|        - |  1987 | `			break;` |
|        - |  1988 | `		}` |
|        - |  1989 | `#endif` |
|    24762 |  1990 | `		nByte = SyStrlen(zName);` |
|    24762 |  1991 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1992 | `			/* Check if the superglobal is already installed */` |
|    24762 |  1993 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12382 |  1994 | `		}else{` |
|        - |  1995 | `			/* Query the top active VM frame */` |
|      ! 0 |  1996 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1997 | `		}` |
|    24762 |  1998 | `		if( pEntry ){` |
|        - |  1999 | `			/* Variable already installed */` |
|      ! 0 |  2000 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2001 | `			/* Extract contents */` |
|      ! 0 |  2002 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2003 | `			if( pObj ){` |
|        - |  2004 | `				/* Overwrite old contents */` |
|      ! 0 |  2005 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2006 | `			}` |
|      ! 0 |  2007 | `		}else{` |
|        - |  2008 | `			/* Install a new variable */` |
|    24762 |  2009 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    24762 |  2010 | `			if( pObj == 0 ){` |
|      ! 0 |  2011 | `				rc = SXERR_MEM;` |
|      ! 0 |  2012 | `				break;` |
|        - |  2013 | `			}` |
|    24762 |  2014 | `			nIdx = pObj->nIdx;` |
|        - |  2015 | `			/* Copy value */` |
|    24762 |  2016 | `			PH7_MemObjStore(pValue,pObj);` |
|    24762 |  2017 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2018 | `				/* Install the superglobal */` |
|    24762 |  2019 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12382 |  2020 | `			}else{` |
|        - |  2021 | `				/* Install in the current frame */` |
|      ! 0 |  2022 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2023 | `			}` |
|    24762 |  2024 | `			if( rc == SXRET_OK ){` |
|        - |  2025 | `				SyHashEntry *pRef;` |
|    24762 |  2026 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    24762 |  2027 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12382 |  2028 | `				}else{` |
|      ! 0 |  2029 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2030 | `				}` |
|        - |  2031 | `				/* Install in the reference table */` |
|    24762 |  2032 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    24762 |  2033 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2034 | `					/* Register in the $GLOBALS array */` |
|    24762 |  2035 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12380 |  2036 | `				}` |
|    12380 |  2037 | `			}` |
|        - |  2038 | `		}` |
|    24762 |  2039 | `		break;` |
|        - |  2040 | `									}` |
|     1368 |  2041 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2042 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2043 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2044 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2045 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2046 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2047 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2738 |  2048 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2738 |  2049 | `		const char *zValue = va_arg(ap,const char *);` |
|     2738 |  2050 | `		int nLen = va_arg(ap,int);` |
|        - |  2051 | `		ph7_hashmap *pMap;` |
|        - |  2052 | `		ph7_value *pValue;` |
|     2738 |  2053 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2054 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2055 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2737 |  2056 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2057 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2058 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2736 |  2059 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2060 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2061 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2736 |  2062 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2063 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2064 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2736 |  2065 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2066 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2067 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2736 |  2068 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2069 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2070 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2071 | `		}else{` |
|        - |  2072 | `			/* Extract the $_SERVER superglobal */` |
|     2736 |  2073 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2074 | `		}` |
|     2738 |  2075 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2076 | `			/* No such entry */` |
|      ! 0 |  2077 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2078 | `			break;` |
|        - |  2079 | `		}` |
|        - |  2080 | `		/* Point to the hashmap */` |
|     2738 |  2081 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2082 | `		/* Perform the insertion */` |
|     2738 |  2083 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2738 |  2084 | `		break;` |
|        - |  2085 | `								   }` |
|       11 |  2086 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2087 | `		/* Script arguments */` |
|       24 |  2088 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2089 | `		ph7_hashmap *pMap;` |
|        - |  2090 | `		ph7_value *pValue;` |
|        - |  2091 | `		sxu32 n;` |
|       24 |  2092 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2093 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2094 | `			break;` |
|        - |  2095 | `		}` |
|        - |  2096 | `		/* Extract the $argv array */` |
|       24 |  2097 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2098 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2099 | `			/* No such entry */` |
|      ! 0 |  2100 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2101 | `			break;` |
|        - |  2102 | `		}` |
|        - |  2103 | `		/* Point to the hashmap */` |
|       24 |  2104 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2105 | `		/* Perform the insertion */` |
|       24 |  2106 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2107 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2108 | `		if( rc == SXRET_OK ){` |
|       24 |  2109 | `			if( pMap->nEntry > 1 ){` |
|        - |  2110 | `				/* Append space separator first */` |
|       18 |  2111 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2112 | `			}` |
|       24 |  2113 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2114 | `		}` |
|       24 |  2115 | `		break;` |
|        - |  2116 | `								  }` |
|      ! 0 |  2117 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2118 | `		/* error_log() consumer */` |
|      ! 0 |  2119 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2120 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2121 | `		break;` |
|        - |  2122 | `										}` |
|      ! 0 |  2123 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2124 | `		/* Script return value */` |
|      ! 0 |  2125 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2126 | `#ifdef UNTRUST` |
|        - |  2127 | `		if( ppValue == 0 ){` |
|        - |  2128 | `			rc = SXERR_CORRUPT;` |
|        - |  2129 | `			break;` |
|        - |  2130 | `		}` |
|        - |  2131 | `#endif` |
|      ! 0 |  2132 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2133 | `		break;` |
|        - |  2134 | `								   }` |
|     2476 |  2135 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2136 | `		/* Register an IO stream device */` |
|     4954 |  2137 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2138 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7428 |  2139 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4954 |  2140 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2141 | `				/* Invalid stream */` |
|      ! 0 |  2142 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2143 | `				break;` |
|        - |  2144 | `		}` |
|     4954 |  2145 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2146 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2478 |  2147 | `			pVm->pDefStream = pStream;` |
|     1238 |  2148 | `		}` |
|        - |  2149 | `		/* Insert in the appropriate container */` |
|     4954 |  2150 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4954 |  2151 | `		break;` |
|        - |  2152 | `								  }` |
|        8 |  2153 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2154 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2155 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2156 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2157 | `#ifdef UNTRUST` |
|        - |  2158 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2159 | `			rc = SXERR_CORRUPT;` |
|        - |  2160 | `			break;` |
|        - |  2161 | `		}` |
|        - |  2162 | `#endif` |
|       16 |  2163 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2164 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2165 | `		break;` |
|        - |  2166 | `									   }` |
|        8 |  2167 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2168 | `		/* Raw HTTP request*/` |
|       16 |  2169 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2170 | `		int nByte = va_arg(ap,int);` |
|       16 |  2171 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2172 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2173 | `			break;` |
|        - |  2174 | `		}` |
|       16 |  2175 | `		if( nByte < 0 ){` |
|        - |  2176 | `			/* Compute length automatically */` |
|      ! 0 |  2177 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2178 | `		}` |
|        - |  2179 | `		/* Process the request */` |
|       16 |  2180 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2181 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2182 | `		if( rc == SXRET_OK ){` |
|       16 |  2183 | `			pVm->bHttpContext = 1;` |
|        8 |  2184 | `		}` |
|       16 |  2185 | `		break;` |
|        - |  2186 | `									}` |
|        8 |  2187 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2188 | `		/* Extract HTTP response status code */` |
|       16 |  2189 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2190 | `		if( pStatus ){` |
|       16 |  2191 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2192 | `		}` |
|       16 |  2193 | `		break;` |
|        - |  2194 | `										}` |
|        8 |  2195 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2196 | `		/* Iterate response headers via callback */` |
|        - |  2197 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2198 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2199 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2200 | `		if( xCallback ){` |
|       16 |  2201 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2202 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2203 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2204 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2205 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2206 | `							   pUserData);` |
|       12 |  2207 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2208 | `					break;` |
|        - |  2209 | `				}` |
|        6 |  2210 | `			}` |
|        8 |  2211 | `		}` |
|       16 |  2212 | `		break;` |
|        - |  2213 | `										 }` |
|      ! 0 |  2214 | `	default:` |
|        - |  2215 | `		/* Unknown configuration option */` |
|      ! 0 |  2216 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2217 | `		break;` |
|        - |  2218 | `	}` |
|    39948 |  2219 | `	return rc;` |
|        2 |  2220 |  |
|        - |  2221 | `/* Forward declaration */` |
|        - |  2222 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2223 | `/*` |
|        - |  2224 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2225 | ` * format.` |
|        - |  2226 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2227 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2228 | ` * (STDOUT).` |
|        - |  2229 | ` */` |
|        2 |  2230 | `static sxi32 VmByteCodeDump(` |
|        - |  2231 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2232 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2233 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2234 | `	)` |
|        1 |  2235 |  |
|        - |  2236 | `	static const char zDump[] = {` |
|        - |  2237 | `		"====================================================\n"` |
|        - |  2238 | `		"PH7 VM Dump\n"` |
|        - |  2239 | `		"====================================================\n"` |
|        - |  2240 | `	};` |
|        - |  2241 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2242 | `	sxi32 rc = SXRET_OK;` |
|        - |  2243 | `	sxu32 n;` |
|        - |  2244 | `	/* Point to the PH7 instructions */` |
|        3 |  2245 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2246 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2247 | `	n = 0;` |
|        3 |  2248 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2249 | `	/* Dump instructions */` |
|        7 |  2250 | `	for(;;){` |
|       15 |  2251 | `		if( pInstr >= pEnd ){` |
|        - |  2252 | `			/* No more instructions */` |
|        3 |  2253 | `			break;` |
|        - |  2254 | `		}` |
|        - |  2255 | `		/* Format and call the consumer callback */` |
|       19 |  2256 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2257 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2258 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2259 | `		if( rc != SXRET_OK ){` |
|        - |  2260 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2261 | `			return rc;` |
|        - |  2262 | `		}` |
|       13 |  2263 | `		++n;` |
|       13 |  2264 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2265 | `	}` |
|        3 |  2266 | `	return rc;` |
|        2 |  2267 |  |
|        - |  2268 | `/* Forward declaration */` |
|        - |  2269 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2270 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2271 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2272 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2273 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2274 | `/*` |
|        - |  2275 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2276 | ` * consumer callback.` |
|        - |  2277 | ` */` |
|      544 |  2278 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2279 |  |
|      545 |  2280 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      545 |  2281 | `	sxi32 rc = SXRET_OK;` |
|        - |  2282 | `	/* Append a new line */` |
|        - |  2283 | `#ifdef __WINNT__` |
|        1 |  2284 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2285 | `#else` |
|      544 |  2286 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2287 | `#endif` |
|        - |  2288 | `	/* Invoke the output consumer callback */` |
|      545 |  2289 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      545 |  2290 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      545 |  2291 | `	return rc;` |
|        1 |  2292 |  |
|        - |  2293 | `/*` |
|        - |  2294 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2295 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2296 | ` * information.` |
|        - |  2297 | ` */` |
|      132 |  2298 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2299 |  |
|      134 |  2300 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2301 | `		ph7_value apArg[4];` |
|        - |  2302 | `		ph7_value *apArgPtr[4];` |
|        - |  2303 | `		ph7_value sResult;` |
|        - |  2304 | `		SyString sErr;` |
|        - |  2305 | `		/* Prepare arguments */` |
|       61 |  2306 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2307 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2308 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2309 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2310 | `		if( pFile ){` |
|       61 |  2311 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2312 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2313 | `		}else{` |
|      ! 0 |  2314 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2315 | `		}` |
|       61 |  2316 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2317 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2318 | `		/* Set up pointer array */` |
|       61 |  2319 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2320 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2321 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2322 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2323 | `		/* Call the handler */` |
|       61 |  2324 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2325 | `		/* Check return value */` |
|       61 |  2326 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2327 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2328 | `		}` |
|        - |  2329 | `		/* Release */` |
|       61 |  2330 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2331 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2332 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2333 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2334 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2335 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2336 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2337 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2338 | `	}` |
|        - |  2339 | `	/* No handler, always call error handler */` |
|       73 |  2340 | `	return TRUE;` |
|       68 |  2341 |  |
|       96 |  2342 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2343 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2344 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2345 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2346 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2347 | `	)` |
|        2 |  2348 |  |
|       98 |  2349 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2350 | `	SyString *pFile;` |
|        - |  2351 | `	char *zErr;` |
|       98 |  2352 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2353 | `	if( !pVm->bErrReport ){` |
|        - |  2354 | `		/* Don't bother reporting errors */` |
|        3 |  2355 | `		return SXRET_OK;` |
|        - |  2356 | `	}` |
|        - |  2357 | `	/* Reset the working buffer */` |
|       96 |  2358 | `	SyBlobReset(pWorker);` |
|        - |  2359 | `	/* Peek the processed file if available */` |
|       96 |  2360 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2361 | `	if( pFile ){` |
|        - |  2362 | `		/* Append file name */` |
|       96 |  2363 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2364 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2365 | `	}` |
|        - |  2366 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2367 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2368 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2369 | `	 * E_DEPRECATED). */` |
|       96 |  2370 | `	zErr = "Error:  ";` |
|       96 |  2371 | `	switch(iErr){` |
|       18 |  2372 | `	case PH7_CTX_WARNING:` |
|       38 |  2373 | `		zErr = "Warning:  ";` |
|       38 |  2374 | `		break;` |
|        6 |  2375 | `	case PH7_CTX_NOTICE:` |
|       14 |  2376 | `		zErr = "Notice:  ";` |
|       12 |  2377 | `		break;` |
|       23 |  2378 | `	default:` |
|        - |  2379 | `		/* keep iErr unchanged */` |
|       46 |  2380 | `		break;` |
|        - |  2381 | `	}` |
|       96 |  2382 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2383 | `	if( pFuncName ){` |
|        - |  2384 | `		/* Append function name first */` |
|       23 |  2385 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2386 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2387 | `	}` |
|       96 |  2388 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2389 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2390 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2391 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2392 | `	}` |
|       96 |  2393 | `	return rc;` |
|       50 |  2394 |  |
|        - |  2395 | `/*` |
|        - |  2396 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2397 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2398 | ` * information.` |
|        - |  2399 | ` */` |
|       38 |  2400 | `static sxi32 VmThrowErrorAp(` |
|        - |  2401 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2402 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2403 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2404 | `	const char *zFormat, /* Format message */` |
|        - |  2405 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2406 | `	)` |
|        2 |  2407 |  |
|       40 |  2408 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2409 | `	SyBlob sMsg;` |
|        - |  2410 | `	SyString *pFile;` |
|        - |  2411 | `	char *zErr;` |
|       40 |  2412 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2413 | `	if( !pVm->bErrReport ){` |
|        - |  2414 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2415 | `		return SXRET_OK;` |
|        - |  2416 | `	}` |
|        - |  2417 | `	/* Reset the working buffer */` |
|       40 |  2418 | `	SyBlobReset(pWorker);` |
|        - |  2419 | `	/* Peek the processed file if available */` |
|       40 |  2420 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2421 | `	if( pFile ){` |
|        - |  2422 | `		/* Append file name */` |
|       40 |  2423 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2424 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2425 | `	}` |
|        - |  2426 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2427 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2428 | `	 * the correct errno value. */` |
|       40 |  2429 | `	zErr = "Error:  ";` |
|       40 |  2430 | `	switch(iErr){` |
|        4 |  2431 | `	case PH7_CTX_WARNING:` |
|        9 |  2432 | `		zErr = "Warning:  ";` |
|        9 |  2433 | `		break;` |
|        3 |  2434 | `	case PH7_CTX_NOTICE:` |
|        7 |  2435 | `		zErr = "Notice:  ";` |
|        6 |  2436 | `		break;` |
|       12 |  2437 | `	default:` |
|        - |  2438 | `		/* do not change iErr */` |
|       24 |  2439 | `		break;` |
|        - |  2440 | `	}` |
|       40 |  2441 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2442 | `	if( pFuncName ){` |
|        - |  2443 | `		/* Append function name first */` |
|       26 |  2444 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2445 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2446 | `	}` |
|        - |  2447 | `	/* Format the raw message */` |
|       40 |  2448 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2449 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2450 | `	/* Check if a user error handler is installed */` |
|       40 |  2451 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2452 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2453 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2454 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2455 | `	}` |
|       40 |  2456 | `	SyBlobRelease(&sMsg);` |
|       40 |  2457 | `	return rc;` |
|       21 |  2458 |  |
|        - |  2459 | `/*` |
|        - |  2460 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2461 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2462 | ` * information.` |
|        - |  2463 | ` * ------------------------------------` |
|        - |  2464 | ` * Simple boring wrapper function.` |
|        - |  2465 | ` * ------------------------------------` |
|        - |  2466 | ` */` |
|       14 |  2467 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2468 |  |
|        - |  2469 | `	va_list ap;` |
|        - |  2470 | `	sxi32 rc;` |
|       15 |  2471 | `	va_start(ap,zFormat);` |
|       15 |  2472 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2473 | `	va_end(ap);` |
|       15 |  2474 | `	return rc;` |
|        1 |  2475 |  |
|        - |  2476 | `/*` |
|        - |  2477 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2478 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2479 | ` * information.` |
|        - |  2480 | ` * ------------------------------------` |
|        - |  2481 | ` * Simple boring wrapper function.` |
|        - |  2482 | ` * ------------------------------------` |
|        - |  2483 | ` */` |
|       24 |  2484 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2485 |  |
|        - |  2486 | `	sxi32 rc;` |
|       26 |  2487 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2488 | `	return rc;` |
|        2 |  2489 |  |
|        - |  2490 | `/*` |
|        - |  2491 | ` * Resolve function context from the current frame.` |
|        - |  2492 | ` */` |
|      934 |  2493 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2494 |  |
|        - |  2495 | `	VmFrame *pFrame;` |
|        - |  2496 | `	ph7_vm_func *pFunc;` |
|      935 |  2497 | `	*pzFuncName = 0;` |
|      935 |  2498 | `	*pnFuncLen = 0;` |
|      935 |  2499 | `	pFrame = pVm->pFrame;` |
|      935 |  2500 | `	if( pFrame == 0 ){` |
|      ! 0 |  2501 | `		return;` |
|        - |  2502 | `	}` |
|      935 |  2503 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2504 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2505 | `		return;` |
|        - |  2506 | `	}` |
|        7 |  2507 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2508 | `	if( pFunc == 0 ){` |
|      ! 0 |  2509 | `		return;` |
|        - |  2510 | `	}` |
|        7 |  2511 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2512 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2513 |  |
|        - |  2514 | `/*` |
|        - |  2515 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2516 | ` */` |
|      470 |  2517 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2518 |  |
|        - |  2519 | `	SyBlob sOut;` |
|        - |  2520 | `	SyString *pFile;` |
|      471 |  2521 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2522 | `		return PH7_OK;` |
|        - |  2523 | `	}` |
|      471 |  2524 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2525 | `		zClass = "Exception";` |
|      ! 0 |  2526 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2527 | `	}` |
|      471 |  2528 | `	if( zMsg == 0 ){` |
|      ! 0 |  2529 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2530 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2531 | `	}` |
|      471 |  2532 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2533 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2534 | `	}` |
|      471 |  2535 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2536 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2537 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2538 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2539 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2540 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2541 | `	if( pFile ){` |
|      471 |  2542 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2543 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2544 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2545 | `	}` |
|      471 |  2546 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2547 | `	if( pFile ){` |
|      471 |  2548 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2549 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2550 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2551 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2552 | `		}else{` |
|      465 |  2553 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2554 | `		}` |
|      235 |  2555 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2556 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2557 | `	}else{` |
|      ! 0 |  2558 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2559 | `	}` |
|      471 |  2560 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2561 | `	if( pFile ){` |
|      471 |  2562 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2563 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2564 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2565 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2566 | `	}` |
|      471 |  2567 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2568 | `	SyBlobRelease(&sOut);` |
|      471 |  2569 | `	return PH7_ABORT;` |
|      236 |  2570 |  |
|        - |  2571 | `/*` |
|        - |  2572 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2573 | ` */` |
|      472 |  2574 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2575 |  |
|        - |  2576 | `	ph7_vm *pVm;` |
|        - |  2577 | `	ph7_class *pClass;` |
|        - |  2578 | `	ph7_class_instance *pThis;` |
|        - |  2579 | `	ph7_class_method *pCons;` |
|        - |  2580 | `	ph7_value sArg;` |
|        - |  2581 | `	ph7_value *apArg[1];` |
|        - |  2582 | `	SyBlob sMsg;` |
|        - |  2583 | `	SyString sMsgStr;` |
|        - |  2584 | `	VmFrame *pFrame;` |
|        - |  2585 | `	va_list ap;` |
|        - |  2586 | `	sxi32 rc;` |
|        - |  2587 |  |
|      474 |  2588 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2589 | `		return PH7_ABORT;` |
|        - |  2590 | `	}` |
|      474 |  2591 | `	pVm = pCtx->pVm;` |
|      474 |  2592 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2593 | `		zClass = "Error";` |
|      ! 0 |  2594 | `	}` |
|      474 |  2595 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      474 |  2596 | `	if( pClass == 0 ){` |
|      ! 0 |  2597 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2598 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2599 | `			zClass` |
|        - |  2600 | `			);` |
|        - |  2601 | `	}` |
|      474 |  2602 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      474 |  2603 | `	if( pThis == 0 ){` |
|      ! 0 |  2604 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2605 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2606 | `			);` |
|        - |  2607 | `	}` |
|        - |  2608 |  |
|      474 |  2609 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      474 |  2610 | `	va_start(ap,zFormat);` |
|      474 |  2611 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      474 |  2612 | `	va_end(ap);` |
|        - |  2613 |  |
|      474 |  2614 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      474 |  2615 | `	if( pCons ){` |
|      474 |  2616 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      474 |  2617 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      474 |  2618 | `		apArg[0] = &sArg;` |
|      474 |  2619 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      474 |  2620 | `		PH7_MemObjRelease(&sArg);` |
|      236 |  2621 | `	}` |
|      474 |  2622 | `	SyBlobRelease(&sMsg);` |
|        - |  2623 |  |
|      474 |  2624 | `	pFrame = pVm->pFrame;` |
|      474 |  2625 | `	if( pFrame ){` |
|      474 |  2626 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      474 |  2627 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      236 |  2628 | `	}` |
|      474 |  2629 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      474 |  2630 | `	PH7_ClassInstanceUnref(pThis);` |
|      474 |  2631 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2632 | `		return PH7_ABORT;` |
|        - |  2633 | `	}` |
|       12 |  2634 | `	return PH7_EXCEPTION;` |
|      238 |  2635 |  |
|        - |  2636 | `/*` |
|        - |  2637 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2638 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2639 | ` */` |
|      ! 0 |  2640 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2641 |  |
|        - |  2642 | `	ph7_vm *pVm;` |
|        - |  2643 | `	SyBlob sMsg;` |
|      ! 0 |  2644 | `	const char *zFuncName = 0;` |
|      ! 0 |  2645 | `	int nFuncLen = 0;` |
|        - |  2646 | `	va_list ap;` |
|        - |  2647 | `	sxi32 rc;` |
|        - |  2648 |  |
|      ! 0 |  2649 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2650 | `		return PH7_OK;` |
|        - |  2651 | `	}` |
|      ! 0 |  2652 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2653 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2654 | `		zClass = "Error";` |
|      ! 0 |  2655 | `	}` |
|        - |  2656 |  |
|      ! 0 |  2657 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2658 |  |
|      ! 0 |  2659 | `	va_start(ap,zFormat);` |
|      ! 0 |  2660 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2661 | `	va_end(ap);` |
|        - |  2662 |  |
|      ! 0 |  2663 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2664 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2665 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2666 | `	}` |
|      ! 0 |  2667 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2668 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2669 | `	}` |
|      ! 0 |  2670 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2671 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2672 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2673 | `	return rc;` |
|      ! 0 |  2674 |  |
|        - |  2675 | `/*` |
|        - |  2676 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2677 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2678 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2679 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2680 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2681 | ` * when VmByteCodeExec returns.` |
|        - |  2682 | ` */` |
|      132 |  2683 | `static sxi32 VmSuspendCtx(` |
|        - |  2684 | `	ph7_vm *pVm,` |
|        - |  2685 | `	ph7_exec_ctx *pCtx,` |
|        - |  2686 | `	sxi32 pc,` |
|        - |  2687 | `	sxi32 nTos` |
|        - |  2688 | `	)` |
|        1 |  2689 |  |
|       66 |  2690 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      133 |  2691 | `	pCtx->pc = pc;` |
|      133 |  2692 | `	pCtx->nTos = nTos;` |
|      133 |  2693 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      133 |  2694 | `	return PH7_SUSPEND;` |
|        1 |  2695 |  |
|        - |  2696 | `/*` |
|        - |  2697 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2698 | ` *` |
|        - |  2699 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2700 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2701 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2702 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2703 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2704 | ` * then the program execution is halted.` |
|        - |  2705 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2706 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2707 | ` * or to reset the VM to it's initial state.` |
|        - |  2708 | ` */` |
|    32612 |  2709 | `static sxi32 VmByteCodeExec(` |
|        - |  2710 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2711 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2712 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2713 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2714 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2715 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2716 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2717 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2718 | `	)` |
|        2 |  2719 |  |
|        - |  2720 | `	VmInstr *pInstr;` |
|        - |  2721 | `	ph7_value *pTos;` |
|        - |  2722 | `	SySet aArg;` |
|        - |  2723 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2724 | `	sxi32 pc;` |
|        - |  2725 | `	sxi32 rc;` |
|        - |  2726 | `	/* Argument container */` |
|    32614 |  2727 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32614 |  2728 | `	if( nTos < 0 ){` |
|    30598 |  2729 | `		pTos = &pStack[-1];` |
|    15300 |  2730 | `	}else{` |
|     2018 |  2731 | `		pTos = &pStack[nTos];` |
|        - |  2732 | `	}` |
|    32614 |  2733 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32614 |  2734 | `	pc = nPc;` |
|        - |  2735 | `	/* Execute as much as we can */` |
|  4943502 |  2736 | `	for(;;){` |
|        - |  2737 | `		/* Fetch the instruction to execute */` |
|  9886302 |  2738 | `		pInstr = &aInstr[pc];` |
|  9886302 |  2739 | `		rc = SXRET_OK;` |
|        - |  2740 | `/*` |
|        - |  2741 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2742 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2743 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2744 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2745 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2746 | ` */` |
|  9886302 |  2747 | `		switch(pInstr->iOp){` |
|        - |  2748 | `/*` |
|        - |  2749 | ` * DONE: P1 * *` |
|        - |  2750 | ` *` |
|        - |  2751 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2752 | ` * and return immediately.` |
|        - |  2753 | ` */` |
|    15995 |  2754 | `case PH7_OP_DONE:` |
|    31992 |  2755 | `	if( pInstr->iP1 ){` |
|        - |  2756 | `#ifdef UNTRUST` |
|        - |  2757 | `		if( pTos < pStack ){` |
|        - |  2758 | `			goto Abort;` |
|        - |  2759 | `		}` |
|        - |  2760 | `#endif` |
|    18544 |  2761 | `		if( pLastRef ){` |
|    12102 |  2762 | `			*pLastRef = pTos->nIdx;` |
|     6050 |  2763 | `		}` |
|    18544 |  2764 | `		if( pResult ){` |
|        - |  2765 | `			/* Execution result */` |
|    17608 |  2766 | `			PH7_MemObjStore(pTos,pResult);` |
|     8803 |  2767 | `		}` |
|    18544 |  2768 | `		VmPopOperand(&pTos,1);` |
|    22721 |  2769 | `	}else if( pLastRef ){` |
|        - |  2770 | `		/* Nothing referenced */` |
|      988 |  2771 | `		*pLastRef = SXU32_HIGH;` |
|      493 |  2772 | `	}` |
|        - |  2773 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2774 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2775 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2776 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2777 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2778 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2779 | `	 * block can override it.` |
|        - |  2780 | `	 */` |
|    31994 |  2781 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2782 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2783 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2784 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2785 | `		pExc->pFrame = 0;` |
|        3 |  2786 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2787 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2788 | `			pExc->iFinallyDone = 1;` |
|        - |  2789 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2790 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2791 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2792 | `				goto Abort;` |
|        - |  2793 | `			}` |
|        1 |  2794 | `		}` |
|        1 |  2795 | `	}` |
|    31992 |  2796 | `	goto Done;` |
|        - |  2797 | `/*` |
|        - |  2798 | ` * HALT: P1 * *` |
|        - |  2799 | ` *` |
|        - |  2800 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2801 | ` * and abort immediately.` |
|        - |  2802 | ` */` |
|        4 |  2803 | `case PH7_OP_HALT:` |
|        9 |  2804 | `	if( pInstr->iP1 ){` |
|        - |  2805 | `#ifdef UNTRUST` |
|        - |  2806 | `		if( pTos < pStack ){` |
|        - |  2807 | `			goto Abort;` |
|        - |  2808 | `		}` |
|        - |  2809 | `#endif` |
|        9 |  2810 | `		if( pLastRef ){` |
|      ! 0 |  2811 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2812 | `		}` |
|        9 |  2813 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2814 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2815 | `				/* Output the exit message */` |
|        7 |  2816 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2817 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2818 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2819 | `			}` |
|        7 |  2820 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2821 | `			/* Record exit status */` |
|        5 |  2822 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2823 | `		}` |
|        9 |  2824 | `		VmPopOperand(&pTos,1);` |
|        4 |  2825 | `	}else if( pLastRef ){` |
|        - |  2826 | `		/* Nothing referenced */` |
|      ! 0 |  2827 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2828 | `	}` |
|        - |  2829 | `	/* Check if we're in an included file context */` |
|        9 |  2830 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2831 | `		/* Terminate the entire process */` |
|        9 |  2832 | `		exit(pVm->iExitStatus);` |
|        - |  2833 | `	}` |
|      ! 0 |  2834 | `	goto Abort;` |
|        - |  2835 | `/*` |
|        - |  2836 | ` * JMP: * P2 *` |
|        - |  2837 | ` *` |
|        - |  2838 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2839 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2840 | ` */` |
|   213102 |  2841 | `case PH7_OP_JMP:` |
|   426250 |  2842 | `	pc = pInstr->iP2 - 1;` |
|   426250 |  2843 | `	break;` |
|        - |  2844 | `/*` |
|        - |  2845 | ` * JZ: P1 P2 *` |
|        - |  2846 | ` *` |
|        - |  2847 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2848 | ` * entry in the stack if P1 is zero.` |
|        - |  2849 | ` */` |
|   497780 |  2850 | `case PH7_OP_JZ:` |
|        - |  2851 | `#ifdef UNTRUST` |
|        - |  2852 | `	if( pTos < pStack ){` |
|        - |  2853 | `		goto Abort;` |
|        - |  2854 | `	}` |
|        - |  2855 | `#endif` |
|        - |  2856 | `	/* Get a boolean value */` |
|   995650 |  2857 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2858 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2859 | `	}` |
|   995650 |  2860 | `	if( !pTos->x.iVal ){` |
|        - |  2861 | `		/* Take the jump */` |
|   502388 |  2862 | `		pc = pInstr->iP2 - 1;` |
|   251193 |  2863 | `	}` |
|   995650 |  2864 | `	if( !pInstr->iP1 ){` |
|   793264 |  2865 | `		VmPopOperand(&pTos,1);` |
|   396653 |  2866 | `	}` |
|   995650 |  2867 | `	break;` |
|        - |  2868 | `/*` |
|        - |  2869 | ` * JNZ: P1 P2 *` |
|        - |  2870 | ` *` |
|        - |  2871 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2872 | ` * entry in the stack if P1 is zero.` |
|        - |  2873 | ` */` |
|    53397 |  2874 | `case PH7_OP_JNZ:` |
|        - |  2875 | `#ifdef UNTRUST` |
|        - |  2876 | `	if( pTos < pStack ){` |
|        - |  2877 | `		goto Abort;` |
|        - |  2878 | `	}` |
|        - |  2879 | `#endif` |
|        - |  2880 | `	/* Get a boolean value */` |
|   106796 |  2881 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2882 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2883 | `	}` |
|   106796 |  2884 | `	if( pTos->x.iVal ){` |
|        - |  2885 | `		/* Take the jump */` |
|     4430 |  2886 | `		pc = pInstr->iP2 - 1;` |
|     2214 |  2887 | `	}` |
|   106796 |  2888 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2889 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2890 | `	}` |
|   106796 |  2891 | `	break;` |
|        - |  2892 | `/*` |
|        - |  2893 | ` * NOOP: * * *` |
|        - |  2894 | ` *` |
|        - |  2895 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2896 | ` * destination.` |
|        - |  2897 | ` */` |
|      ! 0 |  2898 | `case PH7_OP_NOOP:` |
|      ! 0 |  2899 | `	break;` |
|        - |  2900 | `/*` |
|        - |  2901 | ` * POP: P1 * *` |
|        - |  2902 | ` *` |
|        - |  2903 | ` * Pop P1 elements from the operand stack.` |
|        - |  2904 | ` */` |
|   388975 |  2905 | `case PH7_OP_POP: {` |
|   777996 |  2906 | `	sxi32 n = pInstr->iP1;` |
|   777996 |  2907 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2908 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2909 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2910 | `	}` |
|   777996 |  2911 | `	VmPopOperand(&pTos,n);` |
|   777996 |  2912 | `	break;` |
|        - |  2913 | `				 }` |
|        - |  2914 | `/*` |
|        - |  2915 | ` * DUP: * * *` |
|        - |  2916 | ` *` |
|        - |  2917 | ` * Duplicate the top of the stack.` |
|        - |  2918 | ` */` |
|       35 |  2919 | `case PH7_OP_DUP:` |
|        - |  2920 | `#ifdef UNTRUST` |
|        - |  2921 | `	if( pTos < pStack ){` |
|        - |  2922 | `		goto Abort;` |
|        - |  2923 | `	}` |
|        - |  2924 | `#endif` |
|       72 |  2925 | `	pTos++;` |
|       72 |  2926 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2927 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2928 | `	break;` |
|        - |  2929 | `/*` |
|        - |  2930 | ` * NSSWITCH: * * P3` |
|        - |  2931 | ` *` |
|        - |  2932 | ` * Switch the active namespace at runtime.` |
|        - |  2933 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2934 | ` */` |
|     6456 |  2935 | `case PH7_OP_NSSWITCH:` |
|    12914 |  2936 | `	SyBlobReset(&pVm->sNamespace);` |
|    12914 |  2937 | `	if( pInstr->p3 ){` |
|       51 |  2938 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2939 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2940 | `	}` |
|    12914 |  2941 | `	break;` |
|        - |  2942 | `/*` |
|        - |  2943 | ` * CVT_INT: * * *` |
|        - |  2944 | ` *` |
|        - |  2945 | ` * Force the top of the stack to be an integer.` |
|        - |  2946 | ` */` |
|       35 |  2947 | `case PH7_OP_CVT_INT:` |
|        - |  2948 | `#ifdef UNTRUST` |
|        - |  2949 | `	if( pTos < pStack ){` |
|        - |  2950 | `		goto Abort;` |
|        - |  2951 | `	}` |
|        - |  2952 | `#endif` |
|       72 |  2953 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2954 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2955 | `	}` |
|        - |  2956 | `	/* Invalidate any prior representation */` |
|       72 |  2957 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2958 | `	break;` |
|        - |  2959 | `/*` |
|        - |  2960 | ` * CVT_REAL: * * *` |
|        - |  2961 | ` *` |
|        - |  2962 | ` * Force the top of the stack to be a real.` |
|        - |  2963 | ` */` |
|        4 |  2964 | `case PH7_OP_CVT_REAL:` |
|        - |  2965 | `#ifdef UNTRUST` |
|        - |  2966 | `	if( pTos < pStack ){` |
|        - |  2967 | `		goto Abort;` |
|        - |  2968 | `	}` |
|        - |  2969 | `#endif` |
|        9 |  2970 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2971 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2972 | `	}` |
|        - |  2973 | `	/* Invalidate any prior representation */` |
|        9 |  2974 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2975 | `	break;` |
|        - |  2976 | `/*` |
|        - |  2977 | ` * CVT_STR: * * *` |
|        - |  2978 | ` *` |
|        - |  2979 | ` * Force the top of the stack to be a string.` |
|        - |  2980 | ` */` |
|      146 |  2981 | `case PH7_OP_CVT_STR:` |
|        - |  2982 | `#ifdef UNTRUST` |
|        - |  2983 | `	if( pTos < pStack ){` |
|        - |  2984 | `		goto Abort;` |
|        - |  2985 | `	}` |
|        - |  2986 | `#endif` |
|      294 |  2987 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2988 | `		PH7_MemObjToString(pTos);` |
|      146 |  2989 | `	}` |
|      294 |  2990 | `	break;` |
|        - |  2991 | `/*` |
|        - |  2992 | ` * CVT_BOOL: * * *` |
|        - |  2993 | ` *` |
|        - |  2994 | ` * Force the top of the stack to be a boolean.` |
|        - |  2995 | ` */` |
|        5 |  2996 | `case PH7_OP_CVT_BOOL:` |
|        - |  2997 | `#ifdef UNTRUST` |
|        - |  2998 | `	if( pTos < pStack ){` |
|        - |  2999 | `		goto Abort;` |
|        - |  3000 | `	}` |
|        - |  3001 | `#endif` |
|       11 |  3002 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3003 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3004 | `	}` |
|       11 |  3005 | `	break;` |
|        - |  3006 | `/*` |
|        - |  3007 | ` * CVT_NULL: * * *` |
|        - |  3008 | ` *` |
|        - |  3009 | ` * Nullify the top of the stack.` |
|        - |  3010 | ` */` |
|        3 |  3011 | `case PH7_OP_CVT_NULL:` |
|        - |  3012 | `#ifdef UNTRUST` |
|        - |  3013 | `	if( pTos < pStack ){` |
|        - |  3014 | `		goto Abort;` |
|        - |  3015 | `	}` |
|        - |  3016 | `#endif` |
|        7 |  3017 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3018 | `	break;` |
|        - |  3019 | `/*` |
|        - |  3020 | ` * CVT_NUMC: * * *` |
|        - |  3021 | ` *` |
|        - |  3022 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3023 | ` */` |
|      ! 0 |  3024 | `case PH7_OP_CVT_NUMC:` |
|        - |  3025 | `#ifdef UNTRUST` |
|        - |  3026 | `	if( pTos < pStack ){` |
|        - |  3027 | `		goto Abort;` |
|        - |  3028 | `	}` |
|        - |  3029 | `#endif` |
|        - |  3030 | `	/* Force a numeric cast */` |
|      ! 0 |  3031 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3032 | `	break;` |
|        - |  3033 | `/*` |
|        - |  3034 | ` * CVT_ARRAY: * * *` |
|        - |  3035 | ` *` |
|        - |  3036 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3037 | ` */` |
|       10 |  3038 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3039 | `#ifdef UNTRUST` |
|        - |  3040 | `	if( pTos < pStack ){` |
|        - |  3041 | `		goto Abort;` |
|        - |  3042 | `	}` |
|        - |  3043 | `#endif` |
|        - |  3044 | `	/* Force a hashmap cast */` |
|       21 |  3045 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3046 | `	if( rc != SXRET_OK ){` |
|        - |  3047 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3048 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3049 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3050 | `	}` |
|       21 |  3051 | `	break;` |
|        - |  3052 | `/*` |
|        - |  3053 | ` * CVT_OBJ: * * *` |
|        - |  3054 | ` *` |
|        - |  3055 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3056 | ` */` |
|        8 |  3057 | `case PH7_OP_CVT_OBJ:` |
|        - |  3058 | `#ifdef UNTRUST` |
|        - |  3059 | `	if( pTos < pStack ){` |
|        - |  3060 | `		goto Abort;` |
|        - |  3061 | `	}` |
|        - |  3062 | `#endif` |
|       17 |  3063 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3064 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3065 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3066 | `	}` |
|       17 |  3067 | `	break;` |
|        - |  3068 | `/*` |
|        - |  3069 | ` * ERR_CTRL * * *` |
|        - |  3070 | ` *` |
|        - |  3071 | ` * Error control operator.` |
|        - |  3072 | ` */` |
|    12674 |  3073 | `case PH7_OP_ERR_CTRL:` |
|        - |  3074 | `	/*` |
|        - |  3075 | `	 * TICKET 1433-038:` |
|        - |  3076 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3077 | `	 * use the public API,to control error output.` |
|        - |  3078 | `	 */` |
|    25348 |  3079 | `	break;` |
|        - |  3080 | `/*` |
|        - |  3081 | ` * IS_A * * *` |
|        - |  3082 | ` *` |
|        - |  3083 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3084 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3085 | ` * holding a class name or an object).` |
|        - |  3086 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3087 | ` */` |
|       23 |  3088 | `case PH7_OP_IS_A:{` |
|       48 |  3089 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3090 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3091 | `#ifdef UNTRUST` |
|        - |  3092 | `	if( pNos < pStack ){` |
|        - |  3093 | `		goto Abort;` |
|        - |  3094 | `	}` |
|        - |  3095 | `#endif` |
|       48 |  3096 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3097 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3098 | `		ph7_class *pClass = 0;` |
|        - |  3099 | `		/* Extract the target class */` |
|       46 |  3100 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3101 | `			/* Instance already loaded */` |
|      ! 0 |  3102 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3103 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3104 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3105 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3106 | `			/* Handle self/static/parent keywords */` |
|       46 |  3107 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3108 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3109 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3110 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3111 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3112 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3113 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3114 | `					pClass = pSelf->pBase;` |
|        2 |  3115 | `				}` |
|        3 |  3116 | `			}else{` |
|       36 |  3117 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3118 | `			}` |
|       22 |  3119 | `		}` |
|       46 |  3120 | `		if( pClass ){` |
|        - |  3121 | `			/* Perform the query */` |
|       46 |  3122 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3123 | `		}` |
|       22 |  3124 | `	}` |
|        - |  3125 | `	/* Push result */` |
|       48 |  3126 | `	VmPopOperand(&pTos,1);` |
|       48 |  3127 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3128 | `	pTos->x.iVal = iRes;` |
|       48 |  3129 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3130 | `	break;` |
|        - |  3131 | `				 }` |
|        - |  3132 |  |
|        - |  3133 | `/*` |
|        - |  3134 | ` * LOADC P1 P2 *` |
|        - |  3135 | ` *` |
|        - |  3136 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3137 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3138 | ` */` |
|   825863 |  3139 | `case PH7_OP_LOADC: {` |
|        - |  3140 | `	ph7_value *pObj;` |
|        - |  3141 | `	/* Reserve a room */` |
|  1651772 |  3142 | `	pTos++;` |
|  2469521 |  3143 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1651772 |  3144 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3145 | `			SyHashEntry *pEntry;` |
|        - |  3146 | `			/* Candidate for expansion via user defined callbacks */` |
|    16320 |  3147 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16320 |  3148 | `			if( pEntry ){` |
|    16316 |  3149 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3150 | `				/* Set a NULL default value */` |
|    16316 |  3151 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16316 |  3152 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3153 | `				/* Invoke the callback and deal with the expanded value */` |
|    16316 |  3154 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3155 | `				/* Mark as constant */` |
|    16316 |  3156 | `				pTos->nIdx = SXU32_HIGH;` |
|    16316 |  3157 | `				break;` |
|        - |  3158 | `			}` |
|        - |  3159 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3160 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3161 | `			 * through to string value for backward compatibility. */` |
|        - |  3162 | `			{` |
|        6 |  3163 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3164 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3165 | `				sxu32 j;` |
|       32 |  3166 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3167 | `					if( zLit[j] == '\\' ){` |
|        - |  3168 | `						/* Qualified name: must be a real constant.` |
|        - |  3169 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3170 | `						{` |
|        3 |  3171 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3172 | `							SyBlob sErr;` |
|        3 |  3173 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3174 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3175 | `							if( pErrFile ){` |
|        3 |  3176 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3177 | `							}` |
|        3 |  3178 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3179 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3180 | `							SyBlobRelease(&sErr);` |
|        - |  3181 | `						}` |
|        3 |  3182 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3183 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3184 | `						goto LoadC_Done;` |
|        - |  3185 | `					}` |
|       15 |  3186 | `				}` |
|        - |  3187 | `			}` |
|        1 |  3188 | `		}` |
|  1635456 |  3189 | `		PH7_MemObjLoad(pObj,pTos);` |
|   817751 |  3190 | `	}else{` |
|        - |  3191 | `		/* Set a NULL value */` |
|      ! 0 |  3192 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3193 | `	}` |
|   817706 |  3194 | `LoadC_Done:` |
|        - |  3195 | `	/* Mark as constant */` |
|  1635458 |  3196 | `	pTos->nIdx = SXU32_HIGH;` |
|  1635458 |  3197 | `	break;` |
|        - |  3198 | `				  }` |
|        - |  3199 | `/*` |
|        - |  3200 | ` * LOAD: P1 * P3` |
|        - |  3201 | ` *` |
|        - |  3202 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3203 | ` * from the P3 operand.` |
|        - |  3204 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3205 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3206 | ` */` |
|  1342076 |  3207 | `case PH7_OP_LOAD:{` |
|        - |  3208 | `	ph7_value *pObj;` |
|        - |  3209 | `	SyString sName;` |
|  2684374 |  3210 | `	if( pInstr->p3 == 0 ){` |
|        - |  3211 | `		/* Take the variable name from the top of the stack */` |
|        - |  3212 | `#ifdef UNTRUST` |
|        - |  3213 | `		if( pTos < pStack ){` |
|        - |  3214 | `			goto Abort;` |
|        - |  3215 | `		}` |
|        - |  3216 | `#endif` |
|        - |  3217 | `		/* Force a string cast */` |
|       19 |  3218 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3219 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3220 | `		}` |
|       19 |  3221 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3222 | `	}else{` |
|  2684356 |  3223 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3224 | `		/* Reserve a room for the target object */` |
|  2684356 |  3225 | `		pTos++;` |
|        - |  3226 | `	}` |
|        - |  3227 | `	/* Extract the requested memory object */` |
|  2684374 |  3228 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2684374 |  3229 | `	if( pObj == 0 ){` |
|       26 |  3230 | `		if( pInstr->iP1 ){` |
|        - |  3231 | `			/* Variable not found,load NULL */` |
|       26 |  3232 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3233 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3234 | `			}else{` |
|       26 |  3235 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3236 | `			}` |
|       26 |  3237 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1342090 |  3238 | `			break;` |
|      ! 0 |  3239 | `		}else{` |
|        - |  3240 | `			/* Fatal error */` |
|      ! 0 |  3241 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3242 | `			goto Abort;` |
|        - |  3243 | `		}` |
|        - |  3244 | `	}` |
|        - |  3245 | `	/* Load variable contents */` |
|  2684350 |  3246 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2684350 |  3247 | `	pTos->nIdx = pObj->nIdx;` |
|  2684350 |  3248 | `	break;` |
|        - |  3249 | `				   }` |
|        - |  3250 | `/*` |
|        - |  3251 | ` * LOAD_MAP P1 * *` |
|        - |  3252 | ` *` |
|        - |  3253 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3254 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3255 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3256 | ` */` |
|    18335 |  3257 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3258 | `	ph7_hashmap *pMap;` |
|        - |  3259 | `	/* Allocate a new hashmap instance */` |
|    36672 |  3260 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    36672 |  3261 | `	if( pMap == 0 ){` |
|      ! 0 |  3262 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3263 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3264 | `		goto Abort;` |
|        - |  3265 | `	}` |
|    36672 |  3266 | `	if( pInstr->iP1 > 0 ){` |
|     2238 |  3267 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3268 | `		/* Perform the insertion */` |
|     6838 |  3269 | `		while( pEntry < pTos ){` |
|     4602 |  3270 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3271 | `				/* Insertion by reference */` |
|      142 |  3272 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3273 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3274 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3275 | `					);` |
|       48 |  3276 | `			}else{` |
|        - |  3277 | `				/* Standard insertion */` |
|     6761 |  3278 | `				PH7_HashmapInsert(pMap,` |
|     4506 |  3279 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2253 |  3280 | `					&pEntry[1]` |
|        - |  3281 | `				);` |
|        - |  3282 | `			}` |
|        - |  3283 | `			/* Next pair on the stack */` |
|     4602 |  3284 | `			pEntry += 2;` |
|        2 |  3285 | `		}` |
|        - |  3286 | `		/* Pop P1 elements */` |
|     2238 |  3287 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1118 |  3288 | `	}` |
|        - |  3289 | `	/* Push the hashmap */` |
|    36672 |  3290 | `	pTos++;` |
|    36672 |  3291 | `	pTos->nIdx = SXU32_HIGH;` |
|    36672 |  3292 | `	pTos->x.pOther = pMap;` |
|    36672 |  3293 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    36672 |  3294 | `	break;` |
|        - |  3295 | `					  }` |
|        - |  3296 | `/*` |
|        - |  3297 | ` * LOAD_LIST: P1 * *` |
|        - |  3298 | ` *` |
|        - |  3299 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3300 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3301 | ` * Caveats:` |
|        - |  3302 | ` *  This implementation support only a single nesting level.` |
|        - |  3303 | ` */` |
|       26 |  3304 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3305 | `	ph7_value *pEntry;` |
|       54 |  3306 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3307 | `		/* Empty list,break immediately */` |
|      ! 0 |  3308 | `		break;` |
|        - |  3309 | `	}` |
|       54 |  3310 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3311 | `#ifdef UNTRUST` |
|        - |  3312 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3313 | `		goto Abort;` |
|        - |  3314 | `	}` |
|        - |  3315 | `#endif` |
|       54 |  3316 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       50 |  3317 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3318 | `		ph7_hashmap_node *pNode;` |
|        - |  3319 | `		ph7_value sKey,*pObj;` |
|        - |  3320 | `		/* Start Copying */` |
|       50 |  3321 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      154 |  3322 | `		while( pEntry <= pTos ){` |
|      106 |  3323 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       98 |  3324 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       98 |  3325 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       98 |  3326 | `					if( rc == SXRET_OK ){` |
|        - |  3327 | `						/* Store node value */` |
|       98 |  3328 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       50 |  3329 | `					}else{` |
|        - |  3330 | `						/* Nullify the variable */` |
|      ! 0 |  3331 | `						PH7_MemObjRelease(pObj);` |
|        - |  3332 | `					}` |
|       48 |  3333 | `				}` |
|       48 |  3334 | `			}` |
|      106 |  3335 | `			sKey.x.iVal++; /* Next numeric index */` |
|      106 |  3336 | `			pEntry++;` |
|        2 |  3337 | `		}` |
|       24 |  3338 | `	}` |
|       54 |  3339 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       54 |  3340 | `	break;` |
|        - |  3341 | `					   }` |
|        - |  3342 | `/*` |
|        - |  3343 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3344 | ` *` |
|        - |  3345 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3346 | ` * from the stack.` |
|        - |  3347 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3348 | ` * instead.` |
|        - |  3349 | ` */` |
|   215821 |  3350 | `case PH7_OP_LOAD_IDX: {` |
|   431688 |  3351 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   431688 |  3352 | `	ph7_hashmap *pMap = 0;` |
|        - |  3353 | `	ph7_value *pIdx;` |
|   431688 |  3354 | `	pIdx = 0;` |
|   431688 |  3355 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3356 | `		if( !pInstr->iP2){` |
|        - |  3357 | `			/* No available index,load NULL */` |
|      ! 0 |  3358 | `			if( pTos >= pStack ){` |
|      ! 0 |  3359 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3360 | `			}else{` |
|        - |  3361 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3362 | `				pTos++;` |
|      ! 0 |  3363 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3364 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3365 | `			}` |
|        - |  3366 | `			/* Emit a notice */` |
|      ! 0 |  3367 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3368 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3369 | `			break;` |
|        - |  3370 | `		}` |
|      ! 0 |  3371 | `	}else{` |
|   431688 |  3372 | `		pIdx = pTos;` |
|   431688 |  3373 | `		pTos--;` |
|        - |  3374 | `	}` |
|   431688 |  3375 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3376 | `		/* String access */` |
|   340676 |  3377 | `		if( pIdx ){` |
|        - |  3378 | `			sxu32 nOfft;` |
|   340676 |  3379 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3380 | `				/* Force an int cast */` |
|      ! 0 |  3381 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3382 | `			}` |
|   340676 |  3383 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340676 |  3384 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3385 | `				/* Invalid offset,load null */` |
|      ! 0 |  3386 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3387 | `			}else{` |
|   340676 |  3388 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340676 |  3389 | `				int c = zData[nOfft];` |
|   340676 |  3390 | `				PH7_MemObjRelease(pTos);` |
|   340676 |  3391 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340676 |  3392 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3393 | `			}` |
|   170361 |  3394 | `		}else{` |
|        - |  3395 | `			/* No available index,load NULL */` |
|      ! 0 |  3396 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3397 | `		}` |
|   340676 |  3398 | `		break;` |
|        - |  3399 | `	}` |
|    91014 |  3400 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3401 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3402 | `			ph7_value *pObj;` |
|      ! 0 |  3403 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3404 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3405 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3406 | `			}` |
|      ! 0 |  3407 | `		}` |
|      ! 0 |  3408 | `	}` |
|    91014 |  3409 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    91014 |  3410 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    91014 |  3411 | `		if( pInstr->iP2 ){` |
|        - |  3412 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3413 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3414 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3415 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3416 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3417 | `		}` |
|        - |  3418 | `		/* Point to the hashmap */` |
|    91014 |  3419 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    91014 |  3420 | `		if( pIdx ){` |
|        - |  3421 | `			/* Load the desired entry */` |
|    91014 |  3422 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    45506 |  3423 | `		}` |
|    91014 |  3424 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3425 | `			/* Create a new empty entry */` |
|      265 |  3426 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3427 | `			if( rc == SXRET_OK ){` |
|        - |  3428 | `				/* Point to the last inserted entry */` |
|      265 |  3429 | `				pNode = pMap->pLast;` |
|      132 |  3430 | `			}` |
|      132 |  3431 | `		}` |
|    45506 |  3432 | `	}` |
|    91014 |  3433 | `	if( pIdx ){` |
|    91014 |  3434 | `		PH7_MemObjRelease(pIdx);` |
|    45506 |  3435 | `	}` |
|    91014 |  3436 | `	if( rc == SXRET_OK ){` |
|        - |  3437 | `		/* Load entry contents */` |
|    41706 |  3438 | `		if( pMap->iRef < 2 ){` |
|        - |  3439 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3440 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3441 | `			 */` |
|       24 |  3442 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3443 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3444 | `		}else{` |
|    41684 |  3445 | `			pTos->nIdx = pNode->nValIdx;` |
|    41684 |  3446 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    41684 |  3447 | `			PH7_HashmapUnref(pMap);` |
|        - |  3448 | `		}` |
|    20854 |  3449 | `	}else{` |
|        - |  3450 | `		/* No such entry,load NULL */` |
|    49310 |  3451 | `		PH7_MemObjRelease(pTos);` |
|    49310 |  3452 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3453 | `	}` |
|    91014 |  3454 | `	break;` |
|        - |  3455 | `					  }` |
|        - |  3456 | `/*` |
|        - |  3457 | ` * LOAD_CLOSURE * * P3` |
|        - |  3458 | ` *` |
|        - |  3459 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3460 | ` * name in the stack.` |
|        - |  3461 | ` */` |
|        4 |  3462 | `case PH7_OP_LOAD_CLOSURE:{` |
|       10 |  3463 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       10 |  3464 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3465 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3466 | `		ph7_vm_func *pClosure;` |
|        - |  3467 | `		char *zName;` |
|        - |  3468 | `		sxu32 mLen;` |
|        - |  3469 | `		sxu32 n;` |
|        - |  3470 | `		/* Create a new VM function */` |
|       10 |  3471 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3472 | `		/* Generate an unique closure name */` |
|       10 |  3473 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       10 |  3474 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3475 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3476 | `			goto Abort;` |
|        - |  3477 | `		}` |
|       10 |  3478 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       10 |  3479 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3480 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3481 | `		}` |
|        - |  3482 | `		/* Zero the stucture */` |
|       10 |  3483 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3484 | `		/* Perform a structure assignment on read-only items */` |
|       10 |  3485 | `		pClosure->aArgs = pFunc->aArgs;` |
|       10 |  3486 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       10 |  3487 | `		pClosure->aStatic = pFunc->aStatic;` |
|       10 |  3488 | `		pClosure->iFlags = pFunc->iFlags;` |
|       10 |  3489 | `		pClosure->pUserData = pFunc->pUserData;` |
|       10 |  3490 | `		pClosure->sSignature = pFunc->sSignature;` |
|       10 |  3491 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3492 | `		/* Register the closure */` |
|       10 |  3493 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3494 | `		/* Set up closure environment */` |
|       10 |  3495 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       10 |  3496 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       28 |  3497 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3498 | `			ph7_value *pValue;` |
|       20 |  3499 | `			pEnv = &aEnv[n];` |
|       20 |  3500 | `			sEnv.sName  = pEnv->sName;` |
|       20 |  3501 | `			sEnv.iFlags = pEnv->iFlags;` |
|       20 |  3502 | `			sEnv.nIdx = SXU32_HIGH;` |
|       20 |  3503 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       20 |  3504 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3505 | `				/* Pass by reference */` |
|      ! 0 |  3506 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3507 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3508 | `					);` |
|      ! 0 |  3509 | `			}` |
|        - |  3510 | `			/* Standard pass by value */` |
|       20 |  3511 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       20 |  3512 | `			if( pValue ){` |
|        - |  3513 | `				/* Copy imported value */` |
|       12 |  3514 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3515 | `			}` |
|        - |  3516 | `			/* Insert the imported variable */` |
|       20 |  3517 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       11 |  3518 | `		}` |
|        - |  3519 | `		/* Finally,load the closure name on the stack */` |
|       10 |  3520 | `		pTos++;` |
|       10 |  3521 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3522 | `	}` |
|       10 |  3523 | `	break;` |
|        - |  3524 | `						 }` |
|        - |  3525 | `/*` |
|        - |  3526 | ` * STORE * P2 P3` |
|        - |  3527 | ` *` |
|        - |  3528 | ` * Perform a store (Assignment) operation.` |
|        - |  3529 | ` */` |
|   113416 |  3530 | `case PH7_OP_STORE: {` |
|        - |  3531 | `	ph7_value *pObj;` |
|        - |  3532 | `	SyString sName;` |
|        - |  3533 | `#ifdef UNTRUST` |
|        - |  3534 | `	if( pTos < pStack ){` |
|        - |  3535 | `		goto Abort;` |
|        - |  3536 | `	}` |
|        - |  3537 | `#endif` |
|   226834 |  3538 | `	if( pInstr->iP2 ){` |
|        - |  3539 | `		sxu32 nIdx;` |
|        - |  3540 | `		/* Member store operation */` |
|     2954 |  3541 | `		nIdx = pTos->nIdx;` |
|     2954 |  3542 | `		VmPopOperand(&pTos,1);` |
|     2954 |  3543 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3544 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3545 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3546 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3547 | `		}else{` |
|        - |  3548 | `			/* Point to the desired memory object */` |
|     2950 |  3549 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2950 |  3550 | `			if( pObj ){` |
|        - |  3551 | `				/* Perform the store operation */` |
|     2950 |  3552 | `				PH7_MemObjStore(pTos,pObj);` |
|     1474 |  3553 | `			}` |
|        - |  3554 | `		}` |
|   114894 |  3555 | `		break;` |
|   223882 |  3556 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3557 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3558 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3559 | `			/* Force a string cast */` |
|      ! 0 |  3560 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3561 | `		}` |
|        7 |  3562 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3563 | `		pTos--;` |
|        - |  3564 | `#ifdef UNTRUST` |
|        - |  3565 | `		if( pTos < pStack  ){` |
|        - |  3566 | `			goto Abort;` |
|        - |  3567 | `		}` |
|        - |  3568 | `#endif` |
|        4 |  3569 | `	}else{` |
|   223876 |  3570 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3571 | `	}` |
|        - |  3572 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   223882 |  3573 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   223882 |  3574 | `	if( pObj == 0 ){` |
|      ! 0 |  3575 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3576 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3577 | `		goto Abort;` |
|        - |  3578 | `	}` |
|   223882 |  3579 | `	if( !pInstr->p3 ){` |
|        7 |  3580 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3581 | `	}` |
|        - |  3582 | `	/* Perform the store operation */` |
|   223882 |  3583 | `	PH7_MemObjStore(pTos,pObj);` |
|   223882 |  3584 | `	break;` |
|        - |  3585 | `				   }` |
|        - |  3586 | `/*` |
|        - |  3587 | ` * STORE_IDX:   P1 * P3` |
|        - |  3588 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3589 | ` *` |
|        - |  3590 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3591 | ` */` |
|    81765 |  3592 | `case PH7_OP_STORE_IDX:` |
|        - |  3593 | `case PH7_OP_STORE_IDX_REF: {` |
|   163532 |  3594 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3595 | `	ph7_value *pKey;` |
|        - |  3596 | `	sxu32 nIdx;` |
|   163532 |  3597 | `	if( pInstr->iP1 ){` |
|        - |  3598 | `		/* Key is next on stack */` |
|    57422 |  3599 | `		pKey = pTos;` |
|    57422 |  3600 | `		pTos--;` |
|    28712 |  3601 | `	}else{` |
|   106112 |  3602 | `		pKey = 0;` |
|        - |  3603 | `	}` |
|   163532 |  3604 | `	nIdx = pTos->nIdx;` |
|   163532 |  3605 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3606 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3607 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3608 | `		 * checking true sharing count, then re-add after separation. */` |
|   163480 |  3609 | `		if( nIdx != SXU32_HIGH ){` |
|   163480 |  3610 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   245219 |  3611 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   163480 |  3612 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3613 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3614 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3615 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3616 | `				 * refcounts if the backing array was already separated. */` |
|   163480 |  3617 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   163480 |  3618 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   163480 |  3619 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   163480 |  3620 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   163480 |  3621 | `					pTos->x.pOther = pMap;` |
|    81741 |  3622 | `				}else{` |
|        - |  3623 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3624 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3625 | `					pMap = pCur;` |
|        - |  3626 | `				}` |
|    81741 |  3627 | `			}else{` |
|      ! 0 |  3628 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3629 | `			}` |
|    81741 |  3630 | `		}else{` |
|      ! 0 |  3631 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3632 | `		}` |
|   163480 |  3633 | `		if( pMap->iRef < 2 ){` |
|        - |  3634 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3635 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3636 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3637 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3638 | `			pMap->iRef = 2;` |
|      ! 0 |  3639 | `		}` |
|    81741 |  3640 | `	}else{` |
|        - |  3641 | `		ph7_value *pObj;` |
|       53 |  3642 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3643 | `		if( pObj == 0 ){` |
|      ! 0 |  3644 | `			if( pKey ){` |
|      ! 0 |  3645 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3646 | `			}` |
|      ! 0 |  3647 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3648 | `			break;` |
|        - |  3649 | `		}` |
|        - |  3650 | `		/* Phase#1: Load the array */` |
|       53 |  3651 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3652 | `			VmPopOperand(&pTos,1);` |
|       53 |  3653 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3654 | `				/* Force a string cast */` |
|      ! 0 |  3655 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3656 | `			}` |
|       53 |  3657 | `			if( pKey == 0 ){` |
|        - |  3658 | `				/* Append string */` |
|        3 |  3659 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3660 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3661 | `				}` |
|        2 |  3662 | `			}else{` |
|        - |  3663 | `				sxu32 nOfft;` |
|       51 |  3664 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3665 | `					/* Force an int cast */` |
|       51 |  3666 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3667 | `				}` |
|       51 |  3668 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3669 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3670 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3671 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3672 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3673 | `				}else{` |
|      ! 0 |  3674 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3675 | `						/* Perform an append operation */` |
|      ! 0 |  3676 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3677 | `					}` |
|        - |  3678 | `				}` |
|        - |  3679 | `			}` |
|       53 |  3680 | `			if( pKey ){` |
|       51 |  3681 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3682 | `			}` |
|       53 |  3683 | `			break;` |
|      ! 0 |  3684 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3685 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3686 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3687 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3688 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3689 | `				goto Abort;` |
|        - |  3690 | `			}` |
|      ! 0 |  3691 | `		}` |
|        - |  3692 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3693 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3694 | `	}` |
|   163480 |  3695 | `	VmPopOperand(&pTos,1);` |
|        - |  3696 | `	/* Phase#2: Perform the insertion */` |
|   163480 |  3697 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3698 | `		/* Insertion by reference */` |
|       15 |  3699 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3700 | `	}else{` |
|   163466 |  3701 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3702 | `	}` |
|   163480 |  3703 | `	if( pKey ){` |
|    57372 |  3704 | `		PH7_MemObjRelease(pKey);` |
|    28685 |  3705 | `	}` |
|   163480 |  3706 | `	break;` |
|        - |  3707 | `					   }` |
|        - |  3708 | `/*` |
|        - |  3709 | ` * INCR: P1 * *` |
|        - |  3710 | ` *` |
|        - |  3711 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3712 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3713 | ` * the stack and increment after that.` |
|        - |  3714 | ` */` |
|   151306 |  3715 | `case PH7_OP_INCR:` |
|        - |  3716 | `#ifdef UNTRUST` |
|        - |  3717 | `	if( pTos < pStack ){` |
|        - |  3718 | `		goto Abort;` |
|        - |  3719 | `	}` |
|        - |  3720 | `#endif` |
|   302658 |  3721 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302658 |  3722 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3723 | `			ph7_value *pObj;` |
|   302658 |  3724 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3725 | `				/* Force a numeric cast */` |
|   302658 |  3726 | `				PH7_MemObjToNumeric(pObj);` |
|   302658 |  3727 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3728 | `					pObj->rVal++;` |
|        - |  3729 | `					/* Try to get an integer representation */` |
|      ! 0 |  3730 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3731 | `				}else{` |
|   302658 |  3732 | `					pObj->x.iVal++;` |
|   302658 |  3733 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3734 | `				}` |
|   302658 |  3735 | `				if( pInstr->iP1 ){` |
|        - |  3736 | `					/* Pre-icrement */` |
|       71 |  3737 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3738 | `				}` |
|   151350 |  3739 | `			}` |
|   151352 |  3740 | `		}else{` |
|      ! 0 |  3741 | `			if( pInstr->iP1 ){` |
|        - |  3742 | `				/* Force a numeric cast */` |
|      ! 0 |  3743 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3744 | `				/* Pre-increment */` |
|      ! 0 |  3745 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3746 | `					pTos->rVal++;` |
|        - |  3747 | `					/* Try to get an integer representation */` |
|      ! 0 |  3748 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3749 | `				}else{` |
|      ! 0 |  3750 | `					pTos->x.iVal++;` |
|      ! 0 |  3751 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3752 | `				}` |
|      ! 0 |  3753 | `			}` |
|        - |  3754 | `		}` |
|   151350 |  3755 | `	}` |
|   302658 |  3756 | `	break;` |
|        - |  3757 | `/*` |
|        - |  3758 | ` * DECR: P1 * *` |
|        - |  3759 | ` *` |
|        - |  3760 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3761 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3762 | ` * and decrement after that.` |
|        - |  3763 | ` */` |
|        2 |  3764 | `case PH7_OP_DECR:` |
|        - |  3765 | `#ifdef UNTRUST` |
|        - |  3766 | `	if( pTos < pStack ){` |
|        - |  3767 | `		goto Abort;` |
|        - |  3768 | `	}` |
|        - |  3769 | `#endif` |
|        5 |  3770 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3771 | `		/* Force a numeric cast */` |
|        5 |  3772 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3773 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3774 | `			ph7_value *pObj;` |
|        5 |  3775 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3776 | `				/* Force a numeric cast */` |
|        5 |  3777 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3778 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3779 | `					pObj->rVal--;` |
|        - |  3780 | `					/* Try to get an integer representation */` |
|      ! 0 |  3781 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3782 | `				}else{` |
|        5 |  3783 | `					pObj->x.iVal--;` |
|        5 |  3784 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3785 | `				}` |
|        5 |  3786 | `				if( pInstr->iP1 ){` |
|        - |  3787 | `					/* Pre-icrement */` |
|      ! 0 |  3788 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3789 | `				}` |
|        2 |  3790 | `			}` |
|        3 |  3791 | `		}else{` |
|      ! 0 |  3792 | `			if( pInstr->iP1 ){` |
|        - |  3793 | `				/* Pre-increment */` |
|      ! 0 |  3794 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3795 | `					pTos->rVal--;` |
|        - |  3796 | `					/* Try to get an integer representation */` |
|      ! 0 |  3797 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3798 | `				}else{` |
|      ! 0 |  3799 | `					pTos->x.iVal--;` |
|      ! 0 |  3800 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3801 | `				}` |
|      ! 0 |  3802 | `			}` |
|        - |  3803 | `		}` |
|        2 |  3804 | `	}` |
|        5 |  3805 | `	break;` |
|        - |  3806 | `/*` |
|        - |  3807 | ` * UMINUS: * * *` |
|        - |  3808 | ` *` |
|        - |  3809 | ` * Perform a unary minus operation.` |
|        - |  3810 | ` */` |
|    23725 |  3811 | `case PH7_OP_UMINUS:` |
|        - |  3812 | `#ifdef UNTRUST` |
|        - |  3813 | `	if( pTos < pStack ){` |
|        - |  3814 | `		goto Abort;` |
|        - |  3815 | `	}` |
|        - |  3816 | `#endif` |
|        - |  3817 | `	/* Force a numeric (integer,real or both) cast */` |
|    47452 |  3818 | `	PH7_MemObjToNumeric(pTos);` |
|    47452 |  3819 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3820 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3821 | `	}` |
|    47452 |  3822 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    47422 |  3823 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23710 |  3824 | `	}` |
|    47452 |  3825 | `	break;` |
|        - |  3826 | `/*` |
|        - |  3827 | ` * UPLUS: * * *` |
|        - |  3828 | ` *` |
|        - |  3829 | ` * Perform a unary plus operation.` |
|        - |  3830 | ` */` |
|       16 |  3831 | `case PH7_OP_UPLUS:` |
|        - |  3832 | `#ifdef UNTRUST` |
|        - |  3833 | `	if( pTos < pStack ){` |
|        - |  3834 | `		goto Abort;` |
|        - |  3835 | `	}` |
|        - |  3836 | `#endif` |
|        - |  3837 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3838 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3839 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3840 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3841 | `	}` |
|       33 |  3842 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3843 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3844 | `	}` |
|       33 |  3845 | `	break;` |
|        - |  3846 | `/*` |
|        - |  3847 | ` * OP_LNOT: * * *` |
|        - |  3848 | ` *` |
|        - |  3849 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3850 | ` * with its complement.` |
|        - |  3851 | ` */` |
|    40021 |  3852 | `case PH7_OP_LNOT:` |
|        - |  3853 | `#ifdef UNTRUST` |
|        - |  3854 | `	if( pTos < pStack ){` |
|        - |  3855 | `		goto Abort;` |
|        - |  3856 | `	}` |
|        - |  3857 | `#endif` |
|        - |  3858 | `	/* Force a boolean cast */` |
|    80088 |  3859 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3860 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3861 | `	}` |
|    80088 |  3862 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80088 |  3863 | `	break;` |
|        - |  3864 | `/*` |
|        - |  3865 | ` * OP_BITNOT: * * *` |
|        - |  3866 | ` *` |
|        - |  3867 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3868 | ` * with its ones-complement.` |
|        - |  3869 | ` */` |
|       14 |  3870 | `case PH7_OP_BITNOT:` |
|        - |  3871 | `#ifdef UNTRUST` |
|        - |  3872 | `	if( pTos < pStack ){` |
|        - |  3873 | `		goto Abort;` |
|        - |  3874 | `	}` |
|        - |  3875 | `#endif` |
|        - |  3876 | `	/* Force an integer cast */` |
|       30 |  3877 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3878 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3879 | `	}` |
|       30 |  3880 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3881 | `	break;` |
|        - |  3882 | `/* OP_MUL * * *` |
|        - |  3883 | ` * OP_MUL_STORE * * *` |
|        - |  3884 | ` *` |
|        - |  3885 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3886 | ` * and push the result back onto the stack.` |
|        - |  3887 | ` */` |
|     1247 |  3888 | `case PH7_OP_MUL:` |
|        - |  3889 | `case PH7_OP_MUL_STORE: {` |
|     2496 |  3890 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3891 | `	/* Force the operand to be numeric */` |
|        - |  3892 | `#ifdef UNTRUST` |
|        - |  3893 | `	if( pNos < pStack ){` |
|        - |  3894 | `		goto Abort;` |
|        - |  3895 | `	}` |
|        - |  3896 | `#endif` |
|     2496 |  3897 | `	PH7_MemObjToNumeric(pTos);` |
|     2496 |  3898 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3899 | `	/* Perform the requested operation */` |
|     2496 |  3900 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3901 | `		/* Floating point arithemic */` |
|        - |  3902 | `		ph7_real a,b,r;` |
|       17 |  3903 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3904 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3905 | `		}` |
|       17 |  3906 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3907 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3908 | `		}` |
|       17 |  3909 | `		a = pNos->rVal;` |
|       17 |  3910 | `		b = pTos->rVal;` |
|       17 |  3911 | `		r = a * b;` |
|        - |  3912 | `		/* Push the result */` |
|       17 |  3913 | `		pNos->rVal = r;` |
|       17 |  3914 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3915 | `		/* Try to get an integer representation */` |
|       17 |  3916 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3917 | `	}else{` |
|        - |  3918 | `		/* Integer arithmetic */` |
|        - |  3919 | `		sxi64 a,b,r;` |
|     2480 |  3920 | `		a = pNos->x.iVal;` |
|     2480 |  3921 | `		b = pTos->x.iVal;` |
|     2480 |  3922 | `		r = a * b;` |
|        - |  3923 | `		/* Push the result */` |
|     2480 |  3924 | `		pNos->x.iVal = r;` |
|     2480 |  3925 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3926 | `	}` |
|     2496 |  3927 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3928 | `		ph7_value *pObj;` |
|       25 |  3929 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3930 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       25 |  3931 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       25 |  3932 | `			PH7_MemObjStore(pNos,pObj);` |
|       12 |  3933 | `		}` |
|       12 |  3934 | `	}` |
|     2496 |  3935 | `	VmPopOperand(&pTos,1);` |
|     2496 |  3936 | `	break;` |
|        - |  3937 | `				 }` |
|        - |  3938 | `/* OP_ADD * * *` |
|        - |  3939 | ` *` |
|        - |  3940 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3941 | ` * and push the result back onto the stack.` |
|        - |  3942 | ` */` |
|      439 |  3943 | `case PH7_OP_ADD:{` |
|      880 |  3944 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3945 | `#ifdef UNTRUST` |
|        - |  3946 | `	if( pNos < pStack ){` |
|        - |  3947 | `		goto Abort;` |
|        - |  3948 | `	}` |
|        - |  3949 | `#endif` |
|        - |  3950 | `	/* Perform the addition */` |
|      880 |  3951 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      880 |  3952 | `	VmPopOperand(&pTos,1);` |
|      880 |  3953 | `	break;` |
|        - |  3954 | `				}` |
|        - |  3955 | `/*` |
|        - |  3956 | ` * OP_ADD_STORE * * *` |
|        - |  3957 | ` *` |
|        - |  3958 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3959 | ` * and push the result back onto the stack.` |
|        - |  3960 | ` */` |
|      483 |  3961 | `case PH7_OP_ADD_STORE:{` |
|      968 |  3962 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3963 | `	ph7_value *pObj;` |
|        - |  3964 | `	sxu32 nIdx;` |
|        - |  3965 | `#ifdef UNTRUST` |
|        - |  3966 | `	if( pNos < pStack ){` |
|        - |  3967 | `		goto Abort;` |
|        - |  3968 | `	}` |
|        - |  3969 | `#endif` |
|        - |  3970 | `	/* Perform the addition */` |
|      968 |  3971 | `	nIdx = pTos->nIdx;` |
|      968 |  3972 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3973 | `	/* Peform the store operation */` |
|      968 |  3974 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3975 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      968 |  3976 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      968 |  3977 | `		PH7_MemObjStore(pTos,pObj);` |
|      483 |  3978 | `	}` |
|        - |  3979 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      968 |  3980 | `	PH7_MemObjStore(pTos,pNos);` |
|      968 |  3981 | `	VmPopOperand(&pTos,1);` |
|      968 |  3982 | `	break;` |
|        - |  3983 | `				}` |
|        - |  3984 | `/* OP_SUB * * *` |
|        - |  3985 | ` *` |
|        - |  3986 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3987 | ` * first (what was next on the stack) from the second (the` |
|        - |  3988 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3989 | ` */` |
|      299 |  3990 | `case PH7_OP_SUB: {` |
|      600 |  3991 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3992 | `#ifdef UNTRUST` |
|        - |  3993 | `	if( pNos < pStack ){` |
|        - |  3994 | `		goto Abort;` |
|        - |  3995 | `	}` |
|        - |  3996 | `#endif` |
|      600 |  3997 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3998 | `		/* Floating point arithemic */` |
|        - |  3999 | `		ph7_real a,b,r;` |
|       95 |  4000 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4001 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4002 | `		}` |
|       95 |  4003 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4004 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4005 | `		}` |
|       95 |  4006 | `		a = pNos->rVal;` |
|       95 |  4007 | `		b = pTos->rVal;` |
|       95 |  4008 | `		r = a - b;` |
|        - |  4009 | `		/* Push the result */` |
|       95 |  4010 | `		pNos->rVal = r;` |
|       95 |  4011 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4012 | `		/* Try to get an integer representation */` |
|       95 |  4013 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4014 | `	}else{` |
|        - |  4015 | `		/* Integer arithmetic */` |
|        - |  4016 | `		sxi64 a,b,r;` |
|      506 |  4017 | `		a = pNos->x.iVal;` |
|      506 |  4018 | `		b = pTos->x.iVal;` |
|      506 |  4019 | `		r = a - b;` |
|        - |  4020 | `		/* Push the result */` |
|      506 |  4021 | `		pNos->x.iVal = r;` |
|      506 |  4022 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4023 | `	}` |
|      600 |  4024 | `	VmPopOperand(&pTos,1);` |
|      600 |  4025 | `	break;` |
|        - |  4026 | `				 }` |
|        - |  4027 | `/* OP_SUB_STORE * * *` |
|        - |  4028 | ` *` |
|        - |  4029 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4030 | ` * first (what was next on the stack) from the second (the` |
|        - |  4031 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4032 | ` */` |
|        1 |  4033 | `case PH7_OP_SUB_STORE: {` |
|        3 |  4034 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4035 | `	ph7_value *pObj;` |
|        - |  4036 | `#ifdef UNTRUST` |
|        - |  4037 | `	if( pNos < pStack ){` |
|        - |  4038 | `		goto Abort;` |
|        - |  4039 | `	}` |
|        - |  4040 | `#endif` |
|        3 |  4041 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4042 | `		/* Floating point arithemic */` |
|        - |  4043 | `		ph7_real a,b,r;` |
|      ! 0 |  4044 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4045 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4046 | `		}` |
|      ! 0 |  4047 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4048 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4049 | `		}` |
|      ! 0 |  4050 | `		a = pTos->rVal;` |
|      ! 0 |  4051 | `		b = pNos->rVal;` |
|      ! 0 |  4052 | `		r = a - b;` |
|        - |  4053 | `		/* Push the result */` |
|      ! 0 |  4054 | `		pNos->rVal = r;` |
|      ! 0 |  4055 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4056 | `		/* Try to get an integer representation */` |
|      ! 0 |  4057 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4058 | `	}else{` |
|        - |  4059 | `		/* Integer arithmetic */` |
|        - |  4060 | `		sxi64 a,b,r;` |
|        3 |  4061 | `		a = pTos->x.iVal;` |
|        3 |  4062 | `		b = pNos->x.iVal;` |
|        3 |  4063 | `		r = a - b;` |
|        - |  4064 | `		/* Push the result */` |
|        3 |  4065 | `		pNos->x.iVal = r;` |
|        3 |  4066 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4067 | `	}` |
|        3 |  4068 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4069 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4070 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4071 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4072 | `	}` |
|        3 |  4073 | `	VmPopOperand(&pTos,1);` |
|        3 |  4074 | `	break;` |
|        - |  4075 | `				 }` |
|        - |  4076 |  |
|        - |  4077 | `/*` |
|        - |  4078 | ` * OP_MOD * * *` |
|        - |  4079 | ` *` |
|        - |  4080 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4081 | ` * first (what was next on the stack) from the second (the` |
|        - |  4082 | ` * top of the stack) and push the remainder after division` |
|        - |  4083 | ` * onto the stack.` |
|        - |  4084 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4085 | ` */` |
|      305 |  4086 | `case PH7_OP_MOD:{` |
|      612 |  4087 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4088 | `	sxi64 a,b,r;` |
|        - |  4089 | `#ifdef UNTRUST` |
|        - |  4090 | `	if( pNos < pStack ){` |
|        - |  4091 | `		goto Abort;` |
|        - |  4092 | `	}` |
|        - |  4093 | `#endif` |
|        - |  4094 | `	/* Force the operands to be integer */` |
|      612 |  4095 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4096 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4097 | `	}` |
|      612 |  4098 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4099 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4100 | `	}` |
|        - |  4101 | `	/* Perform the requested operation */` |
|      612 |  4102 | `	a = pNos->x.iVal;` |
|      612 |  4103 | `	b = pTos->x.iVal;` |
|      612 |  4104 | `	if( b == 0 ){` |
|        3 |  4105 | `		r = 0;` |
|        3 |  4106 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4107 | `		/* goto Abort; */` |
|        2 |  4108 | `	}else{` |
|      609 |  4109 | `		r = a%b;` |
|        - |  4110 | `	}` |
|        - |  4111 | `	/* Push the result */` |
|      612 |  4112 | `	pNos->x.iVal = r;` |
|      612 |  4113 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      612 |  4114 | `	VmPopOperand(&pTos,1);` |
|      612 |  4115 | `	break;` |
|        - |  4116 | `				}` |
|        - |  4117 | `/*` |
|        - |  4118 | ` * OP_MOD_STORE * * *` |
|        - |  4119 | ` *` |
|        - |  4120 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4121 | ` * first (what was next on the stack) from the second (the` |
|        - |  4122 | ` * top of the stack) and push the remainder after division` |
|        - |  4123 | ` * onto the stack.` |
|        - |  4124 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4125 | ` */` |
|        1 |  4126 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4127 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4128 | `	ph7_value *pObj;` |
|        - |  4129 | `	sxi64 a,b,r;` |
|        - |  4130 | `#ifdef UNTRUST` |
|        - |  4131 | `	if( pNos < pStack ){` |
|        - |  4132 | `		goto Abort;` |
|        - |  4133 | `	}` |
|        - |  4134 | `#endif` |
|        - |  4135 | `	/* Force the operands to be integer */` |
|        3 |  4136 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4137 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4138 | `	}` |
|        3 |  4139 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4140 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4141 | `	}` |
|        - |  4142 | `	/* Perform the requested operation */` |
|        3 |  4143 | `	a = pTos->x.iVal;` |
|        3 |  4144 | `	b = pNos->x.iVal;` |
|        3 |  4145 | `	if( b == 0 ){` |
|      ! 0 |  4146 | `		r = 0;` |
|      ! 0 |  4147 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4148 | `		/* goto Abort; */` |
|      ! 0 |  4149 | `	}else{` |
|        3 |  4150 | `		r = a%b;` |
|        - |  4151 | `	}` |
|        - |  4152 | `	/* Push the result */` |
|        3 |  4153 | `	pNos->x.iVal = r;` |
|        3 |  4154 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4155 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4156 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4157 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4158 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4159 | `	}` |
|        3 |  4160 | `	VmPopOperand(&pTos,1);` |
|        3 |  4161 | `	break;` |
|        - |  4162 | `				}` |
|        - |  4163 | `/*` |
|        - |  4164 | ` * OP_DIV * * *` |
|        - |  4165 | ` *` |
|        - |  4166 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4167 | ` * first (what was next on the stack) from the second (the` |
|        - |  4168 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4169 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4170 | ` */` |
|       28 |  4171 | `case PH7_OP_DIV:{` |
|       58 |  4172 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4173 | `	ph7_real a,b,r;` |
|        - |  4174 | `#ifdef UNTRUST` |
|        - |  4175 | `	if( pNos < pStack ){` |
|        - |  4176 | `		goto Abort;` |
|        - |  4177 | `	}` |
|        - |  4178 | `#endif` |
|        - |  4179 | `	/* Force the operands to be real */` |
|       58 |  4180 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  4181 | `		PH7_MemObjToReal(pTos);` |
|       26 |  4182 | `	}` |
|       58 |  4183 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  4184 | `		PH7_MemObjToReal(pNos);` |
|        9 |  4185 | `	}` |
|        - |  4186 | `	/* Perform the requested operation */` |
|       58 |  4187 | `	a = pNos->rVal;` |
|       58 |  4188 | `	b = pTos->rVal;` |
|       58 |  4189 | `	if( b == 0 ){` |
|        - |  4190 | `		/* Division by zero */` |
|        3 |  4191 | `		pNos->rVal = 0;` |
|        3 |  4192 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4193 | `		/* goto Abort; */` |
|        2 |  4194 | `	}else{` |
|       55 |  4195 | `		r = a/b;` |
|        - |  4196 | `		/* Push the result */` |
|       55 |  4197 | `		pNos->rVal = r;` |
|       55 |  4198 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4199 | `		/* Try to get an integer representation */` |
|       55 |  4200 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4201 | `	}` |
|       58 |  4202 | `	VmPopOperand(&pTos,1);` |
|       58 |  4203 | `	break;` |
|        - |  4204 | `				}` |
|        - |  4205 | `/*` |
|        - |  4206 | ` * OP_DIV_STORE * * *` |
|        - |  4207 | ` *` |
|        - |  4208 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4209 | ` * first (what was next on the stack) from the second (the` |
|        - |  4210 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4211 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4212 | ` */` |
|        1 |  4213 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4214 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4215 | `	ph7_value *pObj;` |
|        - |  4216 | `	ph7_real a,b,r;` |
|        - |  4217 | `#ifdef UNTRUST` |
|        - |  4218 | `	if( pNos < pStack ){` |
|        - |  4219 | `		goto Abort;` |
|        - |  4220 | `	}` |
|        - |  4221 | `#endif` |
|        - |  4222 | `	/* Force the operands to be real */` |
|        3 |  4223 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4224 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4225 | `	}` |
|        3 |  4226 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4227 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4228 | `	}` |
|        - |  4229 | `	/* Perform the requested operation */` |
|        3 |  4230 | `	a = pTos->rVal;` |
|        3 |  4231 | `	b = pNos->rVal;` |
|        3 |  4232 | `	if( b == 0 ){` |
|        - |  4233 | `		/* Division by zero */` |
|      ! 0 |  4234 | `		r = 0;` |
|      ! 0 |  4235 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4236 | `		/* goto Abort; */` |
|      ! 0 |  4237 | `	}else{` |
|        3 |  4238 | `		r = a/b;` |
|        - |  4239 | `		/* Push the result */` |
|        3 |  4240 | `		pNos->rVal = r;` |
|        3 |  4241 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4242 | `		/* Try to get an integer representation */` |
|        3 |  4243 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4244 | `	}` |
|        3 |  4245 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4246 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4247 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4248 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4249 | `	}` |
|        3 |  4250 | `	VmPopOperand(&pTos,1);` |
|        3 |  4251 | `	break;` |
|        - |  4252 | `				}` |
|        - |  4253 | `/* OP_BAND * * *` |
|        - |  4254 | ` *` |
|        - |  4255 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4256 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4257 | ` * two elements.` |
|        - |  4258 | `*/` |
|        - |  4259 | `/* OP_BOR * * *` |
|        - |  4260 | ` *` |
|        - |  4261 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4262 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4263 | ` * two elements.` |
|        - |  4264 | ` */` |
|        - |  4265 | `/* OP_BXOR * * *` |
|        - |  4266 | ` *` |
|        - |  4267 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4268 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4269 | ` * two elements.` |
|        - |  4270 | ` */` |
|       30 |  4271 | `case PH7_OP_BAND:` |
|        - |  4272 | `case PH7_OP_BOR:` |
|        - |  4273 | `case PH7_OP_BXOR:{` |
|       62 |  4274 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4275 | `	sxi64 a,b,r;` |
|        - |  4276 | `#ifdef UNTRUST` |
|        - |  4277 | `	if( pNos < pStack ){` |
|        - |  4278 | `		goto Abort;` |
|        - |  4279 | `	}` |
|        - |  4280 | `#endif` |
|        - |  4281 | `	/* Force the operands to be integer */` |
|       62 |  4282 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4283 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4284 | `	}` |
|       62 |  4285 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4286 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4287 | `	}` |
|        - |  4288 | `	/* Perform the requested operation */` |
|       62 |  4289 | `	a = pNos->x.iVal;` |
|       62 |  4290 | `	b = pTos->x.iVal;` |
|       62 |  4291 | `	switch(pInstr->iOp){` |
|        6 |  4292 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4293 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4294 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4295 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4296 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4297 | `	case PH7_OP_BAND:` |
|       38 |  4298 | `	default:          r = a&b; break;` |
|        - |  4299 | `	}` |
|        - |  4300 | `	/* Push the result */` |
|       62 |  4301 | `	pNos->x.iVal = r;` |
|       62 |  4302 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4303 | `	VmPopOperand(&pTos,1);` |
|       62 |  4304 | `	break;` |
|        - |  4305 | `				 }` |
|        - |  4306 | `/* OP_BAND_STORE * * *` |
|        - |  4307 | ` *` |
|        - |  4308 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4309 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4310 | ` * two elements.` |
|        - |  4311 | `*/` |
|        - |  4312 | `/* OP_BOR_STORE * * *` |
|        - |  4313 | ` *` |
|        - |  4314 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4315 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4316 | ` * two elements.` |
|        - |  4317 | ` */` |
|        - |  4318 | `/* OP_BXOR_STORE * * *` |
|        - |  4319 | ` *` |
|        - |  4320 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4321 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4322 | ` * two elements.` |
|        - |  4323 | ` */` |
|        7 |  4324 | `case PH7_OP_BAND_STORE:` |
|        - |  4325 | `case PH7_OP_BOR_STORE:` |
|        - |  4326 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4327 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4328 | `	ph7_value *pObj;` |
|        - |  4329 | `	sxi64 a,b,r;` |
|        - |  4330 | `#ifdef UNTRUST` |
|        - |  4331 | `	if( pNos < pStack ){` |
|        - |  4332 | `		goto Abort;` |
|        - |  4333 | `	}` |
|        - |  4334 | `#endif` |
|        - |  4335 | `	/* Force the operands to be integer */` |
|       15 |  4336 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4337 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4338 | `	}` |
|       15 |  4339 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4340 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4341 | `	}` |
|        - |  4342 | `	/* Perform the requested operation */` |
|       15 |  4343 | `	a = pTos->x.iVal;` |
|       15 |  4344 | `	b = pNos->x.iVal;` |
|       15 |  4345 | `	switch(pInstr->iOp){` |
|        2 |  4346 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4347 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4348 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4349 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4350 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4351 | `	case PH7_OP_BAND:` |
|        5 |  4352 | `	default:          r = a&b; break;` |
|        - |  4353 | `	}` |
|        - |  4354 | `	/* Push the result */` |
|       15 |  4355 | `	pNos->x.iVal = r;` |
|       15 |  4356 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4357 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4358 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4359 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4360 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4361 | `	}` |
|       15 |  4362 | `	VmPopOperand(&pTos,1);` |
|       15 |  4363 | `	break;` |
|        - |  4364 | `				 }` |
|        - |  4365 | `/* OP_SHL * * *` |
|        - |  4366 | ` *` |
|        - |  4367 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4368 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4369 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4370 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4371 | ` */` |
|        - |  4372 | `/* OP_SHR * * *` |
|        - |  4373 | ` *` |
|        - |  4374 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4375 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4376 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4377 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4378 | ` */` |
|        9 |  4379 | `case PH7_OP_SHL:` |
|        - |  4380 | `case PH7_OP_SHR: {` |
|       19 |  4381 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4382 | `	sxi64 a,r;` |
|        - |  4383 | `	sxi32 b;` |
|        - |  4384 | `#ifdef UNTRUST` |
|        - |  4385 | `	if( pNos < pStack ){` |
|        - |  4386 | `		goto Abort;` |
|        - |  4387 | `	}` |
|        - |  4388 | `#endif` |
|        - |  4389 | `	/* Force the operands to be integer */` |
|       19 |  4390 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4391 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4392 | `	}` |
|       19 |  4393 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4394 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4395 | `	}` |
|        - |  4396 | `	/* Perform the requested operation */` |
|       19 |  4397 | `	a = pNos->x.iVal;` |
|       19 |  4398 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4399 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4400 | `		r = a << b;` |
|        6 |  4401 | `	}else{` |
|        9 |  4402 | `		r = a >> b;` |
|        - |  4403 | `	}` |
|        - |  4404 | `	/* Push the result */` |
|       19 |  4405 | `	pNos->x.iVal = r;` |
|       19 |  4406 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4407 | `	VmPopOperand(&pTos,1);` |
|       19 |  4408 | `	break;` |
|        - |  4409 | `				 }` |
|        - |  4410 | `/*  OP_SHL_STORE * * *` |
|        - |  4411 | ` *` |
|        - |  4412 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4413 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4414 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4415 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4416 | ` */` |
|        - |  4417 | `/* OP_SHR_STORE * * *` |
|        - |  4418 | ` *` |
|        - |  4419 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4420 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4421 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4422 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4423 | ` */` |
|        7 |  4424 | `case PH7_OP_SHL_STORE:` |
|        - |  4425 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4426 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4427 | `	ph7_value *pObj;` |
|        - |  4428 | `	sxi64 a,r;` |
|        - |  4429 | `	sxi32 b;` |
|        - |  4430 | `#ifdef UNTRUST` |
|        - |  4431 | `	if( pNos < pStack ){` |
|        - |  4432 | `		goto Abort;` |
|        - |  4433 | `	}` |
|        - |  4434 | `#endif` |
|        - |  4435 | `	/* Force the operands to be integer */` |
|       15 |  4436 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4437 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4438 | `	}` |
|       15 |  4439 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4440 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4441 | `	}` |
|        - |  4442 | `	/* Perform the requested operation */` |
|       15 |  4443 | `	a = pTos->x.iVal;` |
|       15 |  4444 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4445 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4446 | `		r = a << b;` |
|        4 |  4447 | `	}else{` |
|        9 |  4448 | `		r = a >> b;` |
|        - |  4449 | `	}` |
|        - |  4450 | `	/* Push the result */` |
|       15 |  4451 | `	pNos->x.iVal = r;` |
|       15 |  4452 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4453 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4454 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4455 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4456 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4457 | `	}` |
|       15 |  4458 | `	VmPopOperand(&pTos,1);` |
|       15 |  4459 | `	break;` |
|        - |  4460 | `				 }` |
|        - |  4461 | `/* CAT:  P1 * *` |
|        - |  4462 | ` *` |
|        - |  4463 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4464 | ` * back.` |
|        - |  4465 | ` */` |
|    63080 |  4466 | `case PH7_OP_CAT:{` |
|        - |  4467 | `	ph7_value *pNos,*pCur;` |
|   126162 |  4468 | `	if( pInstr->iP1 < 1 ){` |
|    99130 |  4469 | `		pNos = &pTos[-1];` |
|    49566 |  4470 | `	}else{` |
|    27034 |  4471 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4472 | `	}` |
|        - |  4473 | `#ifdef UNTRUST` |
|        - |  4474 | `	if( pNos < pStack ){` |
|        - |  4475 | `		goto Abort;` |
|        - |  4476 | `	}` |
|        - |  4477 | `#endif` |
|        - |  4478 | `	/* Force a string cast */` |
|   126162 |  4479 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1190 |  4480 | `		PH7_MemObjToString(pNos);` |
|      594 |  4481 | `	}` |
|   126162 |  4482 | `	pCur = &pNos[1];` |
|   254346 |  4483 | `	while( pCur <= pTos ){` |
|   128186 |  4484 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50638 |  4485 | `			PH7_MemObjToString(pCur);` |
|    25318 |  4486 | `		}` |
|        - |  4487 | `		/* Perform the concatenation */` |
|   128186 |  4488 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   128148 |  4489 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64073 |  4490 | `		}` |
|   128186 |  4491 | `		SyBlobRelease(&pCur->sBlob);` |
|   128186 |  4492 | `		pCur++;` |
|        2 |  4493 | `	}` |
|   126162 |  4494 | `	pTos = pNos;` |
|   126162 |  4495 | `	break;` |
|        - |  4496 | `				}` |
|        - |  4497 | `/*  CAT_STORE: * * *` |
|        - |  4498 | ` *` |
|        - |  4499 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4500 | ` * back.` |
|        - |  4501 | ` */` |
|     3591 |  4502 | `case PH7_OP_CAT_STORE:{` |
|     7184 |  4503 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4504 | `	ph7_value *pObj;` |
|        - |  4505 | `#ifdef UNTRUST` |
|        - |  4506 | `	if( pNos < pStack ){` |
|        - |  4507 | `		goto Abort;` |
|        - |  4508 | `	}` |
|        - |  4509 | `#endif` |
|     7184 |  4510 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4511 | `		/* Force a string cast */` |
|      ! 0 |  4512 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4513 | `	}` |
|     7184 |  4514 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4515 | `		/* Force a string cast */` |
|      ! 0 |  4516 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4517 | `	}` |
|        - |  4518 | `	/* Perform the concatenation (Reverse order) */` |
|     7184 |  4519 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7184 |  4520 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3591 |  4521 | `	}` |
|        - |  4522 | `	/* Perform the store operation */` |
|     7184 |  4523 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4524 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7184 |  4525 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7184 |  4526 | `		PH7_MemObjStore(pTos,pObj);` |
|     3591 |  4527 | `	}` |
|     7184 |  4528 | `	PH7_MemObjStore(pTos,pNos);` |
|     7184 |  4529 | `	VmPopOperand(&pTos,1);` |
|     7184 |  4530 | `	break;` |
|        - |  4531 | `				}` |
|        - |  4532 | `/* OP_AND: * * *` |
|        - |  4533 | ` *` |
|        - |  4534 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4535 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4536 | ` * stack.` |
|        - |  4537 | ` */` |
|        - |  4538 | `/* OP_OR: * * *` |
|        - |  4539 | ` *` |
|        - |  4540 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4541 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4542 | ` * stack.` |
|        - |  4543 | ` */` |
|    94423 |  4544 | `case PH7_OP_LAND:` |
|        - |  4545 | `case PH7_OP_LOR: {` |
|   188892 |  4546 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4547 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4548 | `#ifdef UNTRUST` |
|        - |  4549 | `	if( pNos < pStack ){` |
|        - |  4550 | `		goto Abort;` |
|        - |  4551 | `	}` |
|        - |  4552 | `#endif` |
|        - |  4553 | `	/* Force a boolean cast */` |
|   188892 |  4554 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4555 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4556 | `	}` |
|   188892 |  4557 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4558 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4559 | `	}` |
|   188892 |  4560 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   188892 |  4561 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   188892 |  4562 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4563 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86526 |  4564 | `		v1 = and_logic[v1*3+v2];` |
|    43286 |  4565 | `	}else{` |
|        - |  4566 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102368 |  4567 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4568 | `	}` |
|   188892 |  4569 | `	if( v1 == 2 ){` |
|      ! 0 |  4570 | `		v1 = 1;` |
|      ! 0 |  4571 | `	}` |
|   188892 |  4572 | `	VmPopOperand(&pTos,1);` |
|   188892 |  4573 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   188892 |  4574 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   188892 |  4575 | `	break;` |
|        - |  4576 | `				 }` |
|        - |  4577 | `/* OP_LXOR: * * *` |
|        - |  4578 | ` *` |
|        - |  4579 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4580 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4581 | ` * stack.` |
|        - |  4582 | ` * According to the PHP language reference manual:` |
|        - |  4583 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4584 | ` *  TRUE,but not both.` |
|        - |  4585 | ` */` |
|        5 |  4586 | `case PH7_OP_LXOR:{` |
|       11 |  4587 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4588 | `	sxi32 v = 0;` |
|        - |  4589 | `#ifdef UNTRUST` |
|        - |  4590 | `	if( pNos < pStack ){` |
|        - |  4591 | `		goto Abort;` |
|        - |  4592 | `	}` |
|        - |  4593 | `#endif` |
|        - |  4594 | `	/* Force a boolean cast */` |
|       11 |  4595 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4596 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4597 | `	}` |
|       11 |  4598 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4599 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4600 | `	}` |
|       11 |  4601 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4602 | `		v = 1;` |
|        3 |  4603 | `	}` |
|       11 |  4604 | `	VmPopOperand(&pTos,1);` |
|       11 |  4605 | `	pTos->x.iVal = v;` |
|       11 |  4606 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4607 | `	break;` |
|        - |  4608 | `				 }` |
|        - |  4609 | `/* OP_EQ P1 P2 P3` |
|        - |  4610 | ` *` |
|        - |  4611 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4612 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4613 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4614 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4615 | ` */` |
|        - |  4616 | `/* OP_NEQ P1 P2 P3` |
|        - |  4617 | ` *` |
|        - |  4618 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4619 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4620 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4621 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4622 | ` */` |
|     3900 |  4623 | `case PH7_OP_EQ:` |
|        - |  4624 | `case PH7_OP_NEQ: {` |
|     7802 |  4625 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4626 | `	/* Perform the comparison and act accordingly */` |
|        - |  4627 | `#ifdef UNTRUST` |
|        - |  4628 | `	if( pNos < pStack ){` |
|        - |  4629 | `		goto Abort;` |
|        - |  4630 | `	}` |
|        - |  4631 | `#endif` |
|     7802 |  4632 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7802 |  4633 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4634 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7793 |  4635 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7758 |  4636 | `		rc = rc == 0;` |
|     3880 |  4637 | `	}else{` |
|       28 |  4638 | `		rc = rc != 0;` |
|        - |  4639 | `	}` |
|     7802 |  4640 | `	VmPopOperand(&pTos,1);` |
|     7802 |  4641 | `	if( !pInstr->iP2 ){` |
|        - |  4642 | `		/* Push comparison result without taking the jump */` |
|     7802 |  4643 | `		PH7_MemObjRelease(pTos);` |
|     7802 |  4644 | `		pTos->x.iVal = rc;` |
|        - |  4645 | `		/* Invalidate any prior representation */` |
|     7802 |  4646 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3902 |  4647 | `	}else{` |
|      ! 0 |  4648 | `		if( rc ){` |
|        - |  4649 | `			/* Jump to the desired location */` |
|      ! 0 |  4650 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4651 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4652 | `		}` |
|        - |  4653 | `	}` |
|     7802 |  4654 | `	break;` |
|        - |  4655 | `				 }` |
|        - |  4656 | `/* OP_TEQ P1 P2 *` |
|        - |  4657 | ` *` |
|        - |  4658 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4659 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4660 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4661 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4662 | ` */` |
|   131791 |  4663 | `case PH7_OP_TEQ: {` |
|   263584 |  4664 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4665 | `	/* Perform the comparison and act accordingly */` |
|        - |  4666 | `#ifdef UNTRUST` |
|        - |  4667 | `	if( pNos < pStack ){` |
|        - |  4668 | `		goto Abort;` |
|        - |  4669 | `	}` |
|        - |  4670 | `#endif` |
|   263584 |  4671 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   263584 |  4672 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4673 | `		rc = 0;` |
|        2 |  4674 | `	}else{` |
|   263582 |  4675 | `		rc = rc == 0;` |
|        - |  4676 | `	}` |
|   263584 |  4677 | `	VmPopOperand(&pTos,1);` |
|   263584 |  4678 | `	if( !pInstr->iP2 ){` |
|        - |  4679 | `		/* Push comparison result without taking the jump */` |
|   263584 |  4680 | `		PH7_MemObjRelease(pTos);` |
|   263584 |  4681 | `		pTos->x.iVal = rc;` |
|        - |  4682 | `		/* Invalidate any prior representation */` |
|   263584 |  4683 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   131793 |  4684 | `	}else{` |
|      ! 0 |  4685 | `		if( rc ){` |
|        - |  4686 | `			/* Jump to the desired location */` |
|      ! 0 |  4687 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4688 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4689 | `		}` |
|        - |  4690 | `	}` |
|   263584 |  4691 | `	break;` |
|        - |  4692 | `				 }` |
|        - |  4693 | `/* OP_TNE P1 P2 *` |
|        - |  4694 | ` *` |
|        - |  4695 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4696 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4697 | ` * instruction.` |
|        - |  4698 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4699 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4700 | ` *` |
|        - |  4701 | ` */` |
|   102566 |  4702 | `case PH7_OP_TNE: {` |
|   205134 |  4703 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4704 | `	/* Perform the comparison and act accordingly */` |
|        - |  4705 | `#ifdef UNTRUST` |
|        - |  4706 | `	if( pNos < pStack ){` |
|        - |  4707 | `		goto Abort;` |
|        - |  4708 | `	}` |
|        - |  4709 | `#endif` |
|   205134 |  4710 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   205134 |  4711 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4712 | `		rc = 1;` |
|        2 |  4713 | `	}else{` |
|   205132 |  4714 | `		rc = rc != 0;` |
|        - |  4715 | `	}` |
|   205134 |  4716 | `	VmPopOperand(&pTos,1);` |
|   205134 |  4717 | `	if( !pInstr->iP2 ){` |
|        - |  4718 | `		/* Push comparison result without taking the jump */` |
|   205134 |  4719 | `		PH7_MemObjRelease(pTos);` |
|   205134 |  4720 | `		pTos->x.iVal = rc;` |
|        - |  4721 | `		/* Invalidate any prior representation */` |
|   205134 |  4722 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102568 |  4723 | `	}else{` |
|      ! 0 |  4724 | `		if( rc ){` |
|        - |  4725 | `			/* Jump to the desired location */` |
|      ! 0 |  4726 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4727 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4728 | `		}` |
|        - |  4729 | `	}` |
|   205134 |  4730 | `	break;` |
|        - |  4731 | `				 }` |
|        - |  4732 | `/* OP_LT P1 P2 P3` |
|        - |  4733 | ` *` |
|        - |  4734 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4735 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4736 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4737 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4738 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4739 | ` *` |
|        - |  4740 | ` */` |
|        - |  4741 | `/* OP_LE P1 P2 P3` |
|        - |  4742 | ` *` |
|        - |  4743 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4744 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4745 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4746 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4747 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4748 | ` *` |
|        - |  4749 | ` */` |
|   102443 |  4750 | `case PH7_OP_LT:` |
|        - |  4751 | `case PH7_OP_LE: {` |
|   204932 |  4752 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4753 | `	/* Perform the comparison and act accordingly */` |
|        - |  4754 | `#ifdef UNTRUST` |
|        - |  4755 | `	if( pNos < pStack ){` |
|        - |  4756 | `		goto Abort;` |
|        - |  4757 | `	}` |
|        - |  4758 | `#endif` |
|   204932 |  4759 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204932 |  4760 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4761 | `		rc = 0;` |
|   204928 |  4762 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      430 |  4763 | `		rc = rc < 1;` |
|      216 |  4764 | `	}else{` |
|   204496 |  4765 | `		rc = rc < 0;` |
|        - |  4766 | `	}` |
|   204932 |  4767 | `	VmPopOperand(&pTos,1);` |
|   204932 |  4768 | `	if( !pInstr->iP2 ){` |
|        - |  4769 | `		/* Push comparison result without taking the jump */` |
|   204932 |  4770 | `		PH7_MemObjRelease(pTos);` |
|   204932 |  4771 | `		pTos->x.iVal = rc;` |
|        - |  4772 | `		/* Invalidate any prior representation */` |
|   204932 |  4773 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102489 |  4774 | `	}else{` |
|      ! 0 |  4775 | `		if( rc ){` |
|        - |  4776 | `			/* Jump to the desired location */` |
|      ! 0 |  4777 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4778 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4779 | `		}` |
|        - |  4780 | `	}` |
|   204932 |  4781 | `	break;` |
|        - |  4782 | `				}` |
|        - |  4783 | `/* OP_GT P1 P2 P3` |
|        - |  4784 | ` *` |
|        - |  4785 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4786 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4787 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4788 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4789 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4790 | ` *` |
|        - |  4791 | ` */` |
|        - |  4792 | `/* OP_GE P1 P2 P3` |
|        - |  4793 | ` *` |
|        - |  4794 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4795 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4796 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4797 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4798 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4799 | ` *` |
|        - |  4800 | ` */` |
|    48771 |  4801 | `case PH7_OP_GT:` |
|        - |  4802 | `case PH7_OP_GE: {` |
|    97544 |  4803 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4804 | `	/* Perform the comparison and act accordingly */` |
|        - |  4805 | `#ifdef UNTRUST` |
|        - |  4806 | `	if( pNos < pStack ){` |
|        - |  4807 | `		goto Abort;` |
|        - |  4808 | `	}` |
|        - |  4809 | `#endif` |
|    97544 |  4810 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97544 |  4811 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4812 | `		rc = 0;` |
|    97540 |  4813 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97388 |  4814 | `		rc = rc >= 0;` |
|    48695 |  4815 | `	}else{` |
|      150 |  4816 | `		rc = rc > 0;` |
|        - |  4817 | `	}` |
|    97544 |  4818 | `	VmPopOperand(&pTos,1);` |
|    97544 |  4819 | `	if( !pInstr->iP2 ){` |
|        - |  4820 | `		/* Push comparison result without taking the jump */` |
|    97544 |  4821 | `		PH7_MemObjRelease(pTos);` |
|    97544 |  4822 | `		pTos->x.iVal = rc;` |
|        - |  4823 | `		/* Invalidate any prior representation */` |
|    97544 |  4824 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48773 |  4825 | `	}else{` |
|      ! 0 |  4826 | `		if( rc ){` |
|        - |  4827 | `			/* Jump to the desired location */` |
|      ! 0 |  4828 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4829 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4830 | `		}` |
|        - |  4831 | `	}` |
|    97544 |  4832 | `	break;` |
|        - |  4833 | `				}` |
|        - |  4834 | `/* OP_SEQ P1 P2 *` |
|        - |  4835 | ` * Strict string comparison.` |
|        - |  4836 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4837 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4838 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4839 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4840 | ` * use PH7_OP_EQ.` |
|        - |  4841 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4842 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4843 | ` */` |
|        - |  4844 | `/* OP_SNE P1 P2 *` |
|        - |  4845 | ` * Strict string comparison.` |
|        - |  4846 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4847 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4848 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4849 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4850 | ` * use PH7_OP_EQ.` |
|        - |  4851 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4852 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4853 | ` */` |
|       18 |  4854 | `case PH7_OP_SEQ:` |
|        - |  4855 | `case PH7_OP_SNE: {` |
|       38 |  4856 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4857 | `	SyString s1,s2;` |
|        - |  4858 | `	/* Perform the comparison and act accordingly */` |
|        - |  4859 | `#ifdef UNTRUST` |
|        - |  4860 | `	if( pNos < pStack ){` |
|        - |  4861 | `		goto Abort;` |
|        - |  4862 | `	}` |
|        - |  4863 | `#endif` |
|        - |  4864 | `	/* Force a string cast */` |
|       38 |  4865 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4866 | `		PH7_MemObjToString(pTos);` |
|        2 |  4867 | `	}` |
|       38 |  4868 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4869 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4870 | `	}` |
|       38 |  4871 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4872 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4873 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4874 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4875 | `		rc = rc != 0;` |
|      ! 0 |  4876 | `	}else{` |
|       38 |  4877 | `		rc = rc == 0;` |
|        - |  4878 | `	}` |
|       38 |  4879 | `	VmPopOperand(&pTos,1);` |
|       38 |  4880 | `	if( !pInstr->iP2 ){` |
|        - |  4881 | `		/* Push comparison result without taking the jump */` |
|       38 |  4882 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4883 | `		pTos->x.iVal = rc;` |
|        - |  4884 | `		/* Invalidate any prior representation */` |
|       38 |  4885 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4886 | `	}else{` |
|      ! 0 |  4887 | `		if( rc ){` |
|        - |  4888 | `			/* Jump to the desired location */` |
|      ! 0 |  4889 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4890 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4891 | `		}` |
|        - |  4892 | `	}` |
|       38 |  4893 | `	break;` |
|        - |  4894 | `				 }` |
|        - |  4895 | `/*` |
|        - |  4896 | ` * OP_LOAD_REF * * *` |
|        - |  4897 | ` * Push the index of a referenced object on the stack.` |
|        - |  4898 | ` */` |
|       57 |  4899 | `case PH7_OP_LOAD_REF: {` |
|        - |  4900 | `	sxu32 nIdx;` |
|        - |  4901 | `#ifdef UNTRUST` |
|        - |  4902 | `	if( pTos < pStack ){` |
|        - |  4903 | `		goto Abort;` |
|        - |  4904 | `	}` |
|        - |  4905 | `#endif` |
|        - |  4906 | `	/* Extract memory object index */` |
|      115 |  4907 | `	nIdx = pTos->nIdx;` |
|      115 |  4908 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4909 | `		/* Nullify the object */` |
|       95 |  4910 | `		PH7_MemObjRelease(pTos);` |
|        - |  4911 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4912 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4913 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4914 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4915 | `	}` |
|      115 |  4916 | `	break;` |
|        - |  4917 | `					  }` |
|        - |  4918 | `/*` |
|        - |  4919 | ` * OP_STORE_REF * * P3` |
|        - |  4920 | ` * Perform an assignment operation by reference.` |
|        - |  4921 | ` */` |
|       15 |  4922 | ` case PH7_OP_STORE_REF: {` |
|       32 |  4923 | `	 SyString sName = { 0 , 0 };` |
|        - |  4924 | `	 VmFrame *pFrameLocal;` |
|        - |  4925 | `	SyHashEntry *pEntry;` |
|        - |  4926 | `	sxu32 nIdx;` |
|        - |  4927 | `#ifdef UNTRUST` |
|        - |  4928 | `	if( pTos < pStack ){` |
|        - |  4929 | `		goto Abort;` |
|        - |  4930 | `	}` |
|        - |  4931 | `#endif` |
|       32 |  4932 | `	if( pInstr->p3 == 0 ){` |
|        - |  4933 | `		char *zName;` |
|        - |  4934 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4935 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4936 | `			/* Force a string cast */` |
|      ! 0 |  4937 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4938 | `		}` |
|      ! 0 |  4939 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4940 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4941 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4942 | `			if( zName ){` |
|      ! 0 |  4943 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4944 | `			}` |
|      ! 0 |  4945 | `		}` |
|      ! 0 |  4946 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4947 | `		pTos--;` |
|      ! 0 |  4948 | `	}else{` |
|       32 |  4949 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4950 | `	}` |
|       32 |  4951 | `	nIdx = pTos->nIdx;` |
|       32 |  4952 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4953 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4954 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4955 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4956 | `		}else{` |
|        - |  4957 | `			ph7_value *pObj;` |
|        - |  4958 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4959 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4960 | `			if( pObj == 0 ){` |
|      ! 0 |  4961 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4962 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4963 | `				goto Abort;` |
|        - |  4964 | `			}` |
|        - |  4965 | `			/* Perform the store operation */` |
|      ! 0 |  4966 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4967 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4968 | `		}` |
|       32 |  4969 | `	}else if( sName.nByte > 0){` |
|       32 |  4970 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4971 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4972 | `		}else{` |
|       32 |  4973 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  4974 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4975 | `			/* Query the local frame */` |
|       32 |  4976 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  4977 | `			if( pEntry ){` |
|      ! 0 |  4978 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4979 | `			}else{` |
|       32 |  4980 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  4981 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4982 | `					/* Insert in the $GLOBALS array */` |
|       28 |  4983 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  4984 | `				}` |
|       32 |  4985 | `				if( rc == SXRET_OK ){` |
|       32 |  4986 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  4987 | `				}` |
|        - |  4988 | `			}` |
|        - |  4989 | `		}` |
|       15 |  4990 | `	}` |
|       32 |  4991 | `	break;` |
|        - |  4992 | `				 }` |
|        - |  4993 | `/*` |
|        - |  4994 | ` * OP_UPLINK P1 * *` |
|        - |  4995 | ` * Link a variable to the top active VM frame.` |
|        - |  4996 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4997 | ` */` |
|       25 |  4998 | `case PH7_OP_UPLINK: {` |
|       52 |  4999 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5000 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5001 | `		SyString sName;` |
|        - |  5002 | `		/* Perform the link */` |
|      104 |  5003 | `		while( pLink <= pTos ){` |
|       54 |  5004 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5005 | `				/* Force a string cast */` |
|      ! 0 |  5006 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5007 | `			}` |
|       54 |  5008 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5009 | `			if( sName.nByte > 0 ){` |
|       54 |  5010 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5011 | `			}` |
|       54 |  5012 | `			pLink++;` |
|        2 |  5013 | `		}` |
|       25 |  5014 | `	}` |
|       52 |  5015 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5016 | `	break;` |
|        - |  5017 | `					}` |
|        - |  5018 | `/*` |
|        - |  5019 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5020 | ` * Push an exception in the corresponding container so that` |
|        - |  5021 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5022 | ` */` |
|       32 |  5023 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5024 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5025 | `	VmFrame *pFrameLocal;` |
|        - |  5026 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5027 | `	pException->iFinallyDone = 0;` |
|       66 |  5028 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5029 | `	/* Create the exception frame */` |
|       66 |  5030 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5031 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5032 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5033 | `		goto Abort;` |
|        - |  5034 | `	}` |
|        - |  5035 | `	/* Mark the special frame */` |
|       66 |  5036 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5037 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5038 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5039 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5040 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5041 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5042 | `	break;` |
|        - |  5043 | `							}` |
|        - |  5044 | `/*` |
|        - |  5045 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5046 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5047 | ` */` |
|       31 |  5048 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5049 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5050 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5051 | `		ph7_exception **apException;` |
|        - |  5052 | `		/* Pop the loaded exception */` |
|       28 |  5053 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5054 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5055 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5056 | `		}` |
|       13 |  5057 | `	}` |
|       64 |  5058 | `	pException->pFrame = 0;` |
|        - |  5059 | `	/* Leave the exception frame */` |
|       64 |  5060 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5061 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5062 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5063 | `		sxi32 rcFinally;` |
|       19 |  5064 | `		pException->iFinallyDone = 1;` |
|       19 |  5065 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  5066 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5067 | `			goto Abort;` |
|        - |  5068 | `		}` |
|        9 |  5069 | `	}` |
|       64 |  5070 | `	break;` |
|        - |  5071 | `							}` |
|        - |  5072 |  |
|        - |  5073 | `/*` |
|        - |  5074 | ` * OP_THROW * P2 *` |
|        - |  5075 | ` * Throw an user exception.` |
|        - |  5076 | ` */` |
|       18 |  5077 | `case PH7_OP_THROW: {` |
|       38 |  5078 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5079 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5080 | `#ifdef UNTRUST` |
|        - |  5081 | `	if( pTos < pStack ){` |
|        - |  5082 | `		goto Abort;` |
|        - |  5083 | `	}` |
|        - |  5084 | `#endif` |
|       38 |  5085 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5086 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5087 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5088 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5089 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5090 | `		ph7_class *pException;` |
|        - |  5091 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5092 | `		 */` |
|       38 |  5093 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5094 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5095 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5096 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5097 | `			if( rc == SXERR_ABORT ){` |
|        - |  5098 | `				/* Abort processing immediately */` |
|      ! 0 |  5099 | `				goto Abort;` |
|        - |  5100 | `			}` |
|      ! 0 |  5101 | `		}else{` |
|        - |  5102 | `			/* Throw the exception */` |
|       38 |  5103 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5104 | `			if( rc == SXERR_ABORT ){` |
|        - |  5105 | `				/* Abort processing immediately */` |
|        9 |  5106 | `				goto Abort;` |
|        - |  5107 | `			}` |
|        - |  5108 | `		}` |
|       16 |  5109 | `	}else{` |
|        - |  5110 | `		/* Expecting a class instance */` |
|      ! 0 |  5111 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5112 | `		if( rc == SXERR_ABORT ){` |
|        - |  5113 | `			/* Abort processing immediately */` |
|      ! 0 |  5114 | `			goto Abort;` |
|        - |  5115 | `		}` |
|        - |  5116 | `	}` |
|        - |  5117 | `	/* Pop the top entry */` |
|       30 |  5118 | `	VmPopOperand(&pTos,1);` |
|        - |  5119 | `	/* Perform an unconditional jump */` |
|       30 |  5120 | `	pc = nJump - 1;` |
|       30 |  5121 | `	break;` |
|        - |  5122 | `				   }` |
|        - |  5123 | `/*` |
|        - |  5124 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5125 | ` * Prepare a foreach step.` |
|        - |  5126 | ` */` |
|     4935 |  5127 | `case PH7_OP_FOREACH_INIT: {` |
|     9872 |  5128 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5129 | `	void *pName;` |
|        - |  5130 | `#ifdef UNTRUST` |
|        - |  5131 | `	if( pTos < pStack ){` |
|        - |  5132 | `		goto Abort;` |
|        - |  5133 | `	}` |
|        - |  5134 | `#endif` |
|     9872 |  5135 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5136 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5137 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5138 | `			/* Force a string cast */` |
|      ! 0 |  5139 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5140 | `		}` |
|        - |  5141 | `		/* Duplicate name */` |
|      ! 0 |  5142 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5143 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5144 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5145 | `		}` |
|      ! 0 |  5146 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5147 | `	}` |
|     9872 |  5148 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5149 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5150 | `			/* Force a string cast */` |
|      ! 0 |  5151 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5152 | `		}` |
|        - |  5153 | `		/* Duplicate name */` |
|      ! 0 |  5154 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5155 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5156 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5157 | `		}` |
|      ! 0 |  5158 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5159 | `	}` |
|        - |  5160 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9872 |  5161 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5162 | `		/* Jump out of the loop */` |
|      ! 0 |  5163 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5164 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5165 | `		}` |
|      ! 0 |  5166 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5167 | `	}else{` |
|        - |  5168 | `		ph7_foreach_step *pStep;` |
|     9872 |  5169 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9872 |  5170 | `		if( pStep == 0 ){` |
|      ! 0 |  5171 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5172 | `			/* Jump out of the loop */` |
|      ! 0 |  5173 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5174 | `		}else{` |
|        - |  5175 | `			/* Zero the structure */` |
|     9872 |  5176 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5177 | `			/* Prepare the step */` |
|     9872 |  5178 | `			pStep->iFlags = pInfo->iFlags;` |
|     9872 |  5179 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5180 | `				ph7_hashmap *pMap;` |
|        - |  5181 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5182 | `				 * source array so mutations don't affect other sharers. */` |
|     9844 |  5183 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5184 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5185 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5186 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5187 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5188 | `						 * variable still points at the same hashmap as` |
|        - |  5189 | `						 * the stack value. */` |
|       10 |  5190 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5191 | `							pCur->iRef--;` |
|       10 |  5192 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5193 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5194 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5195 | `						}` |
|        4 |  5196 | `					}` |
|        4 |  5197 | `				}` |
|     9844 |  5198 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5199 | `				/* Reset the internal loop cursor */` |
|     9844 |  5200 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5201 | `				/* Mark the step */` |
|     9844 |  5202 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9844 |  5203 | `				pStep->xIter.pMap = pMap;` |
|     9844 |  5204 | `				pMap->iRef++;` |
|     4923 |  5205 | `			}else{` |
|       30 |  5206 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5207 | `				ph7_class *pIteratorClass;` |
|        - |  5208 | `				/* Check if the object implements Iterator */` |
|       30 |  5209 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5210 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5211 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5212 | `					ph7_class_method *pRewind;` |
|       19 |  5213 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       19 |  5214 | `					pStep->xIter.pThis = pThis;` |
|       19 |  5215 | `					pThis->iRef++;` |
|       19 |  5216 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       19 |  5217 | `					if( pRewind ){` |
|       19 |  5218 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5219 | `					}` |
|       10 |  5220 | `				}else{` |
|        - |  5221 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5222 | `					ph7_class *pIterAggClass;` |
|       12 |  5223 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5224 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5225 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5226 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5227 | `						ph7_class_method *pGetIter;` |
|        3 |  5228 | `						int iterAggOk = 0;` |
|        3 |  5229 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5230 | `						if( pGetIter ){` |
|        - |  5231 | `							ph7_value sResult;` |
|        3 |  5232 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5233 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5234 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5235 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5236 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5237 | `									ph7_class_method *pRewind;` |
|        3 |  5238 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5239 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5240 | `									pIterObj->iRef++;` |
|        - |  5241 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5242 | `									pStep->pOwner = pThis;` |
|        3 |  5243 | `									pThis->iRef++;` |
|        3 |  5244 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5245 | `									if( pRewind ){` |
|        3 |  5246 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5247 | `									}` |
|        3 |  5248 | `									iterAggOk = 1;` |
|        1 |  5249 | `								}` |
|        1 |  5250 | `							}` |
|        3 |  5251 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5252 | `						}` |
|        3 |  5253 | `						if( !iterAggOk ){` |
|        - |  5254 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5255 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5256 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5257 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5258 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5259 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5260 | `						}` |
|        2 |  5261 | `					}else{` |
|        - |  5262 | `						/* Plain object iteration via hAttr */` |
|        9 |  5263 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5264 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5265 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5266 | `						pThis->iRef++;` |
|        - |  5267 | `					}` |
|        - |  5268 | `				}` |
|        - |  5269 | `			}` |
|        - |  5270 | `		}` |
|     9872 |  5271 | `		if( pStep ){` |
|     9872 |  5272 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5273 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5274 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5275 | `				/* Jump out of the loop */` |
|      ! 0 |  5276 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5277 | `			}` |
|     4935 |  5278 | `		}` |
|        - |  5279 | `	}` |
|     9872 |  5280 | `	VmPopOperand(&pTos,1);` |
|     9872 |  5281 | `	break;` |
|        - |  5282 | `						  }` |
|        - |  5283 | `/*` |
|        - |  5284 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5285 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5286 | ` */` |
|    79607 |  5287 | `case PH7_OP_FOREACH_STEP: {` |
|   159216 |  5288 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5289 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5290 | `	ph7_value *pValue;` |
|        - |  5291 | `	VmFrame *pFrameLocal;` |
|        - |  5292 | `	/* Peek the last step */` |
|   159216 |  5293 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   159216 |  5294 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   159216 |  5295 | `	pFrameLocal = pVm->pFrame;` |
|   159216 |  5296 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   159216 |  5297 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   159104 |  5298 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5299 | `		ph7_hashmap_node *pNode;` |
|        - |  5300 | `		/* Extract the current node value */` |
|   159104 |  5301 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   159104 |  5302 | `		if( pNode == 0 ){` |
|        - |  5303 | `			/* No more entry to process */` |
|     9842 |  5304 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9842 |  5305 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5306 | `				/* Break the reference with the last element */` |
|        7 |  5307 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5308 | `			}` |
|        - |  5309 | `			/* Automatically reset the loop cursor */` |
|     9842 |  5310 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5311 | `			/* Cleanup the mess left behind */` |
|     9842 |  5312 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9842 |  5313 | `			SySetPop(&pInfo->aStep);` |
|     9842 |  5314 | `			PH7_HashmapUnref(pMap);` |
|     4922 |  5315 | `		}else{` |
|   149264 |  5316 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5317 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5318 | `				if( pKey ){` |
|      416 |  5319 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5320 | `				}` |
|      207 |  5321 | `			}` |
|   149264 |  5322 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5323 | `				SyHashEntry *pEntry;` |
|        - |  5324 | `				/* Pass by reference */` |
|       24 |  5325 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5326 | `				if( pEntry ){` |
|       22 |  5327 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5328 | `				}else{` |
|        4 |  5329 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5330 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5331 | `				}` |
|       13 |  5332 | `			}else{` |
|        - |  5333 | `				/* Make a copy of the entry value */` |
|   149242 |  5334 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   149242 |  5335 | `				if( pValue ){` |
|   149242 |  5336 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    74620 |  5337 | `				}` |
|        - |  5338 | `			}` |
|        2 |  5339 | `		}` |
|    79665 |  5340 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5341 | `		/* Iterator-based iteration.` |
|        - |  5342 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5343 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5344 | `		 */` |
|       89 |  5345 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5346 | `		ph7_class_method *pMethod;` |
|        - |  5347 | `		ph7_value sResult;` |
|       89 |  5348 | `		int isValid = 0;` |
|        - |  5349 | `		/* Call next() to advance — but skip on the first iteration */` |
|       89 |  5350 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       21 |  5351 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       11 |  5352 | `		}else{` |
|       69 |  5353 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       69 |  5354 | `			if( pMethod ){` |
|       69 |  5355 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5356 | `			}` |
|        - |  5357 | `		}` |
|        - |  5358 | `		/* Call valid() */` |
|       89 |  5359 | `		PH7_MemObjInit(pVm,&sResult);` |
|       89 |  5360 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       89 |  5361 | `		if( pMethod ){` |
|       89 |  5362 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       89 |  5363 | `			PH7_MemObjToBool(&sResult);` |
|       89 |  5364 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5365 | `		}` |
|       89 |  5366 | `		PH7_MemObjRelease(&sResult);` |
|       89 |  5367 | `		if( !isValid ){` |
|        - |  5368 | `			/* Iterator exhausted */` |
|       19 |  5369 | `			pc = pInstr->iP2 - 1;` |
|        - |  5370 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       19 |  5371 | `			if( pStep->pOwner ){` |
|        3 |  5372 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5373 | `			}` |
|       19 |  5374 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       19 |  5375 | `			SySetPop(&pInfo->aStep);` |
|       19 |  5376 | `			PH7_ClassInstanceUnref(pThis);` |
|       10 |  5377 | `		}else{` |
|        - |  5378 | `			/* Call current() to get value */` |
|       71 |  5379 | `			PH7_MemObjInit(pVm,&sResult);` |
|       71 |  5380 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       71 |  5381 | `			if( pMethod ){` |
|       71 |  5382 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5383 | `			}` |
|       71 |  5384 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       71 |  5385 | `			if( pValue ){` |
|       71 |  5386 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5387 | `			}` |
|       71 |  5388 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5389 | `			/* Call key() if needed */` |
|       71 |  5390 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5391 | `				ph7_value sKey;` |
|       35 |  5392 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5393 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5394 | `				if( pMethod ){` |
|       35 |  5395 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5396 | `				}` |
|       35 |  5397 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5398 | `				if( pValue ){` |
|       35 |  5399 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5400 | `				}` |
|       35 |  5401 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5402 | `			}` |
|        - |  5403 | `		}` |
|       45 |  5404 | `	}else{` |
|       25 |  5405 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5406 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5407 | `		SyHashEntry *pEntry;` |
|        - |  5408 | `		/* Point to the next attribute */` |
|       29 |  5409 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5410 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5411 | `			/* Check access permission */` |
|       31 |  5412 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5413 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5414 | `					break; /* Access is granted */` |
|        - |  5415 | `			}` |
|        1 |  5416 | `		}` |
|       25 |  5417 | `		if( pEntry == 0 ){` |
|        - |  5418 | `			/* Clean up the mess left behind */` |
|        9 |  5419 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5420 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5421 | `				/* Break the reference with the last element */` |
|        3 |  5422 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5423 | `			}` |
|        9 |  5424 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5425 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5426 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5427 | `		}else{` |
|       17 |  5428 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5429 | `			ph7_value *pAttrValue;` |
|       17 |  5430 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5431 | `				/* Fill with the current attribute name */` |
|       17 |  5432 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5433 | `				if( pKey ){` |
|       17 |  5434 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5435 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5436 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5437 | `				}` |
|        8 |  5438 | `			}` |
|        - |  5439 | `			/* Extract attribute value */` |
|       17 |  5440 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5441 | `			if( pAttrValue ){` |
|       17 |  5442 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5443 | `					/* Pass by reference */` |
|        3 |  5444 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5445 | `					if( pEntry ){` |
|        3 |  5446 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5447 | `					}else{` |
|      ! 0 |  5448 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5449 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5450 | `					}` |
|        2 |  5451 | `				}else{` |
|        - |  5452 | `					/* Make a copy of the attribute value */` |
|       15 |  5453 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5454 | `					if( pValue ){` |
|       15 |  5455 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5456 | `					}` |
|        - |  5457 | `				}` |
|        8 |  5458 | `			}` |
|        - |  5459 | `		}` |
|        - |  5460 | `	}` |
|   159216 |  5461 | `	break;` |
|        - |  5462 | `						  }` |
|        - |  5463 | `/*` |
|        - |  5464 | ` * OP_MEMBER P1 P2` |
|        - |  5465 | ` * Load class attribute/method on the stack.` |
|        - |  5466 | ` */` |
|     2180 |  5467 | `case PH7_OP_MEMBER: {` |
|        - |  5468 | `	ph7_class_instance *pThis;` |
|        - |  5469 | `	ph7_value *pNos;` |
|        - |  5470 | `	SyString sName;` |
|     4362 |  5471 | `	if( !pInstr->iP1 ){` |
|     4226 |  5472 | `		pNos = &pTos[-1];` |
|        - |  5473 | `#ifdef UNTRUST` |
|        - |  5474 | `		if( pNos < pStack ){` |
|        - |  5475 | `			goto Abort;` |
|        - |  5476 | `		}` |
|        - |  5477 | `#endif` |
|     4226 |  5478 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5479 | `			ph7_class *pClass;` |
|        - |  5480 | `			/* Class already instantiated */` |
|     4226 |  5481 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5482 | `			/* Point to the instantiated class */` |
|     4226 |  5483 | `			pClass = pThis->pClass;` |
|        - |  5484 | `			/* Extract attribute name first */` |
|     4226 |  5485 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4226 |  5486 | `			if( pInstr->iP2 ){` |
|        - |  5487 | `				/* Method call */` |
|      406 |  5488 | `				ph7_class_method *pMeth = 0;` |
|      406 |  5489 | `				if( sName.nByte > 0 ){` |
|        - |  5490 | `					/* Extract the target method */` |
|      406 |  5491 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      202 |  5492 | `				}` |
|      406 |  5493 | `				if( pMeth == 0 ){` |
|      ! 0 |  5494 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5495 | `						&pClass->sName,&sName` |
|        - |  5496 | `						);` |
|        - |  5497 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5498 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5499 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5500 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5501 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5502 | `				}else{` |
|        - |  5503 | `					/* Push method name on the stack */` |
|      406 |  5504 | `					PH7_MemObjRelease(pTos);` |
|      406 |  5505 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      406 |  5506 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5507 | `				}` |
|      406 |  5508 | `				pTos->nIdx = SXU32_HIGH;` |
|      204 |  5509 | `			}else{` |
|        - |  5510 | `				/* Attribute access */` |
|     3822 |  5511 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5512 | `				SyHashEntry *pEntry;` |
|        - |  5513 | `				/* Extract the target attribute */` |
|     3822 |  5514 | `				if( sName.nByte > 0 ){` |
|     3822 |  5515 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3822 |  5516 | `					if( pEntry ){` |
|        - |  5517 | `						/* Point to the attribute value */` |
|     3820 |  5518 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1909 |  5519 | `					}` |
|     1910 |  5520 | `				}` |
|     3822 |  5521 | `				if( pObjAttr == 0 ){` |
|        - |  5522 | `					/* No such attribute,load null */` |
|        4 |  5523 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5524 | `						&pClass->sName,&sName);` |
|        - |  5525 | `					/* Call the __get magic method if available */` |
|        3 |  5526 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5527 | `				}` |
|     3822 |  5528 | `				VmPopOperand(&pTos,1);` |
|        - |  5529 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5530 | `				 * This is due to the following case:` |
|        - |  5531 | `				 *     (new TestClass())->foo;` |
|        - |  5532 | `				 */` |
|     3822 |  5533 | `				pThis->iRef++;` |
|     3822 |  5534 | `				PH7_MemObjRelease(pTos);` |
|     3822 |  5535 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3822 |  5536 | `				if( pObjAttr ){` |
|     3820 |  5537 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5538 | `					/* Check attribute access */` |
|     3820 |  5539 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5540 | `						/* Load attribute */` |
|     3820 |  5541 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3820 |  5542 | `						if( pValue ){` |
|     3820 |  5543 | `							if( pThis->iRef < 2 ){` |
|        - |  5544 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5545 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5546 | `								 */` |
|        3 |  5547 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5548 | `							}else{` |
|        - |  5549 | `								/* Simple load */` |
|     3818 |  5550 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5551 | `							}` |
|     3820 |  5552 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3818 |  5553 | `								if( pThis->iRef > 1 ){` |
|        - |  5554 | `									/* Load attribute index */` |
|     3816 |  5555 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1907 |  5556 | `								}` |
|     1908 |  5557 | `							}` |
|     1909 |  5558 | `						}` |
|     1909 |  5559 | `					}` |
|     1909 |  5560 | `				}` |
|        - |  5561 | `				/* Safely unreference the object */` |
|     3822 |  5562 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5563 | `			}` |
|     2114 |  5564 | `		}else{` |
|      ! 0 |  5565 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5566 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5567 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5568 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5569 | `		}` |
|     2114 |  5570 | `	}else{` |
|        - |  5571 | `		/* Static member access using class name */` |
|      138 |  5572 | `		pNos = pTos;` |
|      138 |  5573 | `		pThis = 0;` |
|      138 |  5574 | `		if( !pInstr->p3 ){` |
|      126 |  5575 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      126 |  5576 | `			pNos--;` |
|        - |  5577 | `#ifdef UNTRUST` |
|        - |  5578 | `			if( pNos < pStack ){` |
|        - |  5579 | `				goto Abort;` |
|        - |  5580 | `			}` |
|        - |  5581 | `#endif` |
|       64 |  5582 | `		}else{` |
|        - |  5583 | `			/* Attribute name already computed */` |
|       14 |  5584 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5585 | `		}` |
|      138 |  5586 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      138 |  5587 | `			ph7_class *pClass = 0;` |
|      138 |  5588 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5589 | `				/* Class already instantiated */` |
|      ! 0 |  5590 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5591 | `				pClass = pThis->pClass;` |
|      ! 0 |  5592 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5593 | `			}else{` |
|        - |  5594 | `				/* Try to extract the target class */` |
|      138 |  5595 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      138 |  5596 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      138 |  5597 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5598 | `					/* Handle self/static/parent keywords */` |
|      138 |  5599 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5600 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5601 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5602 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5603 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5604 | `						}` |
|      124 |  5605 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5606 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      109 |  5607 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5608 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5609 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5610 | `							pClass = pSelf->pBase;` |
|        6 |  5611 | `						}` |
|        8 |  5612 | `					}else{` |
|       84 |  5613 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5614 | `					}` |
|       68 |  5615 | `				}` |
|        - |  5616 | `			}` |
|      138 |  5617 | `			if( pClass == 0 ){` |
|        - |  5618 | `				/* Undefined class */` |
|      ! 0 |  5619 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5620 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5621 | `					);` |
|      ! 0 |  5622 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5623 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5624 | `				}` |
|      ! 0 |  5625 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5626 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5627 | `			}else{` |
|      138 |  5628 | `				if( pInstr->iP2 ){` |
|        - |  5629 | `					/* Method call */` |
|       68 |  5630 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5631 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5632 | `						/* Extract the target method */` |
|       68 |  5633 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5634 | `					}` |
|       68 |  5635 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5636 | `						if( pMeth ){` |
|      ! 0 |  5637 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5638 | `								&pClass->sName,&sName` |
|        - |  5639 | `								);` |
|      ! 0 |  5640 | `						}else{` |
|      ! 0 |  5641 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5642 | `								&pClass->sName,&sName` |
|        - |  5643 | `								);` |
|        - |  5644 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5645 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5646 | `						}` |
|        - |  5647 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5648 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5649 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5650 | `						}` |
|      ! 0 |  5651 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5652 | `					}else{` |
|        - |  5653 | `						/* Push method name on the stack */` |
|       68 |  5654 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5655 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5656 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5657 | `					}` |
|       68 |  5658 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5659 | `				}else{` |
|        - |  5660 | `					/* Attribute access */` |
|       72 |  5661 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5662 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5663 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5664 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5665 | `						/* ::class returns the fully qualified class name */` |
|        - |  5666 | `						/* Pop the attribute name from the stack */` |
|       54 |  5667 | `						if( !pInstr->p3 ){` |
|       54 |  5668 | `							VmPopOperand(&pTos,1);` |
|       26 |  5669 | `						}` |
|       54 |  5670 | `						PH7_MemObjRelease(pTos);` |
|        - |  5671 | `						/* Load the class name */` |
|       54 |  5672 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5673 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5674 | `					}else{` |
|        - |  5675 | `						/* Extract the target attribute */` |
|       20 |  5676 | `						if( sName.nByte > 0 ){` |
|       20 |  5677 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5678 | `						}` |
|       20 |  5679 | `						if( pAttr == 0 ){` |
|        - |  5680 | `							/* No such attribute,load null */` |
|      ! 0 |  5681 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5682 | `								&pClass->sName,&sName);` |
|        - |  5683 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5684 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5685 | `						}` |
|        - |  5686 | `						/* Pop the attribute name from the stack */` |
|       20 |  5687 | `						if( !pInstr->p3 ){` |
|        7 |  5688 | `							VmPopOperand(&pTos,1);` |
|        3 |  5689 | `						}` |
|       20 |  5690 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5691 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5692 | `						if( pAttr ){` |
|       20 |  5693 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5694 | `								/* Access to a non static attribute */` |
|      ! 0 |  5695 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5696 | `									&pClass->sName,&pAttr->sName` |
|        - |  5697 | `									);` |
|      ! 0 |  5698 | `							}else{` |
|        - |  5699 | `								ph7_value *pValue;` |
|        - |  5700 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5701 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5702 | `									/* Load the desired attribute */` |
|       20 |  5703 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5704 | `									if( pValue ){` |
|       20 |  5705 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5706 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5707 | `											/* Load index number */` |
|       14 |  5708 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5709 | `										}` |
|        9 |  5710 | `									}` |
|        9 |  5711 | `								}` |
|        - |  5712 | `							}` |
|        9 |  5713 | `						}` |
|        - |  5714 | `					}` |
|        - |  5715 | `				}` |
|      138 |  5716 | `				if( pThis ){` |
|        - |  5717 | `					/* Safely unreference the object */` |
|      ! 0 |  5718 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5719 | `				}` |
|        - |  5720 | `			}` |
|       70 |  5721 | `		}else{` |
|        - |  5722 | `			/* Pop operands */` |
|      ! 0 |  5723 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5724 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5725 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5726 | `			}` |
|      ! 0 |  5727 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5728 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5729 | `		}` |
|        - |  5730 | `	}` |
|     4362 |  5731 | `	break;` |
|        - |  5732 | `					}` |
|        - |  5733 | `/*` |
|        - |  5734 | ` * OP_NEW P1 * * *` |
|        - |  5735 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5736 | ` */` |
|      318 |  5737 | `case PH7_OP_NEW: {` |
|      638 |  5738 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      638 |  5739 | `	ph7_class *pClass = 0;` |
|        - |  5740 | `	ph7_class_instance *pNew;` |
|      638 |  5741 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5742 | `		/* Try to extract the desired class */` |
|      956 |  5743 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      636 |  5744 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      318 |  5745 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5746 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5747 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5748 | `	}` |
|      638 |  5749 | `	if( pClass == 0 ){` |
|        - |  5750 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5751 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5752 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5753 | `			);` |
|        - |  5754 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5755 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5756 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5757 | `			/* Pop given arguments */` |
|      ! 0 |  5758 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5759 | `		}` |
|      ! 0 |  5760 | `		goto Abort;` |
|      ! 0 |  5761 | `	}else{` |
|        - |  5762 | `		ph7_class_method *pCons;` |
|        - |  5763 | `		/* Create a new class instance */` |
|      638 |  5764 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      638 |  5765 | `		if( pNew == 0 ){` |
|      ! 0 |  5766 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5767 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5768 | `				&pClass->sName` |
|        - |  5769 | `			);` |
|      ! 0 |  5770 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5771 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5772 | `				/* Pop given arguments */` |
|      ! 0 |  5773 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5774 | `			}` |
|      ! 0 |  5775 | `			break;` |
|        - |  5776 | `		}` |
|        - |  5777 | `		/* Check if a constructor is available */` |
|      638 |  5778 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      638 |  5779 | `		if( pCons == 0 ){` |
|      528 |  5780 | `			SyString *pName = &pClass->sName;` |
|        - |  5781 | `			/* Check for a constructor with the same base class name */` |
|      528 |  5782 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      263 |  5783 | `		}` |
|      638 |  5784 | `		if( pCons ){` |
|        - |  5785 | `			/* Call the class constructor */` |
|      112 |  5786 | `			SySetReset(&aArg);` |
|      212 |  5787 | `			while( pArg < pTos ){` |
|      102 |  5788 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      102 |  5789 | `				pArg++;` |
|        2 |  5790 | `			}` |
|      112 |  5791 | `			if( pVm->bErrReport ){` |
|        - |  5792 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5793 | `				sxu32 n;` |
|       69 |  5794 | `				n = SySetUsed(&aArg);` |
|        - |  5795 | `				/* Emit a notice for missing arguments */` |
|      125 |  5796 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       57 |  5797 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       57 |  5798 | `					if( pFuncArg ){` |
|       57 |  5799 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5800 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5801 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5802 | `						}` |
|       28 |  5803 | `					}` |
|       57 |  5804 | `					n++;` |
|        1 |  5805 | `				}` |
|       34 |  5806 | `			}` |
|      112 |  5807 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5808 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      112 |  5809 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5810 | `				pNew->iRef = 1;` |
|      ! 0 |  5811 | `			}` |
|       55 |  5812 | `		}` |
|      638 |  5813 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5814 | `			/* Pop given arguments */` |
|       94 |  5815 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       46 |  5816 | `		}` |
|      638 |  5817 | `		PH7_MemObjRelease(pTos);` |
|      638 |  5818 | `		pTos->x.pOther = pNew;` |
|      638 |  5819 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5820 | `	}` |
|      638 |  5821 | `	break;` |
|        - |  5822 | `				 }` |
|        - |  5823 | `/*` |
|        - |  5824 | ` * OP_CLONE * * *` |
|        - |  5825 | ` * Perfome a clone operation.` |
|        - |  5826 | ` */` |
|       23 |  5827 | `case PH7_OP_CLONE: {` |
|        - |  5828 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5829 | `#ifdef UNTRUST` |
|        - |  5830 | `	if( pTos < pStack ){` |
|        - |  5831 | `		goto Abort;` |
|        - |  5832 | `	}` |
|        - |  5833 | `#endif` |
|        - |  5834 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5835 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5836 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5837 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5838 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5839 | `		break;` |
|        - |  5840 | `	}` |
|        - |  5841 | `	/* Point to the source */` |
|       44 |  5842 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5843 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  5844 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  5845 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5846 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  5847 | `			&pSrc->pClass->sName);` |
|      ! 0 |  5848 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5849 | `		break;` |
|        - |  5850 | `	}` |
|        - |  5851 | `	/* Perform the clone operation */` |
|       44 |  5852 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5853 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5854 | `	if( pClone == 0 ){` |
|      ! 0 |  5855 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5856 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5857 | `	}else{` |
|        - |  5858 | `		/* Load the cloned object */` |
|       44 |  5859 | `		pTos->x.pOther = pClone;` |
|       44 |  5860 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5861 | `	}` |
|       44 |  5862 | `	break;` |
|        - |  5863 | `				   }` |
|        - |  5864 | `/*` |
|        - |  5865 | ` * OP_SWITCH * * P3` |
|        - |  5866 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5867 | ` */` |
|       18 |  5868 | `case PH7_OP_SWITCH: {` |
|       38 |  5869 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5870 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5871 | `	ph7_value sValue,sCaseValue;` |
|        - |  5872 | `	sxu32 n,nEntry;` |
|        - |  5873 | `#ifdef UNTRUST` |
|        - |  5874 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5875 | `		goto Abort;` |
|        - |  5876 | `	}` |
|        - |  5877 | `#endif` |
|        - |  5878 | `	/* Point to the case table  */` |
|       38 |  5879 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5880 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5881 | `	/* Select the appropriate case block to execute */` |
|       38 |  5882 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5883 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5884 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5885 | `		pCase = &aCase[n];` |
|       92 |  5886 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5887 | `		/* Execute the case expression first */` |
|       92 |  5888 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5889 | `		/* Compare the two expression */` |
|       92 |  5890 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5891 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5892 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5893 | `		if( rc == 0 ){` |
|        - |  5894 | `			/* Value match,jump to this block */` |
|       38 |  5895 | `			pc = pCase->nStart - 1;` |
|       38 |  5896 | `			break;` |
|        - |  5897 | `		}` |
|       29 |  5898 | `	}` |
|       38 |  5899 | `	VmPopOperand(&pTos,1);` |
|       38 |  5900 | `	if( n >= nEntry ){` |
|        - |  5901 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5902 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5903 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5904 | `		}else{` |
|        - |  5905 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5906 | `			pc = pSwitch->nOut - 1;` |
|        - |  5907 | `		}` |
|      ! 0 |  5908 | `	}` |
|       38 |  5909 | `	break;` |
|        - |  5910 | `					}` |
|        - |  5911 | `/*` |
|        - |  5912 | ` * OP_YIELD P1 P2 *` |
|        - |  5913 | ` *  Yield a value from a generator function.` |
|        - |  5914 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  5915 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  5916 | ` */` |
|       28 |  5917 | `case PH7_OP_YIELD: {` |
|        - |  5918 | `	ph7_generator *pGen;` |
|       57 |  5919 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  5920 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  5921 | `		goto Abort;` |
|        - |  5922 | `	}` |
|       57 |  5923 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       57 |  5924 | `	if( pInstr->iP2 ){` |
|        - |  5925 | `		/* yield $key => $value: value on top, key below */` |
|        - |  5926 | `#ifdef UNTRUST` |
|        - |  5927 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  5928 | `#endif` |
|        7 |  5929 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  5930 | `		VmPopOperand(&pTos, 1);` |
|        7 |  5931 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  5932 | `		VmPopOperand(&pTos, 1);` |
|        - |  5933 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  5934 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5935 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  5936 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  5937 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  5938 | `			}` |
|        1 |  5939 | `		}` |
|       54 |  5940 | `	}else if( pInstr->iP1 ){` |
|        - |  5941 | `		/* yield $value */` |
|        - |  5942 | `#ifdef UNTRUST` |
|        - |  5943 | `		if( pTos < pStack ) goto Abort;` |
|        - |  5944 | `#endif` |
|       51 |  5945 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       51 |  5946 | `		VmPopOperand(&pTos, 1);` |
|        - |  5947 | `		/* Auto-increment key */` |
|       51 |  5948 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       51 |  5949 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       51 |  5950 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       26 |  5951 | `	}else{` |
|        - |  5952 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  5953 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  5954 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  5955 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  5956 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  5957 | `	}` |
|        - |  5958 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       57 |  5959 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       57 |  5960 | `	goto Suspend;` |
|        - |  5961 |  |
|        - |  5962 | `/*` |
|        - |  5963 | ` * OP_CALL P1 * *` |
|        - |  5964 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5965 | ` *  function on the stack.` |
|        - |  5966 | ` */` |
|   289727 |  5967 | `case PH7_OP_CALL: {` |
|   579500 |  5968 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5969 | `	SyHashEntry *pEntry;` |
|        - |  5970 | `	SyString sName;` |
|        - |  5971 | `	/* Extract function name */` |
|   579500 |  5972 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5973 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5974 | `			ph7_value sResult;` |
|      ! 0 |  5975 | `			SySetReset(&aArg);` |
|      ! 0 |  5976 | `			while( pArg < pTos ){` |
|      ! 0 |  5977 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5978 | `				pArg++;` |
|      ! 0 |  5979 | `			}` |
|      ! 0 |  5980 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5981 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5982 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5983 | `			SySetReset(&aArg);` |
|        - |  5984 | `			/* Pop given arguments */` |
|      ! 0 |  5985 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5986 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5987 | `			}` |
|        - |  5988 | `			/* Copy result */` |
|      ! 0 |  5989 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5990 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5991 | `		}else{` |
|        3 |  5992 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5993 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5994 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5995 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5996 | `			}else{` |
|        - |  5997 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5998 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5999 | `			}` |
|        - |  6000 | `			/* Pop given arguments */` |
|        3 |  6001 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6002 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6003 | `			}` |
|        - |  6004 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6005 | `			PH7_MemObjRelease(pTos);` |
|        - |  6006 | `		}` |
|   289454 |  6007 | `		break;` |
|        - |  6008 | `	}` |
|   579498 |  6009 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6010 | `	/* Check for a compiled function first.` |
|        - |  6011 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6012 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   579498 |  6013 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6014 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6015 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6016 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6017 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6018 | `	 * function calls inside namespaces. */` |
|   579498 |  6019 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6020 | `		const char *zFunc;` |
|        - |  6021 | `		const char *zEnd;` |
|        - |  6022 | `		const char *z;` |
|        - |  6023 | `		SyString sGlobal;` |
|       15 |  6024 | `		zFunc = sName.zString;` |
|       15 |  6025 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  6026 | `		z = zEnd;` |
|        - |  6027 | `		/* Find last namespace separator */` |
|      133 |  6028 | `		while( z > zFunc ){` |
|      133 |  6029 | `			if( z[-1] == '\\' ){` |
|       15 |  6030 | `				break;` |
|        - |  6031 | `			}` |
|      119 |  6032 | `			z--;` |
|        1 |  6033 | `		}` |
|       15 |  6034 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6035 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  6036 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  6037 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  6038 | `		}` |
|        7 |  6039 | `	}` |
|   579498 |  6040 | `	if( pEntry ){` |
|        - |  6041 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6042 | `		ph7_class_instance *pThis;` |
|        - |  6043 | `		ph7_value *pFrameStack;` |
|        - |  6044 | `		ph7_vm_func *pVmFunc;` |
|        - |  6045 | `		ph7_class *pSelf;` |
|        - |  6046 | `		VmFrame *pFrame;` |
|        - |  6047 | `		ph7_value *pObj;` |
|        - |  6048 | `		VmSlot sArg;` |
|        - |  6049 | `		sxu32 n;` |
|        - |  6050 | `		/* initialize fields */` |
|    13156 |  6051 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13156 |  6052 | `		pThis = 0;` |
|    13156 |  6053 | `		pSelf = 0;` |
|    13156 |  6054 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6055 | `			ph7_class_method *pMeth;` |
|        - |  6056 | `			/* Class method call */` |
|     1956 |  6057 | `			ph7_value *pTarget = &pTos[-1];` |
|     1956 |  6058 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6059 | `				/* Extract the 'this' pointer */` |
|     1956 |  6060 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6061 | `					/* Instance already loaded */` |
|     1884 |  6062 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1884 |  6063 | `					pThis->iRef++;` |
|     1884 |  6064 | `					pSelf = pThis->pClass;` |
|      941 |  6065 | `				}` |
|     1956 |  6066 | `				if( pSelf == 0 ){` |
|       74 |  6067 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6068 | `						/* "Late Static Binding" class name */` |
|      101 |  6069 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6070 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6071 | `					}` |
|       74 |  6072 | `					if( pSelf == 0 ){` |
|       13 |  6073 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6074 | `					}` |
|       36 |  6075 | `				}` |
|     1956 |  6076 | `				if( pThis == 0  ){` |
|       74 |  6077 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6078 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6079 | `					if( pFrameLocal->pParent ){` |
|        - |  6080 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6081 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6082 | `						if( pThis ){` |
|       13 |  6083 | `							pThis->iRef++;` |
|        6 |  6084 | `						}` |
|       28 |  6085 | `					}` |
|       36 |  6086 | `				}` |
|     1956 |  6087 | `				VmPopOperand(&pTos,1);` |
|     1956 |  6088 | `				PH7_MemObjRelease(pTos);` |
|        - |  6089 | `				/* Synchronize pointers */` |
|     1956 |  6090 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  6091 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6092 | `				 * user have already computed the random generated unique class method name` |
|        - |  6093 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6094 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6095 | `				 */` |
|     1956 |  6096 | `				while( pArg < pStack ){` |
|      ! 0 |  6097 | `					pArg++;` |
|      ! 0 |  6098 | `				}` |
|     1956 |  6099 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6100 | `					/* Check if the call is allowed */` |
|     1956 |  6101 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1956 |  6102 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6103 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6104 | `							/* Pop given arguments */` |
|      ! 0 |  6105 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6106 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6107 | `							}` |
|        - |  6108 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6109 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6110 | `							break;` |
|        - |  6111 | `						}` |
|        3 |  6112 | `					}` |
|      977 |  6113 | `				}` |
|      977 |  6114 | `			}` |
|      977 |  6115 | `		}` |
|        - |  6116 | `		/* Check The recursion limit */` |
|    13156 |  6117 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6118 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6119 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6120 | `				&pVmFunc->sName);` |
|        - |  6121 | `			/* Pop given arguments */` |
|        3 |  6122 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6123 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6124 | `			}` |
|        - |  6125 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6126 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6127 | `			break;` |
|        - |  6128 | `		}` |
|    13154 |  6129 | `		if( pVmFunc->pNextName ){` |
|        - |  6130 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6131 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6132 | `		}` |
|    13154 |  6133 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6134 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6135 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6136 | `			ph7_generator *pGenerator;` |
|        - |  6137 | `			ph7_class_instance *pGenObj;` |
|        - |  6138 | `			ph7_value *pCtxAttr;` |
|        - |  6139 | `			SyString sAttrName;` |
|        - |  6140 | `			ph7_value **apCallArgs;` |
|        - |  6141 | `			int nCallArgs, iArg;` |
|        - |  6142 | `			/* Collect arguments from the operand stack */` |
|       19 |  6143 | `			nCallArgs = (int)(pTos - pArg);` |
|       19 |  6144 | `			apCallArgs = 0;` |
|       19 |  6145 | `			if( nCallArgs > 0 ){` |
|        7 |  6146 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6147 | `					nCallArgs * sizeof(ph7_value *));` |
|        5 |  6148 | `				if( apCallArgs == 0 ){` |
|        - |  6149 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6150 | `					nCallArgs = 0;` |
|      ! 0 |  6151 | `				}else{` |
|       11 |  6152 | `					for( iArg = 0; iArg < nCallArgs; iArg++ ){` |
|        7 |  6153 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        4 |  6154 | `					}` |
|        - |  6155 | `				}` |
|        2 |  6156 | `			}` |
|        - |  6157 | `			/* Create execution context and generator wrapper */` |
|       19 |  6158 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       19 |  6159 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6160 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6161 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6162 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6163 | `				break;` |
|        - |  6164 | `			}` |
|       19 |  6165 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       19 |  6166 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6167 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6168 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6169 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6170 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6171 | `				break;` |
|        - |  6172 | `			}` |
|        - |  6173 | `			/* Set up the frame with arguments, closure env, $this */` |
|       19 |  6174 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       19 |  6175 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       19 |  6176 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nCallArgs, apCallArgs);` |
|       19 |  6177 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       19 |  6178 | `			pExecCtx->pFrame->pParent = 0;` |
|       19 |  6179 | `			if( apCallArgs ){` |
|        5 |  6180 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6181 | `			}` |
|       19 |  6182 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6183 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6184 | `				if( pThis ){` |
|      ! 0 |  6185 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6186 | `				}` |
|      ! 0 |  6187 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6188 | `					goto Abort;` |
|        - |  6189 | `				}` |
|      ! 0 |  6190 | `				break;` |
|        - |  6191 | `			}` |
|        - |  6192 | `			/* Create Generator class instance */` |
|       19 |  6193 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       19 |  6194 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6195 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6196 | `				break;` |
|        - |  6197 | `			}` |
|        - |  6198 | `			/* Store generator in __ctx attribute */` |
|       19 |  6199 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       19 |  6200 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       19 |  6201 | `			if( pCtxAttr ){` |
|       19 |  6202 | `				pCtxAttr->x.pOther = pGenerator;` |
|       19 |  6203 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6204 | `			}` |
|        - |  6205 | `			/* Pop args and function name, push Generator object */` |
|       19 |  6206 | `			PH7_MemObjRelease(pTos);` |
|       19 |  6207 | `			pTos = &pTos[-pInstr->iP1];` |
|       19 |  6208 | `			pTos->x.pOther = pGenObj;` |
|       19 |  6209 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       19 |  6210 | `			pGenObj->iRef++;` |
|       19 |  6211 | `			if( pThis ){` |
|      ! 0 |  6212 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6213 | `			}` |
|       19 |  6214 | `			break;` |
|        - |  6215 | `		}` |
|        - |  6216 | `		/* Extract the formal argument set */` |
|    13136 |  6217 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6218 | `		/* Create a new VM frame  */` |
|    13136 |  6219 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13136 |  6220 | `		if( rc != SXRET_OK ){` |
|        - |  6221 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6222 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6223 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6224 | `				&pVmFunc->sName);` |
|        - |  6225 | `			/* Pop given arguments */` |
|      ! 0 |  6226 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6227 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6228 | `			}` |
|        - |  6229 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6230 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6231 | `			break;` |
|        - |  6232 | `		}` |
|    13136 |  6233 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6234 | `			/* Install the '$this' variable */` |
|        - |  6235 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1894 |  6236 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1894 |  6237 | `			if( pObj ){` |
|        - |  6238 | `				/* Reflect the change */` |
|     1894 |  6239 | `				pObj->x.pOther = pThis;` |
|     1894 |  6240 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      946 |  6241 | `			}` |
|      946 |  6242 | `		}` |
|    13136 |  6243 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6244 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6245 | `			/* Install static variables */` |
|      ! 0 |  6246 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6247 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6248 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6249 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6250 | `					/* Initialize the static variables */` |
|      ! 0 |  6251 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6252 | `					if( pObj ){` |
|        - |  6253 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6254 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6255 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6256 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6257 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6258 | `						}` |
|      ! 0 |  6259 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6260 | `					}else{` |
|      ! 0 |  6261 | `						continue;` |
|        - |  6262 | `					}` |
|      ! 0 |  6263 | `				}` |
|        - |  6264 | `				/* Install in the current frame */` |
|      ! 0 |  6265 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6266 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6267 | `			}` |
|      ! 0 |  6268 | `		}` |
|        - |  6269 | `		/* Push arguments in the local frame */` |
|    13136 |  6270 | `		n = 0;` |
|    35760 |  6271 | `		while( pArg < pTos ){` |
|    22626 |  6272 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22472 |  6273 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  6274 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  6275 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6276 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6277 | `						goto Abort;` |
|        - |  6278 | `					}` |
|      ! 0 |  6279 | `				}` |
|        - |  6280 | `				/* Make sure the given arguments are of the correct type */` |
|    22472 |  6281 | `				if( aFormalArg[n].nType > 0 ){` |
|     1098 |  6282 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6283 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  6284 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6285 | `						ph7_class *pClass;` |
|        - |  6286 | `						/* Try to extract the desired class */` |
|      ! 0 |  6287 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  6288 | `						if( pClass ){` |
|      ! 0 |  6289 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6290 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6291 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6292 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6293 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6294 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6295 | `								}` |
|      ! 0 |  6296 | `							}else{` |
|        - |  6297 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  6298 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6299 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  6300 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6301 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6302 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6303 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6304 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6305 | `								}` |
|        - |  6306 | `							}` |
|      ! 0 |  6307 | `						}` |
|     1098 |  6308 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6309 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6310 | `						/* Cast to the desired type */` |
|      ! 0 |  6311 | `						xCast(pArg);` |
|      ! 0 |  6312 | `					}` |
|      548 |  6313 | `				}` |
|    22472 |  6314 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6315 | `					/* Pass by reference */` |
|       50 |  6316 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6317 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6318 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6319 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6320 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6321 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6322 | `						}` |
|        - |  6323 | `						/* Switch to pass by value */` |
|      ! 0 |  6324 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6325 | `					}else{` |
|        - |  6326 | `						SyHashEntry *pRefEntry;` |
|        - |  6327 | `						/* Install the referenced variable in the private function frame */` |
|       50 |  6328 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       50 |  6329 | `						if( pRefEntry == 0 ){` |
|       74 |  6330 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       48 |  6331 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       50 |  6332 | `							sArg.nIdx = pArg->nIdx;` |
|       50 |  6333 | `							sArg.pUserData = 0;` |
|       50 |  6334 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  6335 | `						}` |
|       50 |  6336 | `						pObj = 0;` |
|        - |  6337 | `					}` |
|       26 |  6338 | `				}else{` |
|        - |  6339 | `					/* Pass by value,make a copy of the given argument */` |
|    22424 |  6340 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6341 | `				}` |
|    11237 |  6342 | `			}else{` |
|        - |  6343 | `				char zName[32];` |
|        - |  6344 | `				SyString sArgName;` |
|        - |  6345 | `				/* Set a dummy name */` |
|      156 |  6346 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6347 | `				sArgName.zString = zName;` |
|        - |  6348 | `				/* Annonymous argument */` |
|      156 |  6349 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6350 | `			}` |
|    22626 |  6351 | `			if( pObj ){` |
|    22578 |  6352 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6353 | `				/* Insert argument index  */` |
|    22578 |  6354 | `				sArg.nIdx = pObj->nIdx;` |
|    22578 |  6355 | `				sArg.pUserData = 0;` |
|    22578 |  6356 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11288 |  6357 | `			}` |
|    22626 |  6358 | `			PH7_MemObjRelease(pArg);` |
|    22626 |  6359 | `			pArg++;` |
|    22626 |  6360 | `			++n;` |
|        2 |  6361 | `		}` |
|        - |  6362 | `		/* Set up closure environment */` |
|    13136 |  6363 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6364 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6365 | `			ph7_value *pValue;` |
|        - |  6366 | `			sxu32 iEnv;` |
|       11 |  6367 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6368 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6369 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6370 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6371 | `					/* Do not install null value */` |
|       11 |  6372 | `					continue;` |
|        - |  6373 | `				}` |
|       11 |  6374 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6375 | `				if( pValue == 0 ){` |
|      ! 0 |  6376 | `					continue;` |
|        - |  6377 | `				}` |
|        - |  6378 | `				/* Invalidate any prior representation */` |
|       11 |  6379 | `				PH7_MemObjRelease(pValue);` |
|        - |  6380 | `				/* Duplicate bound variable value */` |
|       11 |  6381 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6382 | `			}` |
|        5 |  6383 | `		}` |
|        - |  6384 | `		/* Process default values */` |
|    15058 |  6385 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1924 |  6386 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1918 |  6387 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1918 |  6388 | `				if( pObj ){` |
|        - |  6389 | `					/* Evaluate the default value and extract it's result */` |
|     1918 |  6390 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1918 |  6391 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6392 | `						goto Abort;` |
|        - |  6393 | `					}` |
|        - |  6394 | `					/* Insert argument index */` |
|     1918 |  6395 | `					sArg.nIdx = pObj->nIdx;` |
|     1918 |  6396 | `					sArg.pUserData = 0;` |
|     1918 |  6397 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6398 | `					/* Make sure the default argument is of the correct type */` |
|     1918 |  6399 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6400 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6401 | `						/* Cast to the desired type */` |
|      ! 0 |  6402 | `						xCast(pObj);` |
|      ! 0 |  6403 | `					}` |
|      958 |  6404 | `				}` |
|      958 |  6405 | `			}` |
|     1924 |  6406 | `			++n;` |
|        2 |  6407 | `		}` |
|        - |  6408 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6409 | `		 * does not return anything.` |
|        - |  6410 | `		 */` |
|    13136 |  6411 | `		PH7_MemObjRelease(pTos);` |
|    13136 |  6412 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6413 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13136 |  6414 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13136 |  6415 | `		if( pFrameStack == 0 ){` |
|        - |  6416 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6417 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6418 | `				&pVmFunc->sName);` |
|      ! 0 |  6419 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6420 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6421 | `			}` |
|      ! 0 |  6422 | `			break;` |
|        - |  6423 | `		}` |
|    13136 |  6424 | `		if( pSelf ){` |
|        - |  6425 | `			/* Push class name */` |
|     1954 |  6426 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      976 |  6427 | `		}` |
|        - |  6428 | `		/* Increment nesting level */` |
|    13136 |  6429 | `		pVm->nRecursionDepth++;` |
|        - |  6430 | `		/* Execute function body */` |
|    13136 |  6431 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6432 | `		/* Decrement nesting level */` |
|    13136 |  6433 | `		pVm->nRecursionDepth--;` |
|    13136 |  6434 | `		if( pSelf ){` |
|        - |  6435 | `			/* Pop class name */` |
|     1954 |  6436 | `			(void)SySetPop(&pVm->aSelf);` |
|      976 |  6437 | `		}` |
|        - |  6438 | `		/* Cleanup the mess left behind */` |
|    13136 |  6439 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6440 | `			/* Return by reference,reflect that */` |
|        9 |  6441 | `			if( n != SXU32_HIGH ){` |
|        9 |  6442 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6443 | `				sxu32 i;` |
|        - |  6444 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6445 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6446 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6447 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6448 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6449 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6450 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6451 | `								&pVmFunc->sName);` |
|      ! 0 |  6452 | `						}` |
|      ! 0 |  6453 | `						n = SXU32_HIGH;` |
|      ! 0 |  6454 | `						break;` |
|        - |  6455 | `					}` |
|        3 |  6456 | `				}` |
|        5 |  6457 | `			}else{` |
|      ! 0 |  6458 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6459 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6460 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6461 | `						&pVmFunc->sName);` |
|      ! 0 |  6462 | `				}` |
|        - |  6463 | `			}` |
|        9 |  6464 | `			pTos->nIdx = n;` |
|        4 |  6465 | `		}` |
|        - |  6466 | `		/* Cleanup the mess left behind */` |
|    13136 |  6467 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6468 | `			/* An exception was throw in this frame */` |
|       12 |  6469 | `			pFrame = pFrame->pParent;` |
|       12 |  6470 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6471 | `				/* Pop the resutlt */` |
|       10 |  6472 | `				VmPopOperand(&pTos,1);` |
|        - |  6473 | `				/* Jump to this destination */` |
|       10 |  6474 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6475 | `				rc = PH7_OK;` |
|        6 |  6476 | `			}else{` |
|        3 |  6477 | `				if( pFrame->pParent ){` |
|        3 |  6478 | `					rc = PH7_EXCEPTION;` |
|        2 |  6479 | `				}else{` |
|        - |  6480 | `					/* Continue normal execution */` |
|      ! 0 |  6481 | `					rc = PH7_OK;` |
|        - |  6482 | `				}` |
|        - |  6483 | `			}` |
|        5 |  6484 | `		}` |
|        - |  6485 | `		/* Free the operand stack */` |
|    13136 |  6486 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6487 | `		/* Leave the frame */` |
|    13136 |  6488 | `		VmLeaveFrame(&(*pVm));` |
|    13136 |  6489 | `		if( rc == PH7_ABORT ){` |
|        - |  6490 | `			/* Abort processing immeditaley */` |
|        7 |  6491 | `			goto Abort;` |
|    13130 |  6492 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6493 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6494 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6495 | `			 * overwriting the state saved by the inner level.` |
|        - |  6496 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6497 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       39 |  6498 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       39 |  6499 | `			goto Suspend;` |
|    13092 |  6500 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6501 | `			goto Exception;` |
|        - |  6502 | `		}` |
|     6546 |  6503 | `	}else{` |
|        - |  6504 | `		ph7_user_func *pFunc;` |
|        - |  6505 | `		ph7_context sCtx;` |
|        - |  6506 | `		ph7_value sRet;` |
|        - |  6507 | `		/* Look for an installed foreign function.` |
|        - |  6508 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6509 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6510 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6511 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6512 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6513 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   566344 |  6514 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   566344 |  6515 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6516 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6517 | `			const char *zShort = sName.zString;` |
|        - |  6518 | `			sxu32 i;` |
|      217 |  6519 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6520 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6521 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6522 | `				}` |
|      102 |  6523 | `			}` |
|       15 |  6524 | `			if( zShort != sName.zString ){` |
|       15 |  6525 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6526 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6527 | `			}` |
|        7 |  6528 | `		}` |
|   566344 |  6529 | `		if( pEntry == 0 ){` |
|        - |  6530 | `			/* Call to undefined function */` |
|        5 |  6531 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6532 | `			/* Pop given arguments */` |
|        5 |  6533 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6534 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6535 | `			}` |
|        - |  6536 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6537 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6538 | `			break;` |
|        - |  6539 | `		}` |
|   566340 |  6540 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6541 | `		/* Start collecting function arguments */` |
|   566340 |  6542 | `		SySetReset(&aArg);` |
|  1518800 |  6543 | `		while( pArg < pTos ){` |
|   952462 |  6544 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   952462 |  6545 | `			pArg++;` |
|        2 |  6546 | `		}` |
|        - |  6547 | `		/* Assume a null return value */` |
|   566340 |  6548 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6549 | `		/* Init the call context */` |
|   566340 |  6550 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6551 | `		/* Call the foreign function */` |
|   566340 |  6552 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6553 | `		/* Release the call context */` |
|   566340 |  6554 | `		VmReleaseCallContext(&sCtx);` |
|   566340 |  6555 | `		if( rc == PH7_ABORT ){` |
|      463 |  6556 | `			goto Abort;` |
|   565878 |  6557 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6558 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6559 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6560 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6561 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6562 | `				goto Exception;` |
|        - |  6563 | `			}` |
|        - |  6564 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6565 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6566 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6567 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6568 | `			}` |
|        - |  6569 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6570 | `			VmPopOperand(&pTos,1);` |
|        - |  6571 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6572 | `			pFrm = pVm->pFrame;` |
|        7 |  6573 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6574 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6575 | `			}` |
|        7 |  6576 | `			break;` |
|        - |  6577 | `		}` |
|   565868 |  6578 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6579 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6580 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6581 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6582 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6583 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6584 | `			 * body), the user-function path above will handle re-saving. */` |
|       39 |  6585 | `			PH7_MemObjRelease(&sRet);` |
|       39 |  6586 | `			if( pInstr->iP1 > 0 ){` |
|       39 |  6587 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6588 | `			}` |
|        - |  6589 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6590 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       39 |  6591 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       39 |  6592 | `			goto Suspend;` |
|        - |  6593 | `		}` |
|   565830 |  6594 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6595 | `			/* Pop function name and arguments */` |
|   548206 |  6596 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   274124 |  6597 | `		}` |
|        - |  6598 | `		/* Save foreign function return value */` |
|   565830 |  6599 | `		PH7_MemObjStore(&sRet,pTos);` |
|   565830 |  6600 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6601 | `	}` |
|   578918 |  6602 | `	break;` |
|        - |  6603 | `				  }` |
|        - |  6604 | `/*` |
|        - |  6605 | ` * OP_CONSUME: P1 * *` |
|        - |  6606 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6607 | ` */` |
|    11297 |  6608 | `case PH7_OP_CONSUME: {` |
|    22596 |  6609 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22596 |  6610 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6611 |  |
|    22596 |  6612 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22596 |  6613 | `	pCur = pOut;` |
|        - |  6614 | `	/* Start the consume process  */` |
|    45190 |  6615 | `	while( pOut <= pTos ){` |
|        - |  6616 | `		/* Force a string cast */` |
|    22596 |  6617 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      304 |  6618 | `			PH7_MemObjToString(pOut);` |
|      151 |  6619 | `		}` |
|    22596 |  6620 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6621 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6622 | `			/* Invoke the output consumer callback */` |
|    12478 |  6623 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12478 |  6624 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    12478 |  6625 | `			SyBlobRelease(&pOut->sBlob);` |
|    12478 |  6626 | `			if( rc == SXERR_ABORT ){` |
|        - |  6627 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6628 | `				goto Abort;` |
|        - |  6629 | `			}` |
|     6238 |  6630 | `		}` |
|    22596 |  6631 | `		pOut++;` |
|        2 |  6632 | `	}` |
|    22596 |  6633 | `	pTos = &pCur[-1];` |
|    22594 |  6634 | `	break;` |
|        - |  6635 | `					 }` |
|        - |  6636 |  |
|        - |  6637 | `		} /* Switch() */` |
|  9853690 |  6638 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6639 | `	} /* For(;;) */` |
|    15995 |  6640 | `Done:` |
|    31992 |  6641 | `	SySetRelease(&aArg);` |
|    31992 |  6642 | `	return SXRET_OK;` |
|       66 |  6643 | `Suspend:` |
|      133 |  6644 | `	SySetRelease(&aArg);` |
|      133 |  6645 | `	return PH7_SUSPEND;` |
|      238 |  6646 | `Abort:` |
|      477 |  6647 | `	SySetRelease(&aArg);` |
|     1661 |  6648 | `	while( pTos >= pStack ){` |
|     1185 |  6649 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6650 | `		pTos--;` |
|        1 |  6651 | `	}` |
|      477 |  6652 | `	return PH7_ABORT;` |
|        3 |  6653 | `Exception:` |
|        8 |  6654 | `	SySetRelease(&aArg);` |
|       22 |  6655 | `	while( pTos >= pStack ){` |
|       16 |  6656 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6657 | `		pTos--;` |
|        2 |  6658 | `	}` |
|        8 |  6659 | `	return PH7_EXCEPTION;` |
|    16304 |  6660 |  |
|        - |  6661 | `/*` |
|        - |  6662 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6663 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6664 | ` * See block-comment on that function for additional information.` |
|        - |  6665 | ` */` |
|    14944 |  6666 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6667 |  |
|        - |  6668 | `	ph7_value *pStack;` |
|        - |  6669 | `	sxi32 rc;` |
|        - |  6670 | `	/* Allocate a new operand stack */` |
|    14946 |  6671 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14946 |  6672 | `	if( pStack == 0 ){` |
|      ! 0 |  6673 | `		return SXERR_MEM;` |
|        - |  6674 | `	}` |
|        - |  6675 | `	/* Execute the program */` |
|    14946 |  6676 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6677 | `	/* Free the operand stack */` |
|    14946 |  6678 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6679 | `	/* Execution result */` |
|    14946 |  6680 | `	return rc;` |
|     7474 |  6681 |  |
|        - |  6682 | `/*` |
|        - |  6683 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6684 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6685 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6686 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6687 | ` * execution ends.` |
|        - |  6688 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6689 | ` * additional information.` |
|        - |  6690 | ` */` |
|     2468 |  6691 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6692 |  |
|        - |  6693 | `	VmShutdownCB *pEntry;` |
|        - |  6694 | `	ph7_value *apArg[10];` |
|        - |  6695 | `	sxu32 n,nEntry;` |
|        - |  6696 | `	int i;` |
|        - |  6697 | `	/* Point to the stack of registered callbacks */` |
|     2470 |  6698 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    27150 |  6699 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    24682 |  6700 | `		apArg[i] = 0;` |
|    12342 |  6701 | `	}` |
|     2472 |  6702 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6703 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6704 | `		if( pEntry ){` |
|        - |  6705 | `			/* Prepare callback arguments if any */` |
|        3 |  6706 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6707 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6708 | `					break;` |
|        - |  6709 | `				}` |
|      ! 0 |  6710 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6711 | `			}` |
|        - |  6712 | `			/* Invoke the callback */` |
|        3 |  6713 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6714 | `			/*` |
|        - |  6715 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6716 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6717 | `			 */` |
|        3 |  6718 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6719 | `			if( pEntry ){` |
|        3 |  6720 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6721 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6722 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6723 | `				}` |
|        1 |  6724 | `			}` |
|        1 |  6725 | `		}` |
|        2 |  6726 | `	}` |
|     2470 |  6727 | `	SySetReset(&pVm->aShutdown);` |
|     2470 |  6728 |  |
|        - |  6729 | `/*` |
|        - |  6730 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6731 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6732 | ` * See block-comment on that function for additional information.` |
|        - |  6733 | ` */` |
|     2476 |  6734 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6735 |  |
|        - |  6736 | `	/* Make sure we are ready to execute this program */` |
|     2478 |  6737 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6738 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6739 | `	}` |
|        - |  6740 | `	/* Set the execution magic number  */` |
|     2478 |  6741 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6742 | `	/* Execute the program */` |
|     2478 |  6743 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6744 | `	/* Invoke any shutdown callbacks */` |
|     2474 |  6745 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6746 | `	/*` |
|        - |  6747 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6748 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6749 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6750 | `	 */` |
|     2474 |  6751 | `	return SXRET_OK;` |
|     1240 |  6752 |  |
|        - |  6753 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6754 | `/*` |
|        - |  6755 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6756 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6757 | ` */` |
|       42 |  6758 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        1 |  6759 |  |
|        - |  6760 | `	ph7_exec_ctx *pCtx;` |
|        - |  6761 | `	ph7_value *pStack;` |
|        - |  6762 | `	VmFrame *pFrame;` |
|       43 |  6763 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       43 |  6764 | `	if( pCtx == 0 ){` |
|      ! 0 |  6765 | `		return 0;` |
|        - |  6766 | `	}` |
|       43 |  6767 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       43 |  6768 | `	pCtx->pVm = pVm;` |
|       43 |  6769 | `	pCtx->pFunc = pFunc;` |
|       43 |  6770 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       43 |  6771 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       43 |  6772 | `	pCtx->pc = 0;` |
|       43 |  6773 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       43 |  6774 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6775 | `	/* Allocate a private operand stack */` |
|       43 |  6776 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       43 |  6777 | `	if( pStack == 0 ){` |
|      ! 0 |  6778 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6779 | `		return 0;` |
|        - |  6780 | `	}` |
|       43 |  6781 | `	pCtx->pStack = pStack;` |
|        - |  6782 | `	/* Create a detached frame for the fiber */` |
|       43 |  6783 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       43 |  6784 | `	if( pFrame == 0 ){` |
|      ! 0 |  6785 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6786 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6787 | `		return 0;` |
|        - |  6788 | `	}` |
|       43 |  6789 | `	pCtx->pFrame = pFrame;` |
|       43 |  6790 | `	return pCtx;` |
|       22 |  6791 |  |
|        - |  6792 | `/*` |
|        - |  6793 | ` * Start executing a fiber context for the first time.` |
|        - |  6794 | ` */` |
|       42 |  6795 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        1 |  6796 |  |
|        - |  6797 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6798 | `	sxi32 rc;` |
|       43 |  6799 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6800 | `		return SXERR_INVALID;` |
|        - |  6801 | `	}` |
|        - |  6802 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       43 |  6803 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       43 |  6804 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6805 | `	/* Save and set the active context */` |
|       43 |  6806 | `	pOldCtx = pVm->pActiveCtx;` |
|       43 |  6807 | `	pVm->pActiveCtx = pCtx;` |
|       43 |  6808 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       43 |  6809 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       43 |  6810 | `	pVm->nRecursionDepth++;` |
|        - |  6811 | `	/* Execute from the beginning */` |
|       64 |  6812 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  6813 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       43 |  6814 | `	pVm->nRecursionDepth--;` |
|        - |  6815 | `	/* Restore the previous context */` |
|       43 |  6816 | `	pVm->pActiveCtx = pOldCtx;` |
|       43 |  6817 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6818 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       41 |  6819 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       41 |  6820 | `		pCtx->pFrame->pParent = 0;` |
|       41 |  6821 | `		if( pResult ){` |
|       23 |  6822 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  6823 | `		}` |
|       41 |  6824 | `		return SXRET_OK;` |
|        - |  6825 | `	}` |
|        - |  6826 | `	/* Detach frame */` |
|        3 |  6827 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  6828 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  6829 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  6830 | `	}` |
|        3 |  6831 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6832 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6833 | `		return PH7_ABORT;` |
|        - |  6834 | `	}` |
|        3 |  6835 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6836 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6837 | `		return PH7_EXCEPTION;` |
|        - |  6838 | `	}` |
|        - |  6839 | `	/* Normal completion */` |
|        3 |  6840 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  6841 | `	if( pResult ){` |
|        3 |  6842 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  6843 | `	}` |
|        3 |  6844 | `	return SXRET_OK;` |
|       22 |  6845 |  |
|        - |  6846 | `/*` |
|        - |  6847 | ` * Resume a suspended fiber context.` |
|        - |  6848 | ` */` |
|       86 |  6849 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        1 |  6850 |  |
|        - |  6851 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6852 | `	sxi32 rc;` |
|       87 |  6853 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  6854 | `		return SXERR_INVALID;` |
|        - |  6855 | `	}` |
|        - |  6856 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  6857 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  6858 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       87 |  6859 | `	if( pResumeValue ){` |
|       39 |  6860 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       20 |  6861 | `	}else{` |
|       49 |  6862 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  6863 | `	}` |
|       87 |  6864 | `	pCtx->nTos++;` |
|        - |  6865 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       87 |  6866 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       87 |  6867 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6868 | `	/* Save and set the active context */` |
|       87 |  6869 | `	pOldCtx = pVm->pActiveCtx;` |
|       87 |  6870 | `	pVm->pActiveCtx = pCtx;` |
|       87 |  6871 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       87 |  6872 | `	pVm->nRecursionDepth++;` |
|        - |  6873 | `	/* Resume execution from saved PC */` |
|      130 |  6874 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  6875 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       87 |  6876 | `	pVm->nRecursionDepth--;` |
|        - |  6877 | `	/* Restore the previous context */` |
|       87 |  6878 | `	pVm->pActiveCtx = pOldCtx;` |
|       87 |  6879 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6880 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       55 |  6881 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       55 |  6882 | `		pCtx->pFrame->pParent = 0;` |
|       55 |  6883 | `		if( pResult ){` |
|       17 |  6884 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  6885 | `		}` |
|       55 |  6886 | `		return SXRET_OK;` |
|        - |  6887 | `	}` |
|        - |  6888 | `	/* Detach frame */` |
|       33 |  6889 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       33 |  6890 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       33 |  6891 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  6892 | `	}` |
|       33 |  6893 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6894 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6895 | `		return PH7_ABORT;` |
|        - |  6896 | `	}` |
|       33 |  6897 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6898 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6899 | `		return PH7_EXCEPTION;` |
|        - |  6900 | `	}` |
|        - |  6901 | `	/* Normal completion */` |
|       33 |  6902 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       33 |  6903 | `	if( pResult ){` |
|       19 |  6904 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  6905 | `	}` |
|       33 |  6906 | `	return SXRET_OK;` |
|       44 |  6907 |  |
|        - |  6908 | `/*` |
|        - |  6909 | ` * Release an execution context and all its resources.` |
|        - |  6910 | ` */` |
|      ! 0 |  6911 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|      ! 0 |  6912 |  |
|      ! 0 |  6913 | `	if( pCtx == 0 ){` |
|      ! 0 |  6914 | `		return;` |
|        - |  6915 | `	}` |
|      ! 0 |  6916 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  6917 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  6918 | `		return;` |
|        - |  6919 | `	}` |
|      ! 0 |  6920 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  6921 | `	/* Release values */` |
|      ! 0 |  6922 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|      ! 0 |  6923 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  6924 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|      ! 0 |  6925 | `	if( pCtx->pFrame ){` |
|        - |  6926 | `		VmSlot *aSlot;` |
|        - |  6927 | `		sxu32 n;` |
|        - |  6928 | `		/* Free local variables */` |
|      ! 0 |  6929 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6930 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|      ! 0 |  6931 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|      ! 0 |  6932 | `		}` |
|        - |  6933 | `		/* Remove local references */` |
|      ! 0 |  6934 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|      ! 0 |  6935 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|      ! 0 |  6936 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|      ! 0 |  6937 | `		}` |
|      ! 0 |  6938 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|      ! 0 |  6939 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|      ! 0 |  6940 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6941 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|      ! 0 |  6942 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|      ! 0 |  6943 | `		pCtx->pFrame = 0;` |
|      ! 0 |  6944 | `	}` |
|        - |  6945 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  6946 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  6947 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|      ! 0 |  6948 | `	if( pCtx->pStack ){` |
|      ! 0 |  6949 | `		if( pCtx->nTos >= 0 ){` |
|      ! 0 |  6950 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|      ! 0 |  6951 | `			while( pTos >= pCtx->pStack ){` |
|      ! 0 |  6952 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6953 | `				pTos--;` |
|      ! 0 |  6954 | `			}` |
|      ! 0 |  6955 | `		}` |
|      ! 0 |  6956 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|      ! 0 |  6957 | `		pCtx->pStack = 0;` |
|      ! 0 |  6958 | `	}` |
|        - |  6959 | `	/* Free the context itself */` |
|      ! 0 |  6960 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6961 |  |
|        - |  6962 | `/*` |
|        - |  6963 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  6964 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  6965 | ` */` |
|       86 |  6966 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        1 |  6967 |  |
|        - |  6968 | `	ph7_class_instance *pThis;` |
|        - |  6969 | `	SyString sAttr;` |
|        - |  6970 | `	ph7_value *pAttr;` |
|       87 |  6971 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6972 | `		return 0;` |
|        - |  6973 | `	}` |
|       87 |  6974 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       87 |  6975 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  6976 | `		return 0;` |
|        - |  6977 | `	}` |
|       87 |  6978 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       87 |  6979 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       87 |  6980 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       31 |  6981 | `		return 0;` |
|        - |  6982 | `	}` |
|       57 |  6983 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       44 |  6984 |  |
|        - |  6985 | `/*` |
|        - |  6986 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  6987 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  6988 | ` */` |
|       38 |  6989 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  6990 |  |
|       39 |  6991 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 |  6992 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  6993 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6994 | `			"Cannot suspend outside of a fiber");` |
|        - |  6995 | `	}` |
|       39 |  6996 | `	if( nArg > 0 ){` |
|       39 |  6997 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       20 |  6998 | `	}else{` |
|      ! 0 |  6999 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7000 | `	}` |
|       39 |  7001 | `	return PH7_SUSPEND;` |
|       20 |  7002 |  |
|        - |  7003 | `/*` |
|        - |  7004 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7005 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7006 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7007 | ` */` |
|       24 |  7008 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7009 |  |
|        - |  7010 | `	ph7_class_instance *pThis;` |
|        - |  7011 | `	ph7_value *pAttr;` |
|        - |  7012 | `	SyString sAttrName;` |
|       25 |  7013 | `	if( nArg < 2 ){` |
|      ! 0 |  7014 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7015 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7016 | `	}` |
|       25 |  7017 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7018 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7019 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7020 | `	}` |
|       25 |  7021 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       25 |  7022 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7023 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7024 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7025 | `	}` |
|        - |  7026 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       25 |  7027 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7028 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7029 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7030 | `	}` |
|        - |  7031 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       25 |  7032 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7033 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7034 | `	if( pAttr ){` |
|       25 |  7035 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7036 | `	}` |
|       25 |  7037 | `	return PH7_OK;` |
|       13 |  7038 |  |
|        - |  7039 | `/*` |
|        - |  7040 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7041 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7042 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7043 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7044 | ` */` |
|       24 |  7045 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7046 | `	ph7_class_instance **ppThis)` |
|        1 |  7047 |  |
|       25 |  7048 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7049 | `	ph7_value *pCallable;` |
|        - |  7050 | `	SyString sAttrName;` |
|       25 |  7051 | `	*ppThis = 0;` |
|       25 |  7052 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7053 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       25 |  7054 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7055 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7056 | `		return 0;` |
|        - |  7057 | `	}` |
|       25 |  7058 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7059 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7060 | `		SyString sName;` |
|        - |  7061 | `		SyHashEntry *pEntry;` |
|        - |  7062 | `		ph7_vm_func *pFunc;` |
|       25 |  7063 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       25 |  7064 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       25 |  7065 | `		if( pEntry == 0 ){` |
|      ! 0 |  7066 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7067 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7068 | `			return 0;` |
|        - |  7069 | `		}` |
|       25 |  7070 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       25 |  7071 | `		return pFunc;` |
|      ! 0 |  7072 | `	}else{` |
|        - |  7073 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7074 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7075 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7076 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7077 | `		if( pMethod == 0 ){` |
|      ! 0 |  7078 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7079 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7080 | `			return 0;` |
|        - |  7081 | `		}` |
|      ! 0 |  7082 | `		*ppThis = pClosure;` |
|      ! 0 |  7083 | `		return &pMethod->sFunc;` |
|        - |  7084 | `	}` |
|       13 |  7085 |  |
|        - |  7086 | `/*` |
|        - |  7087 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7088 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7089 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7090 | ` */` |
|       42 |  7091 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7092 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        1 |  7093 |  |
|       43 |  7094 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7095 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7096 | `	sxu32 nFormal, n;` |
|        - |  7097 | `	VmSlot sSlot;` |
|        - |  7098 | `	sxi32 rc;` |
|        - |  7099 | `	/* Install $this for closure/method callables */` |
|       43 |  7100 | `	if( pClosureThis ){` |
|        - |  7101 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7102 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7103 | `		if( pObj ){` |
|      ! 0 |  7104 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7105 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7106 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7107 | `		}` |
|      ! 0 |  7108 | `	}` |
|        - |  7109 | `	/* Install static variables */` |
|       43 |  7110 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7111 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7112 | `		ph7_value *pVal;` |
|      ! 0 |  7113 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7114 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7115 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7116 | `			if( pVal ){` |
|      ! 0 |  7117 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7118 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7119 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7120 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7121 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7122 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7123 | `				}` |
|      ! 0 |  7124 | `			}` |
|      ! 0 |  7125 | `		}` |
|      ! 0 |  7126 | `	}` |
|        - |  7127 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       43 |  7128 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       43 |  7129 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       53 |  7130 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7131 | `		ph7_value *pObj;` |
|       11 |  7132 | `		if( n < (sxu32)nArg ){` |
|        - |  7133 | `			/* Argument provided — install with type casting */` |
|       11 |  7134 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       11 |  7135 | `			if( pObj ){` |
|       11 |  7136 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7137 | `				/* Type casting */` |
|       11 |  7138 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7139 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7140 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7141 | `						if( xCast ){` |
|      ! 0 |  7142 | `							xCast(pObj);` |
|      ! 0 |  7143 | `						}` |
|      ! 0 |  7144 | `					}` |
|      ! 0 |  7145 | `				}` |
|       11 |  7146 | `				sSlot.nIdx = pObj->nIdx;` |
|       11 |  7147 | `				sSlot.pUserData = 0;` |
|       11 |  7148 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        6 |  7149 | `			}` |
|        5 |  7150 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7151 | `			/* Default value */` |
|      ! 0 |  7152 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7153 | `			if( pObj ){` |
|      ! 0 |  7154 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7155 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7156 | `					return rc;` |
|        - |  7157 | `				}` |
|      ! 0 |  7158 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7159 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7160 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7161 | `						if( xCast ){` |
|      ! 0 |  7162 | `							xCast(pObj);` |
|      ! 0 |  7163 | `						}` |
|      ! 0 |  7164 | `					}` |
|      ! 0 |  7165 | `				}` |
|      ! 0 |  7166 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7167 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7168 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7169 | `			}` |
|      ! 0 |  7170 | `		}` |
|        6 |  7171 | `	}` |
|        - |  7172 | `	/* Install closure environment (captured variables) */` |
|       43 |  7173 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7174 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7175 | `		ph7_value *pValue;` |
|        - |  7176 | `		sxu32 iEnv;` |
|        3 |  7177 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7178 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7179 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7180 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7181 | `				continue;` |
|        - |  7182 | `			}` |
|        5 |  7183 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7184 | `			if( pValue == 0 ){` |
|      ! 0 |  7185 | `				continue;` |
|        - |  7186 | `			}` |
|        5 |  7187 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7188 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7189 | `		}` |
|        1 |  7190 | `	}` |
|       43 |  7191 | `	return SXRET_OK;` |
|       22 |  7192 |  |
|        - |  7193 | `/*` |
|        - |  7194 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7195 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7196 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7197 | ` */` |
|       26 |  7198 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7199 |  |
|       27 |  7200 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7201 | `	ph7_class_instance *pThis;` |
|        - |  7202 | `	ph7_class_instance *pClosureThis;` |
|        - |  7203 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7204 | `	ph7_vm_func *pFunc;` |
|        - |  7205 | `	ph7_value sResult;` |
|        - |  7206 | `	ph7_value *pCtxAttr;` |
|        - |  7207 | `	SyString sAttrName;` |
|        - |  7208 | `	sxi32 rc;` |
|       27 |  7209 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7210 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7211 | `	}` |
|       27 |  7212 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7213 | `	/* Check if already started (has a __ctx) */` |
|       27 |  7214 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       27 |  7215 | `	if( pExecCtx != 0 ){` |
|        3 |  7216 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7217 | `			"Cannot start a fiber that has already been started");` |
|        - |  7218 | `	}` |
|        - |  7219 | `	/* Resolve callable */` |
|       25 |  7220 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       25 |  7221 | `	if( pFunc == 0 ){` |
|      ! 0 |  7222 | `		return PH7_EXCEPTION;` |
|        - |  7223 | `	}` |
|        - |  7224 | `	/* Create execution context now that we know the function */` |
|       25 |  7225 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       25 |  7226 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7227 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7228 | `			"Fiber::start(): out of memory");` |
|        - |  7229 | `	}` |
|        - |  7230 | `	/* Store context in $this->__ctx */` |
|       25 |  7231 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       25 |  7232 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7233 | `	if( pCtxAttr ){` |
|       25 |  7234 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       25 |  7235 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7236 | `	}` |
|        - |  7237 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7238 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7239 | `	 * into the fiber's frame, not the caller's. */` |
|       25 |  7240 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       25 |  7241 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7242 | `	/* Unpack the args array and install into the frame */` |
|        - |  7243 | `	{` |
|       25 |  7244 | `		ph7_value **apValues = 0;` |
|       25 |  7245 | `		int nActual = 0;` |
|       25 |  7246 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       25 |  7247 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7248 | `			ph7_hashmap_node *pNode;` |
|       25 |  7249 | `			sxu32 nCount = pMap->nEntry;` |
|       25 |  7250 | `			if( nCount > 0 ){` |
|        3 |  7251 | `				sxu32 idx = 0;` |
|        4 |  7252 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7253 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7254 | `				if( apValues ){` |
|        3 |  7255 | `					pNode = pMap->pFirst;` |
|        7 |  7256 | `					while( pNode && idx < nCount ){` |
|        5 |  7257 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7258 | `						idx++;` |
|        5 |  7259 | `						pNode = pNode->pPrev;` |
|        1 |  7260 | `					}` |
|        3 |  7261 | `					nActual = (int)idx;` |
|        1 |  7262 | `				}` |
|        1 |  7263 | `			}` |
|       12 |  7264 | `		}` |
|       25 |  7265 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       25 |  7266 | `		if( apValues ){` |
|        3 |  7267 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7268 | `		}` |
|        - |  7269 | `	}` |
|        - |  7270 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       25 |  7271 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       25 |  7272 | `	pExecCtx->pFrame->pParent = 0;` |
|       25 |  7273 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7274 | `		return PH7_ABORT;` |
|        - |  7275 | `	}` |
|       25 |  7276 | `	PH7_MemObjInit(pVm, &sResult);` |
|       25 |  7277 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       25 |  7278 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7279 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7280 | `		return PH7_ABORT;` |
|        - |  7281 | `	}` |
|       25 |  7282 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7283 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7284 | `		return PH7_EXCEPTION;` |
|        - |  7285 | `	}` |
|       25 |  7286 | `	ph7_result_value(pCtx, &sResult);` |
|       25 |  7287 | `	PH7_MemObjRelease(&sResult);` |
|       25 |  7288 | `	return PH7_OK;` |
|       14 |  7289 |  |
|        - |  7290 | `/*` |
|        - |  7291 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7292 | ` */` |
|       36 |  7293 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7294 |  |
|       37 |  7295 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7296 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7297 | `	ph7_value sResult;` |
|        - |  7298 | `	ph7_value *pResumeVal;` |
|        - |  7299 | `	sxi32 rc;` |
|       37 |  7300 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7301 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7302 | `		return PH7_OK;` |
|        - |  7303 | `	}` |
|       37 |  7304 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       37 |  7305 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7306 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7307 | `		return PH7_OK;` |
|        - |  7308 | `	}` |
|       37 |  7309 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7310 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7311 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7312 | `	}` |
|       35 |  7313 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       35 |  7314 | `	PH7_MemObjInit(pVm, &sResult);` |
|       35 |  7315 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       35 |  7316 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7317 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7318 | `		return PH7_ABORT;` |
|        - |  7319 | `	}` |
|       35 |  7320 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7321 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7322 | `		return PH7_EXCEPTION;` |
|        - |  7323 | `	}` |
|       35 |  7324 | `	ph7_result_value(pCtx, &sResult);` |
|       35 |  7325 | `	PH7_MemObjRelease(&sResult);` |
|       35 |  7326 | `	return PH7_OK;` |
|       19 |  7327 |  |
|        - |  7328 | `/*` |
|        - |  7329 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7330 | ` */` |
|        6 |  7331 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7332 |  |
|        7 |  7333 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7334 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7335 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7336 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7337 | `		return PH7_OK;` |
|        - |  7338 | `	}` |
|        7 |  7339 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        7 |  7340 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7341 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7342 | `		return PH7_OK;` |
|        - |  7343 | `	}` |
|        7 |  7344 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7345 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7346 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7347 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7348 | `		}` |
|      ! 0 |  7349 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7350 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7351 | `	}` |
|        7 |  7352 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        7 |  7353 | `	return PH7_OK;` |
|        4 |  7354 |  |
|        - |  7355 | `/*` |
|        - |  7356 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7357 | ` */` |
|        6 |  7358 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7359 |  |
|        - |  7360 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7361 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7362 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7363 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7364 | `	return PH7_OK;` |
|        4 |  7365 |  |
|      ! 0 |  7366 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7367 |  |
|        - |  7368 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7369 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7370 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7371 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7372 | `	return PH7_OK;` |
|      ! 0 |  7373 |  |
|        6 |  7374 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7375 |  |
|        - |  7376 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7377 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7378 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7379 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7380 | `	return PH7_OK;` |
|        4 |  7381 |  |
|        6 |  7382 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7383 |  |
|        - |  7384 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7385 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7386 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7387 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7388 | `	return PH7_OK;` |
|        4 |  7389 |  |
|        - |  7390 | `/*` |
|        - |  7391 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7392 | ` */` |
|      ! 0 |  7393 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7394 |  |
|      ! 0 |  7395 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7396 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7397 | `	if( nArg < 1 ){` |
|      ! 0 |  7398 | `		return PH7_OK;` |
|        - |  7399 | `	}` |
|      ! 0 |  7400 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      ! 0 |  7401 | `	if( pExecCtx ){` |
|      ! 0 |  7402 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7403 | `		/* Clear the attribute so double-free is prevented */` |
|      ! 0 |  7404 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7405 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7406 | `			SyString sAttrName;` |
|        - |  7407 | `			ph7_value *pAttr;` |
|      ! 0 |  7408 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7409 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7410 | `			if( pAttr ){` |
|      ! 0 |  7411 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7412 | `			}` |
|      ! 0 |  7413 | `		}` |
|      ! 0 |  7414 | `	}` |
|      ! 0 |  7415 | `	return PH7_OK;` |
|      ! 0 |  7416 |  |
|        - |  7417 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7418 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7419 |  |
|        - |  7420 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7421 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7422 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7423 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7424 |  |
|      ! 0 |  7425 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7426 |  |
|        - |  7427 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7428 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7429 | `	ph7_exec_ctx *pCtx;` |
|        - |  7430 | `	ph7_vm_func *pFunc;` |
|        - |  7431 | `	ph7_value *pCallable;` |
|        - |  7432 | `	ph7_value *pCtxAttr;` |
|        - |  7433 | `	SyString sAttrName;` |
|        - |  7434 | `	/* Must not already be started */` |
|      ! 0 |  7435 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7436 | `	if( pCtx != 0 ){` |
|      ! 0 |  7437 | `		return SXERR_INVALID;` |
|        - |  7438 | `	}` |
|      ! 0 |  7439 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7440 | `		return SXERR_INVALID;` |
|        - |  7441 | `	}` |
|      ! 0 |  7442 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7443 | `	/* Get the callable */` |
|      ! 0 |  7444 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7445 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7446 | `	if( pCallable == 0 ){` |
|      ! 0 |  7447 | `		return SXERR_INVALID;` |
|        - |  7448 | `	}` |
|        - |  7449 | `	/* Resolve callable */` |
|      ! 0 |  7450 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7451 | `		SyString sName;` |
|        - |  7452 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7453 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7454 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7455 | `		if( pEntry == 0 ){` |
|      ! 0 |  7456 | `			return SXERR_NOTFOUND;` |
|        - |  7457 | `		}` |
|      ! 0 |  7458 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7459 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7460 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7461 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7462 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7463 | `		if( pMethod == 0 ){` |
|      ! 0 |  7464 | `			return SXERR_INVALID;` |
|        - |  7465 | `		}` |
|      ! 0 |  7466 | `		pClosureThis = pClosure;` |
|      ! 0 |  7467 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7468 | `	}else{` |
|      ! 0 |  7469 | `		return SXERR_INVALID;` |
|        - |  7470 | `	}` |
|        - |  7471 | `	/* Create context */` |
|      ! 0 |  7472 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7473 | `	if( pCtx == 0 ){` |
|      ! 0 |  7474 | `		return SXERR_MEM;` |
|        - |  7475 | `	}` |
|        - |  7476 | `	/* Store in __ctx */` |
|      ! 0 |  7477 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7478 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7479 | `	if( pCtxAttr ){` |
|      ! 0 |  7480 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7481 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7482 | `	}` |
|        - |  7483 | `	/* Set up frame with args */` |
|      ! 0 |  7484 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7485 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7486 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7487 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7488 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7489 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7490 |  |
|      ! 0 |  7491 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7492 |  |
|      ! 0 |  7493 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7494 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7495 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7496 |  |
|      ! 0 |  7497 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7498 |  |
|      ! 0 |  7499 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7500 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7501 |  |
|      ! 0 |  7502 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7503 |  |
|      ! 0 |  7504 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7505 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7506 |  |
|      ! 0 |  7507 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7508 |  |
|      ! 0 |  7509 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7510 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7511 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7512 |  |
|        - |  7513 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7514 | `/*` |
|        - |  7515 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7516 | ` */` |
|       18 |  7517 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7518 |  |
|        - |  7519 | `	ph7_generator *pGen;` |
|       19 |  7520 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       19 |  7521 | `	if( pGen == 0 ){` |
|      ! 0 |  7522 | `		return 0;` |
|        - |  7523 | `	}` |
|       19 |  7524 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       19 |  7525 | `	pGen->pCtx = pCtx;` |
|       19 |  7526 | `	pGen->iImplicitKey = 0;` |
|       19 |  7527 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       19 |  7528 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7529 | `	/* Link the generator back to the exec context */` |
|       19 |  7530 | `	pCtx->pPrivate = pGen;` |
|       19 |  7531 | `	return pGen;` |
|       10 |  7532 |  |
|        - |  7533 | `/*` |
|        - |  7534 | ` * Release a generator and its execution context.` |
|        - |  7535 | ` */` |
|      ! 0 |  7536 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7537 |  |
|      ! 0 |  7538 | `	if( pGen == 0 ){` |
|      ! 0 |  7539 | `		return;` |
|        - |  7540 | `	}` |
|      ! 0 |  7541 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7542 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7543 | `	if( pGen->pCtx ){` |
|      ! 0 |  7544 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7545 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7546 | `		pGen->pCtx = 0;` |
|      ! 0 |  7547 | `	}` |
|      ! 0 |  7548 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7549 |  |
|        - |  7550 | `/*` |
|        - |  7551 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7552 | ` */` |
|      192 |  7553 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        1 |  7554 |  |
|        - |  7555 | `	ph7_class_instance *pThis;` |
|        - |  7556 | `	SyString sAttr;` |
|        - |  7557 | `	ph7_value *pAttr;` |
|      193 |  7558 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7559 | `		return 0;` |
|        - |  7560 | `	}` |
|      193 |  7561 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      193 |  7562 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7563 | `		return 0;` |
|        - |  7564 | `	}` |
|      193 |  7565 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      193 |  7566 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      193 |  7567 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7568 | `		return 0;` |
|        - |  7569 | `	}` |
|      193 |  7570 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       97 |  7571 |  |
|        - |  7572 | `/*` |
|        - |  7573 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7574 | ` */` |
|       18 |  7575 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7576 |  |
|        - |  7577 | `	ph7_generator *pGen;` |
|        - |  7578 | `	sxi32 rc;` |
|       19 |  7579 | `	if( nArg < 1 ) return PH7_OK;` |
|       19 |  7580 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       19 |  7581 | `	if( pGen == 0 ) return PH7_OK;` |
|       19 |  7582 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       19 |  7583 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       19 |  7584 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       19 |  7585 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7586 | `	}` |
|       19 |  7587 | `	return PH7_OK;` |
|       10 |  7588 |  |
|        - |  7589 | `/*` |
|        - |  7590 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7591 | ` */` |
|       52 |  7592 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7593 |  |
|        - |  7594 | `	ph7_generator *pGen;` |
|       53 |  7595 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       53 |  7596 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       53 |  7597 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       53 |  7598 | `	return PH7_OK;` |
|       27 |  7599 |  |
|        - |  7600 | `/*` |
|        - |  7601 | ` * Generator::current() — return the last yielded value.` |
|        - |  7602 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7603 | ` */` |
|       56 |  7604 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7605 |  |
|        - |  7606 | `	ph7_generator *pGen;` |
|        - |  7607 | `	sxi32 rc;` |
|       57 |  7608 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7609 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       57 |  7610 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7611 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7612 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7613 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7614 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7615 | `	}` |
|       57 |  7616 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       57 |  7617 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       29 |  7618 | `	}else{` |
|      ! 0 |  7619 | `		ph7_result_null(pCtx);` |
|        - |  7620 | `	}` |
|       57 |  7621 | `	return PH7_OK;` |
|       29 |  7622 |  |
|        - |  7623 | `/*` |
|        - |  7624 | ` * Generator::key() — return the last yielded key.` |
|        - |  7625 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7626 | ` */` |
|       12 |  7627 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7628 |  |
|        - |  7629 | `	ph7_generator *pGen;` |
|        - |  7630 | `	sxi32 rc;` |
|       13 |  7631 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7632 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7633 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7634 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7635 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7636 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7637 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7638 | `	}` |
|       13 |  7639 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7640 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7641 | `	}else{` |
|      ! 0 |  7642 | `		ph7_result_null(pCtx);` |
|        - |  7643 | `	}` |
|       13 |  7644 | `	return PH7_OK;` |
|        7 |  7645 |  |
|        - |  7646 | `/*` |
|        - |  7647 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7648 | ` */` |
|       48 |  7649 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7650 |  |
|        - |  7651 | `	ph7_generator *pGen;` |
|        - |  7652 | `	sxi32 rc;` |
|       49 |  7653 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 |  7654 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 |  7655 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 |  7656 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7657 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 |  7658 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       49 |  7659 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       25 |  7660 | `	}else{` |
|      ! 0 |  7661 | `		return PH7_OK;` |
|        - |  7662 | `	}` |
|       49 |  7663 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 |  7664 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       49 |  7665 | `	return PH7_OK;` |
|       25 |  7666 |  |
|        - |  7667 | `/*` |
|        - |  7668 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7669 | ` */` |
|        4 |  7670 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7671 |  |
|        - |  7672 | `	ph7_generator *pGen;` |
|        - |  7673 | `	ph7_value *pSendVal;` |
|        - |  7674 | `	sxi32 rc;` |
|        5 |  7675 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7676 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7677 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7678 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7679 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7680 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7681 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7682 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7683 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7684 | `	}else{` |
|      ! 0 |  7685 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7686 | `		return PH7_OK;` |
|        - |  7687 | `	}` |
|        5 |  7688 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7689 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7690 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7691 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7692 | `	}else{` |
|        3 |  7693 | `		ph7_result_null(pCtx);` |
|        - |  7694 | `	}` |
|        5 |  7695 | `	return PH7_OK;` |
|        3 |  7696 |  |
|        - |  7697 | `/*` |
|        - |  7698 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7699 | ` *` |
|        - |  7700 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7701 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7702 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7703 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7704 | ` * the exception to the caller.` |
|        - |  7705 | ` */` |
|      ! 0 |  7706 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7707 |  |
|        - |  7708 | `	ph7_generator *pGen;` |
|        - |  7709 | `	const char *zMsg;` |
|        - |  7710 | `	int nLen;` |
|      ! 0 |  7711 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7712 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7713 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7714 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7715 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7716 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7717 | `			"Cannot throw into a closed generator");` |
|        - |  7718 | `	}` |
|        - |  7719 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7720 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7721 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7722 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7723 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7724 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7725 | `	nLen = 0;` |
|      ! 0 |  7726 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7727 | `		/* Try to get the exception's message */` |
|        - |  7728 | `		SyString sAttr;` |
|        - |  7729 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7730 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7731 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7732 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7733 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7734 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7735 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7736 | `		}` |
|      ! 0 |  7737 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7738 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7739 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7740 | `	}` |
|      ! 0 |  7741 | `	(void)nLen;` |
|      ! 0 |  7742 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7743 |  |
|        - |  7744 | `/*` |
|        - |  7745 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7746 | ` */` |
|        2 |  7747 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7748 |  |
|        - |  7749 | `	ph7_generator *pGen;` |
|        3 |  7750 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7751 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7752 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7753 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7754 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7755 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7756 | `	}` |
|        3 |  7757 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7758 | `	return PH7_OK;` |
|        2 |  7759 |  |
|        - |  7760 | `/*` |
|        - |  7761 | ` * Generator::__destruct() — clean up.` |
|        - |  7762 | ` */` |
|      ! 0 |  7763 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7764 |  |
|        - |  7765 | `	ph7_generator *pGen;` |
|      ! 0 |  7766 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7767 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7768 | `	if( pGen ){` |
|      ! 0 |  7769 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7770 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7771 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7772 | `			SyString sAttrName;` |
|        - |  7773 | `			ph7_value *pAttr;` |
|      ! 0 |  7774 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7775 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7776 | `			if( pAttr ){` |
|      ! 0 |  7777 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7778 | `			}` |
|      ! 0 |  7779 | `		}` |
|      ! 0 |  7780 | `	}` |
|      ! 0 |  7781 | `	return PH7_OK;` |
|      ! 0 |  7782 |  |
|        - |  7783 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  7784 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7785 | `/*` |
|        - |  7786 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7787 | ` * the desired message.` |
|        - |  7788 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7789 | ` * in 'api.c' for additional information.` |
|        - |  7790 | ` */` |
|      350 |  7791 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7792 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7793 | `	SyString *pString /* Message to output */` |
|        - |  7794 | `	)` |
|        2 |  7795 |  |
|      352 |  7796 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  7797 | `	sxi32 rc = SXRET_OK;` |
|        - |  7798 | `	/* Call the output consumer */` |
|      352 |  7799 | `	if( pString->nByte > 0 ){` |
|      352 |  7800 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  7801 | `		VmTrackOutput(pVm, pString->nByte);` |
|      175 |  7802 | `	}` |
|      352 |  7803 | `	return rc;` |
|        2 |  7804 |  |
|        - |  7805 | `/*` |
|        - |  7806 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7807 | ` * callback to consume the formatted message.` |
|        - |  7808 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7809 | ` * in 'api.c' for additional information.` |
|        - |  7810 | ` */` |
|        2 |  7811 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7812 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7813 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7814 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7815 | `	)` |
|        1 |  7816 |  |
|        3 |  7817 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7818 | `	sxi32 rc = SXRET_OK;` |
|        - |  7819 | `	SyBlob sWorker;` |
|        - |  7820 | `	/* Format the message and call the output consumer */` |
|        3 |  7821 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7822 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  7823 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  7824 | `		/* Consume the formatted message */` |
|        3 |  7825 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  7826 | `	}` |
|        3 |  7827 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  7828 | `	/* Release the working buffer */` |
|        3 |  7829 | `	SyBlobRelease(&sWorker);` |
|        3 |  7830 | `	return rc;` |
|        1 |  7831 |  |
|        - |  7832 | `/*` |
|        - |  7833 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  7834 | ` * This function never fail and always return a pointer` |
|        - |  7835 | ` * to a null terminated string.` |
|        - |  7836 | ` */` |
|       12 |  7837 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  7838 |  |
|       13 |  7839 | `	const char *zOp = "Unknown     ";` |
|       13 |  7840 | `	switch(nOp){` |
|        3 |  7841 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  7842 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  7843 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  7844 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  7845 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  7846 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  7847 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  7848 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  7849 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  7850 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  7851 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  7852 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  7853 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  7854 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  7855 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  7856 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  7857 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  7858 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  7859 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  7860 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  7861 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  7862 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  7863 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  7864 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  7865 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  7866 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  7867 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  7868 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  7869 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  7870 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  7871 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  7872 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  7873 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  7874 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  7875 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  7876 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  7877 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  7878 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  7879 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  7880 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  7881 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  7882 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  7883 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  7884 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  7885 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  7886 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  7887 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  7888 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  7889 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  7890 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  7891 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  7892 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  7893 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  7894 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  7895 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  7896 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  7897 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  7898 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  7899 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  7900 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  7901 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  7902 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  7903 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  7904 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  7905 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  7906 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  7907 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  7908 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  7909 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  7910 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  7911 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  7912 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  7913 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  7914 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  7915 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  7916 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  7917 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  7918 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  7919 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  7920 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  7921 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  7922 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  7923 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  7924 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  7925 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  7926 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  7927 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  7928 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  7929 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  7930 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  7931 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  7932 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  7933 | `	default:` |
|      ! 0 |  7934 | `		break;` |
|        - |  7935 | `	}` |
|       13 |  7936 | `	return zOp;` |
|        1 |  7937 |  |
|        - |  7938 | `/*` |
|        - |  7939 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  7940 | ` * The xConsumer() callback which is an used defined function` |
|        - |  7941 | ` * is responsible of consuming the generated dump.` |
|        - |  7942 | ` */` |
|        2 |  7943 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  7944 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  7945 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  7946 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  7947 | `	)` |
|        1 |  7948 |  |
|        - |  7949 | `	sxi32 rc;` |
|        3 |  7950 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  7951 | `	return rc;` |
|        1 |  7952 |  |
|        - |  7953 | `/*` |
|        - |  7954 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  7955 | ` * outside a class body [i.e: global or function scope].` |
|        - |  7956 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  7957 | ` * in 'compile.c' for additional information.` |
|        - |  7958 | ` */` |
|        8 |  7959 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  7960 |  |
|        9 |  7961 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  7962 | `	/* Evaluate and expand constant value */` |
|        9 |  7963 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  7964 |  |
|        - |  7965 | `/*` |
|        - |  7966 | ` * Section:` |
|        - |  7967 | ` *  Function handling functions.` |
|        - |  7968 | ` * Status:` |
|        - |  7969 | ` *    Stable.` |
|        - |  7970 | ` */` |
|        - |  7971 | `/*` |
|        - |  7972 | ` * int func_num_args(void)` |
|        - |  7973 | ` *   Returns the number of arguments passed to the function.` |
|        - |  7974 | ` * Parameters` |
|        - |  7975 | ` *   None.` |
|        - |  7976 | ` * Return` |
|        - |  7977 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  7978 | ` *  or -1 if called from the globe scope.` |
|        - |  7979 | ` */` |
|      916 |  7980 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7981 |  |
|        - |  7982 | `	VmFrame *pFrame;` |
|        - |  7983 | `	ph7_vm *pVm;` |
|        - |  7984 | `	/* Point to the target VM */` |
|      918 |  7985 | `	pVm = pCtx->pVm;` |
|        - |  7986 | `	/* Current frame */` |
|      918 |  7987 | `	pFrame = pVm->pFrame;` |
|      918 |  7988 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      918 |  7989 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  7990 | `		SXUNUSED(nArg);` |
|      ! 0 |  7991 | `		SXUNUSED(apArg);` |
|        - |  7992 | `		/* Global frame,return -1 */` |
|      ! 0 |  7993 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  7994 | `		return SXRET_OK;` |
|        - |  7995 | `	}` |
|        - |  7996 | `	/* Total number of arguments passed to the enclosing function */` |
|      918 |  7997 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      918 |  7998 | `	ph7_result_int(pCtx,nArg);` |
|      918 |  7999 | `	return SXRET_OK;` |
|      460 |  8000 |  |
|        - |  8001 | `/*` |
|        - |  8002 | ` * value func_get_arg(int $arg_num)` |
|        - |  8003 | ` *   Return an item from the argument list.` |
|        - |  8004 | ` * Parameters` |
|        - |  8005 | ` *  Argument number(index start from zero).` |
|        - |  8006 | ` * Return` |
|        - |  8007 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8008 | ` */` |
|       22 |  8009 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8010 |  |
|       24 |  8011 | `	ph7_value *pObj = 0;` |
|       24 |  8012 | `	VmSlot *pSlot = 0;` |
|        - |  8013 | `	VmFrame *pFrame;` |
|        - |  8014 | `	ph7_vm *pVm;` |
|        - |  8015 | `	/* Point to the target VM */` |
|       24 |  8016 | `	pVm = pCtx->pVm;` |
|        - |  8017 | `	/* Current frame */` |
|       24 |  8018 | `	pFrame = pVm->pFrame;` |
|       24 |  8019 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8020 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8021 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8022 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8023 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8024 | `		return SXRET_OK;` |
|        - |  8025 | `	}` |
|        - |  8026 | `	/* Extract the desired index */` |
|       21 |  8027 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8028 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8029 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8030 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8031 | `		return SXRET_OK;` |
|        - |  8032 | `	}` |
|        - |  8033 | `	/* Extract the desired argument */` |
|       21 |  8034 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8035 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8036 | `			/* Return the desired argument */` |
|       21 |  8037 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8038 | `		}else{` |
|        - |  8039 | `			/* No such argument,return false */` |
|      ! 0 |  8040 | `			ph7_result_bool(pCtx,0);` |
|        - |  8041 | `		}` |
|       11 |  8042 | `	}else{` |
|        - |  8043 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8044 | `		ph7_result_bool(pCtx,0);` |
|        - |  8045 | `	}` |
|       21 |  8046 | `	return SXRET_OK;` |
|       13 |  8047 |  |
|        - |  8048 | `/*` |
|        - |  8049 | ` * array func_get_args_byref(void)` |
|        - |  8050 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8051 | ` * Parameters` |
|        - |  8052 | ` *  None.` |
|        - |  8053 | ` * Return` |
|        - |  8054 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8055 | ` *  member of the current user-defined function's argument list.` |
|        - |  8056 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8057 | ` * NOTE:` |
|        - |  8058 | ` *  Arguments are returned to the array by reference.` |
|        - |  8059 | ` */` |
|        2 |  8060 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8061 |  |
|        - |  8062 | `	ph7_value *pArray;` |
|        - |  8063 | `	VmFrame *pFrame;` |
|        - |  8064 | `	VmSlot *aSlot;` |
|        - |  8065 | `	sxu32 n;` |
|        - |  8066 | `	/* Point to the current frame */` |
|        3 |  8067 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8068 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8069 | `	if( pFrame->pParent == 0 ){` |
|        - |  8070 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8071 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8072 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8073 | `		return SXRET_OK;` |
|        - |  8074 | `	}` |
|        - |  8075 | `	/* Create a new array */` |
|        3 |  8076 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8077 | `	if( pArray == 0 ){` |
|      ! 0 |  8078 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8079 | `		SXUNUSED(apArg);` |
|      ! 0 |  8080 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8081 | `		return SXRET_OK;` |
|        - |  8082 | `	}` |
|        - |  8083 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8084 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8085 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8086 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8087 | `	}` |
|        - |  8088 | `	/* Return the freshly created array */` |
|        3 |  8089 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8090 | `	return SXRET_OK;` |
|        2 |  8091 |  |
|        - |  8092 | `/*` |
|        - |  8093 | ` * array func_get_args(void)` |
|        - |  8094 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8095 | ` * Parameters` |
|        - |  8096 | ` *  None.` |
|        - |  8097 | ` * Return` |
|        - |  8098 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8099 | ` *  member of the current user-defined function's argument list.` |
|        - |  8100 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8101 | ` */` |
|       88 |  8102 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8103 |  |
|       90 |  8104 | `	ph7_value *pObj = 0;` |
|        - |  8105 | `	ph7_value *pArray;` |
|        - |  8106 | `	VmFrame *pFrame;` |
|        - |  8107 | `	VmSlot *aSlot;` |
|        - |  8108 | `	sxu32 n;` |
|        - |  8109 | `	/* Point to the current frame */` |
|       90 |  8110 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8111 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8112 | `	if( pFrame->pParent == 0 ){` |
|        - |  8113 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8114 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8115 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8116 | `		return SXRET_OK;` |
|        - |  8117 | `	}` |
|        - |  8118 | `	/* Create a new array */` |
|       90 |  8119 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8120 | `	if( pArray == 0 ){` |
|      ! 0 |  8121 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8122 | `		SXUNUSED(apArg);` |
|      ! 0 |  8123 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8124 | `		return SXRET_OK;` |
|        - |  8125 | `	}` |
|        - |  8126 | `	/* Start filling the array with the given arguments */` |
|       90 |  8127 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8128 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8129 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8130 | `		if( pObj ){` |
|      134 |  8131 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8132 | `		}` |
|       68 |  8133 | `	}` |
|        - |  8134 | `	/* Return the freshly created array */` |
|       90 |  8135 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8136 | `	return SXRET_OK;` |
|       46 |  8137 |  |
|        - |  8138 | `/*` |
|        - |  8139 | ` * bool function_exists(string $name)` |
|        - |  8140 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8141 | ` * Parameters` |
|        - |  8142 | ` *  The name of the desired function.` |
|        - |  8143 | ` * Return` |
|        - |  8144 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8145 | ` */` |
|     1682 |  8146 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8147 |  |
|        - |  8148 | `	const char *zName;` |
|        - |  8149 | `	ph7_vm *pVm;` |
|        - |  8150 | `	int nLen;` |
|        - |  8151 | `	int res;` |
|     1684 |  8152 | `	if( nArg < 1 ){` |
|        - |  8153 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8154 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8155 | `		return SXRET_OK;` |
|        - |  8156 | `	}` |
|        - |  8157 | `	/* Point to the target VM */` |
|     1684 |  8158 | `	pVm = pCtx->pVm;` |
|        - |  8159 | `	/* Extract the function name */` |
|     1684 |  8160 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8161 | `	/* Assume the function is not defined */` |
|     1684 |  8162 | `	res = 0;` |
|        - |  8163 | `	/* Perform the lookup */` |
|     2523 |  8164 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8165 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8166 | `			/* Function is defined */` |
|      206 |  8167 | `			res = 1;` |
|      102 |  8168 | `	}` |
|     1684 |  8169 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8170 | `	return SXRET_OK;` |
|      843 |  8171 |  |
|        - |  8172 | `/*` |
|        - |  8173 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8174 | ` * [i.e: Whether it is callable or not].` |
|        - |  8175 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8176 | ` */` |
|    16234 |  8177 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8178 |  |
|    16236 |  8179 | `	int res = 0;` |
|    16236 |  8180 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8181 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8182 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8183 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8184 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8185 | `		if( pMethod && CallInvoke ){` |
|        - |  8186 | `			ph7_value sResult;` |
|        - |  8187 | `			sxi32 rc;` |
|        - |  8188 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8189 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8190 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8191 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8192 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8193 | `			}` |
|      ! 0 |  8194 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8195 | `		}` |
|    16236 |  8196 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8197 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8198 | `		if( pMap->nEntry == 2 ){` |
|        - |  8199 | `			ph7_class *pClass;` |
|        - |  8200 | `			ph7_value *pV;` |
|        - |  8201 | `			/* Extract the target class */` |
|       12 |  8202 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8203 | `			if( pV ){` |
|       12 |  8204 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8205 | `				if( pClass ){` |
|        - |  8206 | `					ph7_class_method *pMethod;` |
|        - |  8207 | `					/* Extract the target method */` |
|       10 |  8208 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8209 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8210 | `						/* Perform the lookup */` |
|       10 |  8211 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8212 | `						if( pMethod ){` |
|        - |  8213 | `							/* Method is callable */` |
|        5 |  8214 | `							res = 1;` |
|        2 |  8215 | `						}` |
|        4 |  8216 | `					}` |
|        4 |  8217 | `				}` |
|        5 |  8218 | `			}` |
|        7 |  8219 | `		}` |
|    16223 |  8220 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8221 | `		const char *zName;` |
|        - |  8222 | `		int nLen;` |
|        - |  8223 | `		/* Extract the name */` |
|     4750 |  8224 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8225 | `		/* Perform the lookup */` |
|     4765 |  8226 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8227 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8228 | `				/* Function is callable */` |
|     4732 |  8229 | `				res = 1;` |
|     2365 |  8230 | `		}` |
|     2374 |  8231 | `	}` |
|    16236 |  8232 | `	return res;` |
|        2 |  8233 |  |
|        - |  8234 | `/*` |
|        - |  8235 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8236 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8237 | ` * Parameters` |
|        - |  8238 | ` * $name` |
|        - |  8239 | ` *    The callback function to check` |
|        - |  8240 | ` * $syntax_only` |
|        - |  8241 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8242 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8243 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8244 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8245 | ` *    a string.` |
|        - |  8246 | ` * Return` |
|        - |  8247 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8248 | ` */` |
|       14 |  8249 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8250 |  |
|        - |  8251 | `	ph7_vm *pVm;` |
|        - |  8252 | `	int res;` |
|       15 |  8253 | `	if( nArg < 1 ){` |
|        - |  8254 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8255 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8256 | `		return SXRET_OK;` |
|        - |  8257 | `	}` |
|        - |  8258 | `	/* Point to the target VM */` |
|       15 |  8259 | `	pVm = pCtx->pVm;` |
|        - |  8260 | `	/* Perform the requested operation */` |
|       15 |  8261 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8262 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8263 | `	return SXRET_OK;` |
|        8 |  8264 |  |
|        - |  8265 | `/*` |
|        - |  8266 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8267 | ` * defined below.` |
|        - |  8268 | ` */` |
|     1172 |  8269 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8270 |  |
|     1173 |  8271 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8272 | `	ph7_value sName;` |
|        - |  8273 | `	sxi32 rc;` |
|        - |  8274 | `	/* Prepare the function name for insertion */` |
|     1173 |  8275 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1173 |  8276 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8277 | `	/* Perform the insertion */` |
|     1173 |  8278 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1173 |  8279 | `	PH7_MemObjRelease(&sName);` |
|     1173 |  8280 | `	return rc;` |
|        1 |  8281 |  |
|        - |  8282 | `/*` |
|        - |  8283 | ` * array get_defined_functions(void)` |
|        - |  8284 | ` *  Returns an array of all defined functions.` |
|        - |  8285 | ` * Parameter` |
|        - |  8286 | ` *  None.` |
|        - |  8287 | ` * Return` |
|        - |  8288 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8289 | ` *  both built-in (internal) and user-defined.` |
|        - |  8290 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8291 | ` *  defined ones using $arr["user"].` |
|        - |  8292 | ` * Note:` |
|        - |  8293 | ` *  NULL is returned on failure.` |
|        - |  8294 | ` */` |
|        2 |  8295 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8296 |  |
|        - |  8297 | `	ph7_value *pArray,*pEntry;` |
|        - |  8298 | `	/* NOTE:` |
|        - |  8299 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8300 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8301 | `	 */` |
|        3 |  8302 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8303 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8304 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8305 | `		SXUNUSED(apArg);` |
|        - |  8306 | `		/* Return NULL */` |
|      ! 0 |  8307 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8308 | `		return SXRET_OK;` |
|        - |  8309 | `	}` |
|        3 |  8310 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8311 | `	if( pEntry == 0 ){` |
|        - |  8312 | `		/* Return NULL */` |
|      ! 0 |  8313 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8314 | `		return SXRET_OK;` |
|        - |  8315 | `	}` |
|        - |  8316 | `	/* Fill with the appropriate information */` |
|        3 |  8317 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8318 | `	/* Create the 'internal' index */` |
|        3 |  8319 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8320 | `	/* Create the user-func array */` |
|        3 |  8321 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8322 | `	if( pEntry == 0 ){` |
|        - |  8323 | `		/* Return NULL */` |
|      ! 0 |  8324 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8325 | `		return SXRET_OK;` |
|        - |  8326 | `	}` |
|        - |  8327 | `	/* Fill with the appropriate information */` |
|        3 |  8328 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8329 | `	/* Create the 'user' index */` |
|        3 |  8330 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8331 | `	/* Return the multi-dimensional array */` |
|        3 |  8332 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8333 | `	return SXRET_OK;` |
|        2 |  8334 |  |
|        - |  8335 | `/*` |
|        - |  8336 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8337 | ` *  Register a function for execution on shutdown.` |
|        - |  8338 | ` * Note` |
|        - |  8339 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8340 | ` *  be called in the same order as they were registered.` |
|        - |  8341 | ` * Parameters` |
|        - |  8342 | ` *  $callback` |
|        - |  8343 | ` *   The shutdown callback to register.` |
|        - |  8344 | ` * $param` |
|        - |  8345 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8346 | ` * Return` |
|        - |  8347 | ` *  Nothing.` |
|        - |  8348 | ` */` |
|        2 |  8349 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8350 |  |
|        - |  8351 | `	VmShutdownCB sEntry;` |
|        - |  8352 | `	int i,j;` |
|        3 |  8353 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8354 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8355 | `		return PH7_OK;` |
|        - |  8356 | `	}` |
|        - |  8357 | `	/* Zero the Entry */` |
|        3 |  8358 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8359 | `	/* Initialize fields */` |
|        3 |  8360 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8361 | `	/* Save the callback name for later invocation name */` |
|        3 |  8362 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8363 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8364 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8365 | `	}` |
|        - |  8366 | `	/* Copy arguments */` |
|        3 |  8367 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8368 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8369 | `			/* Limit reached */` |
|      ! 0 |  8370 | `			break;` |
|        - |  8371 | `		}` |
|      ! 0 |  8372 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8373 | `	}` |
|        3 |  8374 | `	sEntry.nArg = j;` |
|        - |  8375 | `	/* Install the callback */` |
|        3 |  8376 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8377 | `	return PH7_OK;` |
|        2 |  8378 |  |
|        - |  8379 | `/*` |
|        - |  8380 | ` * Section:` |
|        - |  8381 | ` *  Class handling functions.` |
|        - |  8382 | ` * Status:` |
|        - |  8383 | ` *    Stable.` |
|        - |  8384 | ` */` |
|        - |  8385 | `/*` |
|        - |  8386 | ` * Extract the top active class. NULL is returned` |
|        - |  8387 | ` * if the class stack is empty.` |
|        - |  8388 | ` */` |
|      556 |  8389 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8390 |  |
|      558 |  8391 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8392 | `	ph7_class **apClass;` |
|      558 |  8393 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8394 | `		/* Empty stack,return NULL */` |
|       15 |  8395 | `		return 0;` |
|        - |  8396 | `	}` |
|        - |  8397 | `	/* Peek the last entry */` |
|      544 |  8398 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      544 |  8399 | `	return apClass[pSet->nUsed - 1];` |
|      280 |  8400 |  |
|        - |  8401 | `/*` |
|        - |  8402 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8403 | ` *   Get the class that declared the currently executing method.` |
|        - |  8404 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8405 | ` *` |
|        - |  8406 | ` * Parameters` |
|        - |  8407 | ` *   pVm: Target VM` |
|        - |  8408 | ` *` |
|        - |  8409 | ` * Return` |
|        - |  8410 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8411 | ` *   - Not executing within a class method` |
|        - |  8412 | ` *` |
|        - |  8413 | ` * Note` |
|        - |  8414 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8415 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8416 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8417 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8418 | ` *   declaring class.` |
|        - |  8419 | ` */` |
|       52 |  8420 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8421 |  |
|       54 |  8422 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8423 | `	ph7_vm_func *pVmFunc;` |
|        - |  8424 |  |
|        - |  8425 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  8426 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8427 |  |
|        - |  8428 | `	/* Check if we're in a method context */` |
|       54 |  8429 | `	if( pFrame->pParent ){` |
|       50 |  8430 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  8431 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8432 | `			/* Return the declaring class */` |
|       50 |  8433 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8434 | `		}` |
|      ! 0 |  8435 | `	}` |
|        - |  8436 |  |
|        5 |  8437 | `	return 0;` |
|       28 |  8438 |  |
|        - |  8439 |  |
|        - |  8440 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8441 | `/*` |
|        - |  8442 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8443 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8444 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8445 | ` * return value indicates failure.` |
|        - |  8446 | ` */` |
|     1484 |  8447 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8448 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8449 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8450 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8451 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8452 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8453 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8454 | `	)` |
|        2 |  8455 |  |
|        - |  8456 | `	ph7_value *aStack;` |
|        - |  8457 | `	VmInstr aInstr[2];` |
|        - |  8458 | `	int iCursor;` |
|        - |  8459 | `	int i;` |
|        - |  8460 | `	/* Create a new operand stack */` |
|     1486 |  8461 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1486 |  8462 | `	if( aStack == 0 ){` |
|      ! 0 |  8463 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8464 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8465 | `		return SXERR_MEM;` |
|        - |  8466 | `	}` |
|        - |  8467 | `	/* Fill the operand stack with the given arguments */` |
|     2088 |  8468 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      604 |  8469 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8470 | `		/*` |
|        - |  8471 | `		 * Symisc eXtension:` |
|        - |  8472 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8473 | `		 */` |
|      604 |  8474 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      303 |  8475 | `	}` |
|     1486 |  8476 | `	iCursor = nArg + 1;` |
|     1486 |  8477 | `	if( pThis ){` |
|        - |  8478 | `		/*` |
|        - |  8479 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8480 | `		 */` |
|     1480 |  8481 | `		pThis->iRef++; /* Increment reference count */` |
|     1480 |  8482 | `		aStack[i].x.pOther = pThis;` |
|     1480 |  8483 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      739 |  8484 | `	}` |
|     1486 |  8485 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1486 |  8486 | `	i++;` |
|        - |  8487 | `	/* Push method name */` |
|     1486 |  8488 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1486 |  8489 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1486 |  8490 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1486 |  8491 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8492 | `	/* Emit the CALL istruction */` |
|     1486 |  8493 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1486 |  8494 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1486 |  8495 | `	aInstr[0].iP2 = 0;` |
|     1486 |  8496 | `	aInstr[0].p3  = 0;` |
|        - |  8497 | `	/* Emit the DONE instruction */` |
|     1486 |  8498 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1486 |  8499 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1486 |  8500 | `	aInstr[1].iP2 = 0;` |
|     1486 |  8501 | `	aInstr[1].p3  = 0;` |
|        - |  8502 | `	/* Execute the method body (if available) */` |
|     1486 |  8503 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8504 | `	/* Clean up the mess left behind */` |
|     1486 |  8505 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1486 |  8506 | `	return PH7_OK;` |
|      744 |  8507 |  |
|        - |  8508 | `/*` |
|        - |  8509 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8510 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8511 | ` * in the apArg[] array.` |
|        - |  8512 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8513 | ` * return value indicates failure.` |
|        - |  8514 | ` */` |
|      926 |  8515 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8516 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8517 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8518 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8519 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8520 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8521 | `	)` |
|        2 |  8522 |  |
|        - |  8523 | `	ph7_value *aStack;` |
|        - |  8524 | `	VmInstr aInstr[2];` |
|        - |  8525 | `	int i;` |
|      928 |  8526 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8527 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8528 | `		if( pResult ){` |
|        - |  8529 | `			/* Assume a null return value */` |
|      ! 0 |  8530 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8531 | `		}` |
|      471 |  8532 | `		return SXERR_INVALID;` |
|        - |  8533 | `	}` |
|      458 |  8534 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8535 | `		/* Class method */` |
|       11 |  8536 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8537 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8538 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8539 | `		ph7_class *pClass = 0;` |
|        - |  8540 | `		ph7_value *pValue;` |
|        - |  8541 | `		sxi32 rc;` |
|       11 |  8542 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8543 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8544 | `			if( pResult ){` |
|        - |  8545 | `				/* Assume a null return value */` |
|      ! 0 |  8546 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8547 | `			}` |
|      ! 0 |  8548 | `			return SXRET_OK;` |
|        - |  8549 | `		}` |
|        - |  8550 | `		/* Extract the class name or an instance of it */` |
|       11 |  8551 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8552 | `		if( pValue ){` |
|       11 |  8553 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8554 | `		}` |
|       11 |  8555 | `		if( pClass == 0 ){` |
|        - |  8556 | `			/* No such class,return NULL */` |
|      ! 0 |  8557 | `			if( pResult ){` |
|      ! 0 |  8558 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8559 | `			}` |
|      ! 0 |  8560 | `			return SXRET_OK;` |
|        - |  8561 | `		}` |
|       11 |  8562 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8563 | `			/* Point to the class instance */` |
|        5 |  8564 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8565 | `		}` |
|        - |  8566 | `		/* Try to extract the method */` |
|       11 |  8567 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8568 | `		if( pValue ){` |
|       11 |  8569 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8570 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8571 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8572 | `			}` |
|        5 |  8573 | `		}` |
|       11 |  8574 | `		if( pMethod == 0 ){` |
|        - |  8575 | `			/* No such method,return NULL */` |
|      ! 0 |  8576 | `			if( pResult ){` |
|      ! 0 |  8577 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8578 | `			}` |
|      ! 0 |  8579 | `			return SXRET_OK;` |
|        - |  8580 | `		}` |
|        - |  8581 | `		/* Call the class method */` |
|       11 |  8582 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8583 | `		return rc;` |
|        - |  8584 | `	}` |
|        - |  8585 | `	/* Create a new operand stack */` |
|      448 |  8586 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  8587 | `	if( aStack == 0 ){` |
|      ! 0 |  8588 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8589 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8590 | `		if( pResult ){` |
|        - |  8591 | `			/* Assume a null return value */` |
|      ! 0 |  8592 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8593 | `		}` |
|      ! 0 |  8594 | `		return SXERR_MEM;` |
|        - |  8595 | `	}` |
|        - |  8596 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  8597 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  8598 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8599 | `		/*` |
|        - |  8600 | `		 * Symisc eXtension:` |
|        - |  8601 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8602 | `		 */` |
|     1024 |  8603 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  8604 | `	}` |
|        - |  8605 | `	/* Push the function name */` |
|      448 |  8606 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  8607 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8608 | `	/* Emit the CALL istruction */` |
|      448 |  8609 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  8610 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  8611 | `	aInstr[0].iP2 = 0;` |
|      448 |  8612 | `	aInstr[0].p3  = 0;` |
|        - |  8613 | `	/* Emit the DONE instruction */` |
|      448 |  8614 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  8615 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  8616 | `	aInstr[1].iP2 = 0;` |
|      448 |  8617 | `	aInstr[1].p3  = 0;` |
|        - |  8618 | `	/* Execute the function body (if available) */` |
|      448 |  8619 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8620 | `	/* Clean up the mess left behind */` |
|      448 |  8621 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  8622 | `	return PH7_OK;` |
|      465 |  8623 |  |
|        - |  8624 | `/*` |
|        - |  8625 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8626 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8627 | ` * parameter.` |
|        - |  8628 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8629 | ` * return value indicates failure.` |
|        - |  8630 | ` */` |
|      236 |  8631 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8632 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8633 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8634 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8635 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8636 | `	)` |
|        1 |  8637 |  |
|        - |  8638 | `	ph7_value *pArg;` |
|        - |  8639 | `	SySet aArg;` |
|        - |  8640 | `	va_list ap;` |
|        - |  8641 | `	sxi32 rc;` |
|      237 |  8642 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8643 | `	/* Copy arguments one after one */` |
|      237 |  8644 | `	va_start(ap,pResult);` |
|      393 |  8645 | `	for(;;){` |
|      787 |  8646 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8647 | `		if( pArg == 0 ){` |
|      237 |  8648 | `			break;` |
|        - |  8649 | `		}` |
|      551 |  8650 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8651 | `	}` |
|        - |  8652 | `	/* Call the core routine */` |
|      237 |  8653 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8654 | `	/* Cleanup */` |
|      237 |  8655 | `	SySetRelease(&aArg);` |
|      237 |  8656 | `	return rc;` |
|        1 |  8657 |  |
|        - |  8658 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8659 | `/*` |
|        - |  8660 | ` * bool defined(string $name)` |
|        - |  8661 | ` *  Checks whether a given named constant exists.` |
|        - |  8662 | ` * Parameter:` |
|        - |  8663 | ` *  Name of the desired constant.` |
|        - |  8664 | ` * Return` |
|        - |  8665 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8666 | ` */` |
|       14 |  8667 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8668 |  |
|        - |  8669 | `	const char *zName;` |
|       16 |  8670 | `	int nLen = 0;` |
|       16 |  8671 | `	int res = 0;` |
|       16 |  8672 | `	if( nArg < 1 ){` |
|        - |  8673 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8674 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8675 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8676 | `		return SXRET_OK;` |
|        - |  8677 | `	}` |
|        - |  8678 | `	/* Extract constant name */` |
|       16 |  8679 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8680 | `	/* Perform the lookup */` |
|       16 |  8681 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8682 | `		/* Already defined */` |
|       10 |  8683 | `		res = 1;` |
|        4 |  8684 | `	}` |
|       16 |  8685 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8686 | `	return SXRET_OK;` |
|        9 |  8687 |  |
|        - |  8688 | `/*` |
|        - |  8689 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8690 | ` * below.` |
|        - |  8691 | ` */` |
|        8 |  8692 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8693 |  |
|       10 |  8694 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8695 | `	/* Expand constant value */` |
|       10 |  8696 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8697 |  |
|        - |  8698 | `/*` |
|        - |  8699 | ` * bool define(string $constant_name,expression value)` |
|        - |  8700 | ` *  Defines a named constant at runtime.` |
|        - |  8701 | ` * Parameter:` |
|        - |  8702 | ` *  $constant_name` |
|        - |  8703 | ` *   The name of the constant` |
|        - |  8704 | ` *  $value` |
|        - |  8705 | ` *   Constant value` |
|        - |  8706 | ` * Return:` |
|        - |  8707 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8708 | ` */` |
|       10 |  8709 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8710 |  |
|        - |  8711 | `	const char *zName;  /* Constant name */` |
|        - |  8712 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8713 | `	int nLen = 0;       /* Name length */` |
|        - |  8714 | `	sxi32 rc;` |
|       12 |  8715 | `	if( nArg < 2 ){` |
|        - |  8716 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8717 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8718 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8719 | `		return SXRET_OK;` |
|        - |  8720 | `	}` |
|       12 |  8721 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8722 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8723 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8724 | `		return SXRET_OK;` |
|        - |  8725 | `	}` |
|        - |  8726 | `	/* Extract constant name */` |
|       12 |  8727 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8728 | `	if( nLen < 1 ){` |
|      ! 0 |  8729 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8730 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8731 | `		return SXRET_OK;` |
|        - |  8732 | `	}` |
|        - |  8733 | `	/* Duplicate constant value */` |
|       12 |  8734 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8735 | `	if( pValue == 0 ){` |
|      ! 0 |  8736 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8737 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8738 | `		return SXRET_OK;` |
|        - |  8739 | `	}` |
|        - |  8740 | `	/* Initialize the memory object */` |
|       12 |  8741 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8742 | `	/* Register the constant */` |
|       12 |  8743 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8744 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8745 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8746 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8747 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8748 | `		return SXRET_OK;` |
|        - |  8749 | `	}` |
|        - |  8750 | `	/* Duplicate constant value */` |
|       12 |  8751 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8752 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8753 | `		/* Lower case the constant name */` |
|      ! 0 |  8754 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8755 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8756 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8757 | `				/* UTF-8 stream */` |
|      ! 0 |  8758 | `				zCur++;` |
|      ! 0 |  8759 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8760 | `					zCur++;` |
|      ! 0 |  8761 | `				}` |
|      ! 0 |  8762 | `				continue;` |
|        - |  8763 | `			}` |
|      ! 0 |  8764 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8765 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8766 | `				zCur[0] = (char)c;` |
|      ! 0 |  8767 | `			}` |
|      ! 0 |  8768 | `			zCur++;` |
|      ! 0 |  8769 | `		}` |
|        - |  8770 | `		/* Finally,register the constant */` |
|      ! 0 |  8771 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8772 | `	}` |
|        - |  8773 | `	/* All done,return TRUE */` |
|       12 |  8774 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8775 | `	return SXRET_OK;` |
|        7 |  8776 |  |
|        - |  8777 | `/*` |
|        - |  8778 | ` * value constant(string $name)` |
|        - |  8779 | ` *  Returns the value of a constant` |
|        - |  8780 | ` * Parameter` |
|        - |  8781 | ` *  $name` |
|        - |  8782 | ` *    Name of the constant.` |
|        - |  8783 | ` * Return` |
|        - |  8784 | ` *  Constant value or NULL if not defined.` |
|        - |  8785 | ` */` |
|        8 |  8786 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8787 |  |
|        - |  8788 | `	SyHashEntry *pEntry;` |
|        - |  8789 | `	ph7_constant *pCons;` |
|        - |  8790 | `	const char *zName; /* Constant name */` |
|        - |  8791 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8792 | `	int nLen;` |
|       10 |  8793 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8794 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8795 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8796 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8797 | `		return SXRET_OK;` |
|        - |  8798 | `	}` |
|        - |  8799 | `	/* Extract the constant name */` |
|       10 |  8800 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8801 | `	/* Perform the query */` |
|       10 |  8802 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8803 | `	if( pEntry == 0 ){` |
|        3 |  8804 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8805 | `		ph7_result_null(pCtx);` |
|        3 |  8806 | `		return SXRET_OK;` |
|        - |  8807 | `	}` |
|        8 |  8808 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8809 | `	/* Point to the structure that describe the constant */` |
|        8 |  8810 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8811 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8812 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8813 | `	/* Return that value */` |
|        8 |  8814 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8815 | `	/* Cleanup */` |
|        8 |  8816 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8817 | `	return SXRET_OK;` |
|        6 |  8818 |  |
|        - |  8819 | `/*` |
|        - |  8820 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8821 | ` * defined below.` |
|        - |  8822 | ` */` |
|      416 |  8823 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8824 |  |
|      417 |  8825 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8826 | `	ph7_value sName;` |
|        - |  8827 | `	sxi32 rc;` |
|        - |  8828 | `	/* Prepare the constant name for insertion */` |
|      417 |  8829 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  8830 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8831 | `	/* Perform the insertion */` |
|      417 |  8832 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  8833 | `	PH7_MemObjRelease(&sName);` |
|      417 |  8834 | `	return rc;` |
|        1 |  8835 |  |
|        - |  8836 | `/*` |
|        - |  8837 | ` * array get_defined_constants(void)` |
|        - |  8838 | ` *  Returns an associative array with the names of all defined` |
|        - |  8839 | ` *  constants.` |
|        - |  8840 | ` * Parameters` |
|        - |  8841 | ` *  NONE.` |
|        - |  8842 | ` * Returns` |
|        - |  8843 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8844 | ` */` |
|        2 |  8845 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8846 |  |
|        - |  8847 | `	ph7_value *pArray;` |
|        - |  8848 | `	/* Create the array first*/` |
|        3 |  8849 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8850 | `	if( pArray == 0 ){` |
|      ! 0 |  8851 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8852 | `		SXUNUSED(apArg);` |
|        - |  8853 | `		/* Return NULL */` |
|      ! 0 |  8854 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8855 | `		return SXRET_OK;` |
|        - |  8856 | `	}` |
|        - |  8857 | `	/* Fill the array with the defined constants */` |
|        3 |  8858 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8859 | `	/* Return the created array */` |
|        3 |  8860 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8861 | `	return SXRET_OK;` |
|        2 |  8862 |  |
|        - |  8863 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  8864 | `/*` |
|        - |  8865 | ` * Section:` |
|        - |  8866 | ` *  Random numbers/string generators.` |
|        - |  8867 | ` * Status:` |
|        - |  8868 | ` *    Stable.` |
|        - |  8869 | ` */` |
|        - |  8870 | `/*` |
|        - |  8871 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8872 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8873 | ` * used by te SQLite3 library.` |
|        - |  8874 | ` */` |
|     2549 |  8875 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8876 |  |
|        - |  8877 | `	sxu32 iNum;` |
|     2551 |  8878 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2551 |  8879 | `	return iNum;` |
|        2 |  8880 |  |
|        - |  8881 | `/*` |
|        - |  8882 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8883 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8884 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8885 | ` * by te SQLite3 library.` |
|        - |  8886 | ` */` |
|   131900 |  8887 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8888 |  |
|        - |  8889 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8890 | `	int i;` |
|        - |  8891 | `	/* Generate a binary string first */` |
|   131902 |  8892 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8893 | `	/* Turn the binary string into english based alphabet */` |
|  1451070 |  8894 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1319170 |  8895 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   659586 |  8896 | `	 }` |
|   131902 |  8897 |  |
|        - |  8898 | `/*` |
|        - |  8899 | ` * int rand()` |
|        - |  8900 | ` * int mt_rand()` |
|        - |  8901 | ` * int rand(int $min,int $max)` |
|        - |  8902 | ` * int mt_rand(int $min,int $max)` |
|        - |  8903 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8904 | ` * Parameter` |
|        - |  8905 | ` *  $min` |
|        - |  8906 | ` *    The lowest value to return (default: 0)` |
|        - |  8907 | ` *  $max` |
|        - |  8908 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8909 | ` * Return` |
|        - |  8910 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8911 | ` * Note:` |
|        - |  8912 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8913 | ` *  by te SQLite3 library.` |
|        - |  8914 | ` */` |
|       20 |  8915 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8916 |  |
|        - |  8917 | `	sxu32 iNum;` |
|        - |  8918 | `	/* Generate the random number */` |
|       21 |  8919 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8920 | `	if( nArg > 1 ){` |
|        - |  8921 | `		sxu32 iMin,iMax;` |
|        3 |  8922 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8923 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8924 | `		if( iMin < iMax ){` |
|        3 |  8925 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8926 | `			if( iDiv > 0 ){` |
|        3 |  8927 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8928 | `			}` |
|        1 |  8929 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8930 | `			iNum %= iMax;` |
|      ! 0 |  8931 | `		}` |
|        1 |  8932 | `	}` |
|        - |  8933 | `	/* Return the number */` |
|       21 |  8934 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8935 | `	return SXRET_OK;` |
|        1 |  8936 |  |
|        - |  8937 | `/*` |
|        - |  8938 | ` * int getrandmax(void)` |
|        - |  8939 | ` * int mt_getrandmax(void)` |
|        - |  8940 | ` * int rc4_getrandmax(void)` |
|        - |  8941 | ` *   Show largest possible random value` |
|        - |  8942 | ` * Return` |
|        - |  8943 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8944 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8945 | ` * Note:` |
|        - |  8946 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8947 | ` *  by te SQLite3 library.` |
|        - |  8948 | ` */` |
|        4 |  8949 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8950 |  |
|        2 |  8951 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8952 | `	SXUNUSED(apArg);` |
|        5 |  8953 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8954 | `	return SXRET_OK;` |
|        1 |  8955 |  |
|        - |  8956 | `/*` |
|        - |  8957 | ` * string rand_str()` |
|        - |  8958 | ` * string rand_str(int $len)` |
|        - |  8959 | ` *  Generate a random string (English alphabet).` |
|        - |  8960 | ` * Parameter` |
|        - |  8961 | ` *  $len` |
|        - |  8962 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8963 | ` * Return` |
|        - |  8964 | ` *   A pseudo random string.` |
|        - |  8965 | ` * Note:` |
|        - |  8966 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8967 | ` *  by te SQLite3 library.` |
|        - |  8968 | ` *  This function is a symisc extension.` |
|        - |  8969 | ` */` |
|      120 |  8970 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8971 |  |
|        - |  8972 | `	char zString[1024];` |
|      122 |  8973 | `	int iLen = 0x10;` |
|      122 |  8974 | `	if( nArg > 0 ){` |
|        - |  8975 | `		/* Get the desired length */` |
|      122 |  8976 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8977 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8978 | `			/* Default length */` |
|        3 |  8979 | `			iLen = 0x10;` |
|        1 |  8980 | `		}` |
|       60 |  8981 | `	}` |
|        - |  8982 | `	/* Generate the random string */` |
|      122 |  8983 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8984 | `	/* Return the generated string */` |
|      122 |  8985 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8986 | `	return SXRET_OK;` |
|        2 |  8987 |  |
|        - |  8988 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8989 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8990 | `/* Unique ID private data */` |
|        - |  8991 | `struct unique_id_data` |
|        - |  8992 |  |
|        - |  8993 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8994 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8995 | `};` |
|        - |  8996 | `/*` |
|        - |  8997 | ` * Binary to hex consumer callback.` |
|        - |  8998 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8999 | ` * defined below.` |
|        - |  9000 | ` */` |
|      192 |  9001 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9002 |  |
|      193 |  9003 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9004 | `	sxu32 nBuflen;` |
|        - |  9005 | `	/* Extract result buffer length */` |
|      193 |  9006 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9007 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9008 | `			/*` |
|        - |  9009 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9010 | `			 * string will be 13 characters long` |
|        - |  9011 | `			 */` |
|       25 |  9012 | `		return SXERR_ABORT;` |
|        - |  9013 | `	}` |
|      169 |  9014 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9015 | `		return SXERR_ABORT;` |
|        - |  9016 | `	}` |
|        - |  9017 | `	/* Safely Consume the hex stream */` |
|      169 |  9018 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9019 | `	return SXRET_OK;` |
|       97 |  9020 |  |
|        - |  9021 | `/*` |
|        - |  9022 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9023 | ` *  Generate a unique ID` |
|        - |  9024 | ` * Parameter` |
|        - |  9025 | ` * $prefix` |
|        - |  9026 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9027 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9028 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9029 | ` * $more_entropy` |
|        - |  9030 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9031 | ` *  that the result will be unique.` |
|        - |  9032 | ` * Return` |
|        - |  9033 | ` *  Returns the unique identifier, as a string.` |
|        - |  9034 | ` */` |
|       24 |  9035 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9036 |  |
|        - |  9037 | `	struct unique_id_data sUniq;` |
|        - |  9038 | `	unsigned char zDigest[20];` |
|       25 |  9039 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9040 | `	const char *zPrefix;` |
|        - |  9041 | `	SHA1Context sCtx;` |
|        - |  9042 | `	char zRandom[7];` |
|        - |  9043 | `	int nPrefix;` |
|        - |  9044 | `	int entropy;` |
|        - |  9045 | `	/* Generate a random string first */` |
|       25 |  9046 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9047 | `	/* Initialize fields */` |
|       25 |  9048 | `	zPrefix = 0;` |
|       25 |  9049 | `	nPrefix = 0;` |
|       25 |  9050 | `	entropy = 0;` |
|       25 |  9051 | `	if( nArg > 0 ){` |
|        - |  9052 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9053 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9054 | `		if( nArg > 1 ){` |
|      ! 0 |  9055 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9056 | `		}` |
|      ! 0 |  9057 | `	}` |
|       25 |  9058 | `	SHA1Init(&sCtx);` |
|        - |  9059 | `	/* Generate the random ID */` |
|       25 |  9060 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9061 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9062 | `	}` |
|        - |  9063 | `	/* Append the random ID */` |
|       25 |  9064 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9065 | `	/* Append the random string */` |
|       25 |  9066 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9067 | `	/* Increment the number */` |
|       25 |  9068 | `	pVm->unique_id++;` |
|       25 |  9069 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9070 | `	/* Hexify the digest */` |
|       25 |  9071 | `	sUniq.pCtx = pCtx;` |
|       25 |  9072 | `	sUniq.entropy = entropy;` |
|       25 |  9073 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9074 | `	/* All done */` |
|       25 |  9075 | `	return PH7_OK;` |
|        1 |  9076 |  |
|        - |  9077 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9078 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9079 | `/*` |
|        - |  9080 | ` * Section:` |
|        - |  9081 | ` *  Language construct implementation as foreign functions.` |
|        - |  9082 | ` * Status:` |
|        - |  9083 | ` *    Stable.` |
|        - |  9084 | ` */` |
|        - |  9085 | `/*` |
|        - |  9086 | ` * void echo($string...)` |
|        - |  9087 | ` *  Output one or more messages.` |
|        - |  9088 | ` * Parameters` |
|        - |  9089 | ` *  $string` |
|        - |  9090 | ` *   Message to output.` |
|        - |  9091 | ` * Return` |
|        - |  9092 | ` *  NULL.` |
|        - |  9093 | ` */` |
|      ! 0 |  9094 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9095 |  |
|        - |  9096 | `	const char *zData;` |
|      ! 0 |  9097 | `	int nDataLen = 0;` |
|        - |  9098 | `	ph7_vm *pVm;` |
|        - |  9099 | `	int i,rc;` |
|        - |  9100 | `	/* Point to the target VM */` |
|      ! 0 |  9101 | `	pVm = pCtx->pVm;` |
|        - |  9102 | `	/* Output */` |
|      ! 0 |  9103 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9104 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9105 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9106 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9107 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9108 | `			if( rc == SXERR_ABORT ){` |
|        - |  9109 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9110 | `				return PH7_ABORT;` |
|        - |  9111 | `			}` |
|      ! 0 |  9112 | `		}` |
|      ! 0 |  9113 | `	}` |
|      ! 0 |  9114 | `	return SXRET_OK;` |
|      ! 0 |  9115 |  |
|        - |  9116 | `/*` |
|        - |  9117 | ` * int print($string...)` |
|        - |  9118 | ` *  Output one or more messages.` |
|        - |  9119 | ` * Parameters` |
|        - |  9120 | ` *  $string` |
|        - |  9121 | ` *   Message to output.` |
|        - |  9122 | ` * Return` |
|        - |  9123 | ` *  1 always.` |
|        - |  9124 | ` */` |
|        2 |  9125 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9126 |  |
|        - |  9127 | `	const char *zData;` |
|        3 |  9128 | `	int nDataLen = 0;` |
|        - |  9129 | `	ph7_vm *pVm;` |
|        - |  9130 | `	int i,rc;` |
|        - |  9131 | `	/* Point to the target VM */` |
|        3 |  9132 | `	pVm = pCtx->pVm;` |
|        - |  9133 | `	/* Output */` |
|        5 |  9134 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9135 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9136 | `		if( nDataLen > 0 ){` |
|        3 |  9137 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9138 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9139 | `			if( rc == SXERR_ABORT ){` |
|        - |  9140 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9141 | `				return PH7_ABORT;` |
|        - |  9142 | `			}` |
|        1 |  9143 | `		}` |
|        2 |  9144 | `	}` |
|        - |  9145 | `	/* Return 1 */` |
|        3 |  9146 | `	ph7_result_int(pCtx,1);` |
|        3 |  9147 | `	return SXRET_OK;` |
|        2 |  9148 |  |
|        - |  9149 | `/*` |
|        - |  9150 | ` * void exit(string $msg)` |
|        - |  9151 | ` * void exit(int $status)` |
|        - |  9152 | ` * void die(string $ms)` |
|        - |  9153 | ` * void die(int $status)` |
|        - |  9154 | ` *   Output a message and terminate program execution.` |
|        - |  9155 | ` * Parameter` |
|        - |  9156 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9157 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9158 | ` *  and not printed` |
|        - |  9159 | ` * Return` |
|        - |  9160 | ` *  NULL` |
|        - |  9161 | ` */` |
|      ! 0 |  9162 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9163 |  |
|      ! 0 |  9164 | `	if( nArg > 0 ){` |
|      ! 0 |  9165 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9166 | `			const char *zData;` |
|      ! 0 |  9167 | `			int iLen = 0;` |
|        - |  9168 | `			/* Print exit message */` |
|      ! 0 |  9169 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9170 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9171 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9172 | `			sxi32 iExitStatus;` |
|        - |  9173 | `			/* Record exit status code */` |
|      ! 0 |  9174 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9175 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9176 | `		}` |
|      ! 0 |  9177 | `	}` |
|        - |  9178 | `	/* Check if we are in an included file */` |
|      ! 0 |  9179 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9180 | `		/* Exit the entire process */` |
|      ! 0 |  9181 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9182 | `	}` |
|        - |  9183 | `	/* Abort processing immediately */` |
|      ! 0 |  9184 | `	return PH7_ABORT;` |
|      ! 0 |  9185 |  |
|        - |  9186 | `/*` |
|        - |  9187 | ` * bool isset($var,...)` |
|        - |  9188 | ` *  Finds out whether a variable is set.` |
|        - |  9189 | ` * Parameters` |
|        - |  9190 | ` *  One or more variable to check.` |
|        - |  9191 | ` * Return` |
|        - |  9192 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9193 | ` */` |
|    73222 |  9194 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9195 |  |
|        - |  9196 | `	ph7_value *pObj;` |
|    73224 |  9197 | `	int res = 0;` |
|        - |  9198 | `	int i;` |
|    73224 |  9199 | `	if( nArg < 1 ){` |
|        - |  9200 | `		/* Missing arguments,return false */` |
|      ! 0 |  9201 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9202 | `		return SXRET_OK;` |
|        - |  9203 | `	}` |
|        - |  9204 | `	/* Iterate over available arguments */` |
|    96630 |  9205 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    73224 |  9206 | `		pObj = apArg[i];` |
|    73224 |  9207 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    49304 |  9208 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9209 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9210 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9211 | `			}` |
|    24651 |  9212 | `		}` |
|    73224 |  9213 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    73224 |  9214 | `		if( !res ){` |
|        - |  9215 | `			/* Variable not set,return FALSE */` |
|    49818 |  9216 | `			ph7_result_bool(pCtx,0);` |
|    49818 |  9217 | `			return SXRET_OK;` |
|        - |  9218 | `		}` |
|    11705 |  9219 | `	}` |
|        - |  9220 | `	/* All given variable are set,return TRUE */` |
|    23408 |  9221 | `	ph7_result_bool(pCtx,1);` |
|    23408 |  9222 | `	return SXRET_OK;` |
|    36613 |  9223 |  |
|        - |  9224 | `/*` |
|        - |  9225 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9226 | ` * frame,the reference table and discard it's contents.` |
|        - |  9227 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9228 | ` */` |
|  2979010 |  9229 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9230 |  |
|        - |  9231 | `	ph7_value *pObj;` |
|        - |  9232 | `	VmRefObj *pRef;` |
|  2979012 |  9233 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2979012 |  9234 | `	if( pObj ){` |
|        - |  9235 | `		/* Release the object */` |
|  2979012 |  9236 | `		PH7_MemObjRelease(pObj);` |
|  1489505 |  9237 | `	}` |
|        - |  9238 | `	/* Remove old reference links */` |
|  2979012 |  9239 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2979012 |  9240 | `	if( pRef ){` |
|  2979006 |  9241 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9242 | `		/* Unlink from the reference table */` |
|  2979006 |  9243 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2979006 |  9244 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9245 | `			VmSlot sFree;` |
|        - |  9246 | `			/* Restore to the free list */` |
|  2979000 |  9247 | `			sFree.nIdx = nObjIdx;` |
|  2979000 |  9248 | `			sFree.pUserData = 0;` |
|  2979000 |  9249 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1489499 |  9250 | `		}` |
|  1489502 |  9251 | `	}` |
|  2979012 |  9252 | `	return SXRET_OK;` |
|        2 |  9253 |  |
|        - |  9254 | `/*` |
|        - |  9255 | ` * void unset($var,...)` |
|        - |  9256 | ` *   Unset one or more given variable.` |
|        - |  9257 | ` * Parameters` |
|        - |  9258 | ` *  One or more variable to unset.` |
|        - |  9259 | ` * Return` |
|        - |  9260 | ` *  Nothing.` |
|        - |  9261 | ` */` |
|     6678 |  9262 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9263 |  |
|        - |  9264 | `	ph7_value *pObj;` |
|        - |  9265 | `	ph7_vm *pVm;` |
|        - |  9266 | `	int i;` |
|        - |  9267 | `	/* Point to the target VM */` |
|     6680 |  9268 | `	pVm = pCtx->pVm;` |
|        - |  9269 | `	/* Iterate and unset */` |
|    13358 |  9270 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6680 |  9271 | `		pObj = apArg[i];` |
|     6680 |  9272 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9273 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9274 | `				/* Throw an error */` |
|      ! 0 |  9275 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9276 | `			}` |
|      ! 0 |  9277 | `		}else{` |
|     6680 |  9278 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9279 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6680 |  9280 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6674 |  9281 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3336 |  9282 | `			}` |
|        - |  9283 | `		}` |
|     3341 |  9284 | `	}` |
|     6680 |  9285 | `	return SXRET_OK;` |
|        2 |  9286 |  |
|        - |  9287 | `/*` |
|        - |  9288 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9289 | ` */` |
|      110 |  9290 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9291 |  |
|      111 |  9292 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9293 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9294 | `	ph7_value *pObj;` |
|        - |  9295 | `	sxu32 nIdx;` |
|        - |  9296 | `	/* Extract the memory object */` |
|      111 |  9297 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9298 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9299 | `	if( pObj ){` |
|      111 |  9300 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9301 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9302 | `				SyString sName;` |
|        - |  9303 | `				ph7_value sKey;` |
|        - |  9304 | `				/* Perform the insertion */` |
|      109 |  9305 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9306 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9307 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9308 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9309 | `			}` |
|       54 |  9310 | `		}` |
|       55 |  9311 | `	}` |
|      111 |  9312 | `	return SXRET_OK;` |
|        1 |  9313 |  |
|        - |  9314 | `/*` |
|        - |  9315 | ` * array get_defined_vars(void)` |
|        - |  9316 | ` *  Returns an array of all defined variables.` |
|        - |  9317 | ` * Parameter` |
|        - |  9318 | ` *  None` |
|        - |  9319 | ` * Return` |
|        - |  9320 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9321 | ` */` |
|        2 |  9322 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9323 |  |
|        3 |  9324 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9325 | `	ph7_value *pArray;` |
|        - |  9326 | `	/* Create a new array */` |
|        3 |  9327 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9328 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9329 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9330 | `		SXUNUSED(apArg);` |
|        - |  9331 | `		/* Return NULL */` |
|      ! 0 |  9332 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9333 | `		return SXRET_OK;` |
|        - |  9334 | `	}` |
|        - |  9335 | `	/* Superglobals first */` |
|        3 |  9336 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9337 | `	/* Then variable defined in the current frame */` |
|        3 |  9338 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9339 | `	/* Finally,return the created array */` |
|        3 |  9340 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9341 | `	return SXRET_OK;` |
|        2 |  9342 |  |
|        - |  9343 | `/*` |
|        - |  9344 | ` * bool gettype($var)` |
|        - |  9345 | ` *  Get the type of a variable` |
|        - |  9346 | ` * Parameters` |
|        - |  9347 | ` *   $var` |
|        - |  9348 | ` *    The variable being type checked.` |
|        - |  9349 | ` * Return` |
|        - |  9350 | ` *   String representation of the given variable type.` |
|        - |  9351 | ` */` |
|       32 |  9352 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9353 |  |
|       34 |  9354 | `	const char *zType = "Empty";` |
|       34 |  9355 | `	if( nArg > 0 ){` |
|       34 |  9356 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9357 | `	}` |
|        - |  9358 | `	/* Return the variable type */` |
|       34 |  9359 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9360 | `	return SXRET_OK;` |
|        2 |  9361 |  |
|        - |  9362 | `/*` |
|        - |  9363 | ` * string get_resource_type(resource $handle)` |
|        - |  9364 | ` *  This function gets the type of the given resource.` |
|        - |  9365 | ` * Parameters` |
|        - |  9366 | ` *  $handle` |
|        - |  9367 | ` *  The evaluated resource handle.` |
|        - |  9368 | ` * Return` |
|        - |  9369 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9370 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9371 | ` *  the return value will be the string Unknown.` |
|        - |  9372 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9373 | ` *  is not a resource.` |
|        - |  9374 | ` */` |
|        2 |  9375 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9376 |  |
|        3 |  9377 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9378 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9379 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9380 | `		return PH7_OK;` |
|        - |  9381 | `	}` |
|        3 |  9382 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9383 | `	return SXRET_OK;` |
|        2 |  9384 |  |
|        - |  9385 | `/*` |
|        - |  9386 | ` * void var_dump(expression,....)` |
|        - |  9387 | ` *   var_dump � Dumps information about a variable` |
|        - |  9388 | ` * Parameters` |
|        - |  9389 | ` *   One or more expression to dump.` |
|        - |  9390 | ` * Returns` |
|        - |  9391 | ` *  Nothing.` |
|        - |  9392 | ` */` |
|      218 |  9393 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9394 |  |
|        - |  9395 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9396 | `	int i;` |
|      220 |  9397 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9398 | `	/* Dump one or more expressions */` |
|      444 |  9399 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9400 | `		ph7_value *pObj = apArg[i];` |
|        - |  9401 | `		/* Reset the working buffer */` |
|      226 |  9402 | `		SyBlobReset(&sDump);` |
|        - |  9403 | `		/* Dump the given expression */` |
|      226 |  9404 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9405 | `		/* Output */` |
|      226 |  9406 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9407 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9408 | `		}` |
|      114 |  9409 | `	}` |
|        - |  9410 | `	/* Release the working buffer */` |
|      220 |  9411 | `	SyBlobRelease(&sDump);` |
|      220 |  9412 | `	return SXRET_OK;` |
|        2 |  9413 |  |
|        - |  9414 | `/*` |
|        - |  9415 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9416 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9417 | ` * Parameters` |
|        - |  9418 | ` *   expression: Expression to dump` |
|        - |  9419 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9420 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9421 | ` *            print_r() will return the information rather than print it.` |
|        - |  9422 | ` * Return` |
|        - |  9423 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9424 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9425 | ` */` |
|       16 |  9426 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9427 |  |
|       17 |  9428 | `	int ret_string = 0;` |
|        - |  9429 | `	SyBlob sDump;` |
|       17 |  9430 | `	if( nArg < 1 ){` |
|        - |  9431 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9432 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9433 | `		return SXRET_OK;` |
|        - |  9434 | `	}` |
|       17 |  9435 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9436 | `	if ( nArg > 1 ){` |
|        - |  9437 | `		/* Where to redirect output */` |
|       11 |  9438 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9439 | `	}` |
|        - |  9440 | `	/* Generate dump */` |
|       17 |  9441 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9442 | `	if( !ret_string ){` |
|        - |  9443 | `		/* Output dump */` |
|        7 |  9444 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9445 | `		/* Return true */` |
|        7 |  9446 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9447 | `	}else{` |
|        - |  9448 | `		/* Generated dump as return value */` |
|       11 |  9449 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9450 | `	}` |
|        - |  9451 | `	/* Release the working buffer */` |
|       17 |  9452 | `	SyBlobRelease(&sDump);` |
|       17 |  9453 | `	return SXRET_OK;` |
|        9 |  9454 |  |
|        - |  9455 | `/*` |
|        - |  9456 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9457 | ` * Same job as print_r. (see coment above)` |
|        - |  9458 | ` */` |
|        2 |  9459 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9460 |  |
|        3 |  9461 | `	int ret_string = 0;` |
|        - |  9462 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9463 | `	if( nArg < 1 ){` |
|        - |  9464 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9465 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9466 | `		return SXRET_OK;` |
|        - |  9467 | `	}` |
|        3 |  9468 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9469 | `	if ( nArg > 1 ){` |
|        - |  9470 | `		/* Where to redirect output */` |
|        3 |  9471 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9472 | `	}` |
|        - |  9473 | `	/* Generate dump */` |
|        3 |  9474 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9475 | `	if( !ret_string ){` |
|        - |  9476 | `		/* Output dump */` |
|      ! 0 |  9477 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9478 | `		/* Return NULL */` |
|      ! 0 |  9479 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9480 | `	}else{` |
|        - |  9481 | `		/* Generated dump as return value */` |
|        3 |  9482 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9483 | `	}` |
|        - |  9484 | `	/* Release the working buffer */` |
|        3 |  9485 | `	SyBlobRelease(&sDump);` |
|        3 |  9486 | `	return SXRET_OK;` |
|        2 |  9487 |  |
|        - |  9488 | `/*` |
|        - |  9489 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9490 | ` *  Set/get the various assert flags.` |
|        - |  9491 | ` * Parameter` |
|        - |  9492 | ` * $what` |
|        - |  9493 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9494 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9495 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9496 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9497 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9498 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9499 | ` * $value` |
|        - |  9500 | ` *   An optional new value for the option.` |
|        - |  9501 | ` * Return` |
|        - |  9502 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9503 | ` */` |
|       30 |  9504 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9505 |  |
|       32 |  9506 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9507 | `	int iOption;` |
|        - |  9508 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  9509 | `	if( nArg < 1 ){` |
|        3 |  9510 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9511 | `			"ArgumentCountError",` |
|        - |  9512 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9513 | `			);` |
|        - |  9514 | `	}` |
|        - |  9515 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  9516 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  9517 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9518 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9519 | `			"TypeError",` |
|        - |  9520 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9521 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9522 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9523 | `			);` |
|        - |  9524 | `	}` |
|       30 |  9525 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9526 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9527 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9528 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  9529 | `	switch( iOption ){` |
|        6 |  9530 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9531 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  9532 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  9533 | `		if( nArg > 1 ){` |
|        5 |  9534 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9535 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9536 | `			}else{` |
|        3 |  9537 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9538 | `			}` |
|        2 |  9539 | `		}` |
|       14 |  9540 | `		break;` |
|        1 |  9541 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9542 | `		/* Return old callback or null */` |
|        3 |  9543 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9544 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9545 | `		}else{` |
|        3 |  9546 | `			ph7_result_null(pCtx);` |
|        - |  9547 | `		}` |
|        3 |  9548 | `		if( nArg > 1 ){` |
|      ! 0 |  9549 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9550 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9551 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9552 | `			}else{` |
|      ! 0 |  9553 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9554 | `			}` |
|      ! 0 |  9555 | `		}` |
|        3 |  9556 | `		break;` |
|        5 |  9557 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9558 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9559 | `		if( nArg > 1 ){` |
|        5 |  9560 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9561 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9562 | `			}else{` |
|        3 |  9563 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9564 | `			}` |
|        2 |  9565 | `		}` |
|       11 |  9566 | `		break;` |
|      ! 0 |  9567 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9568 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9569 | `		break;` |
|        1 |  9570 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9571 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9572 | `		break;` |
|      ! 0 |  9573 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9574 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9575 | `		break;` |
|        1 |  9576 | `	default:` |
|        - |  9577 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9578 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9579 | `			"ValueError",` |
|        - |  9580 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9581 | `			);` |
|        - |  9582 | `	}` |
|       28 |  9583 | `	return PH7_OK;` |
|       17 |  9584 |  |
|        - |  9585 | `/*` |
|        - |  9586 | ` * bool assert(mixed $assertion)` |
|        - |  9587 | ` *  Checks if assertion is FALSE.` |
|        - |  9588 | ` * Parameter` |
|        - |  9589 | ` *  $assertion` |
|        - |  9590 | ` *    The assertion to test.` |
|        - |  9591 | ` * Return` |
|        - |  9592 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9593 | ` */` |
|       26 |  9594 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9595 |  |
|       28 |  9596 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9597 | `	int iFlags,iResult;` |
|        - |  9598 | `	const char *zDesc;` |
|        - |  9599 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  9600 | `	if( nArg < 1 ){` |
|        3 |  9601 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9602 | `			"ArgumentCountError",` |
|        - |  9603 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9604 | `			);` |
|        - |  9605 | `	}` |
|       26 |  9606 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  9607 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9608 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9609 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9610 | `		return PH7_OK;` |
|        - |  9611 | `	}` |
|        - |  9612 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  9613 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  9614 | `	if( !iResult ){` |
|        - |  9615 | `		/* Assertion failed */` |
|        - |  9616 | `		/* Extract optional description */` |
|       13 |  9617 | `		zDesc = 0;` |
|       13 |  9618 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9619 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9620 | `		}` |
|       13 |  9621 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9622 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9623 | `			ph7_value sFile,sLine;` |
|        - |  9624 | `			ph7_value *apCbArg[3];` |
|        - |  9625 | `			SyString *pFile;` |
|        - |  9626 | `			/* Extract the processed script */` |
|      ! 0 |  9627 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9628 | `			if( pFile == 0 ){` |
|      ! 0 |  9629 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9630 | `			}` |
|        - |  9631 | `			/* Invoke the callback */` |
|      ! 0 |  9632 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9633 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9634 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9635 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9636 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9637 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9638 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9639 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9640 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9641 | `		}` |
|       13 |  9642 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9643 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9644 | `			return PH7_ABORT;` |
|        - |  9645 | `		}` |
|        - |  9646 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9647 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9648 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9649 | `				"AssertionError",` |
|        - |  9650 | `				"%s",` |
|        1 |  9651 | `				zDesc` |
|        - |  9652 | `				);` |
|      ! 0 |  9653 | `		}else{` |
|       11 |  9654 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9655 | `				"AssertionError",` |
|        - |  9656 | `				"assert(false)"` |
|        - |  9657 | `				);` |
|        - |  9658 | `		}` |
|        - |  9659 | `	}` |
|        - |  9660 | `	/* Assertion passed */` |
|       14 |  9661 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9662 | `	return PH7_OK;` |
|       15 |  9663 |  |
|        - |  9664 | `/*` |
|        - |  9665 | ` * Section:` |
|        - |  9666 | ` *  Error reporting functions.` |
|        - |  9667 | ` * Status:` |
|        - |  9668 | ` *    Stable.` |
|        - |  9669 | ` */` |
|        - |  9670 | `/*` |
|        - |  9671 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9672 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9673 | ` * Parameters` |
|        - |  9674 | ` *  $error_msg` |
|        - |  9675 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9676 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9677 | ` * $error_type` |
|        - |  9678 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9679 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9680 | ` * Return` |
|        - |  9681 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9682 | ` */` |
|       12 |  9683 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9684 |  |
|       14 |  9685 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9686 | `	int rc = PH7_OK;` |
|       14 |  9687 | `	if( nArg > 0 ){` |
|        - |  9688 | `		const char *zErr;` |
|        - |  9689 | `		int nLen;` |
|        - |  9690 | `		/* Extract the error message */` |
|       12 |  9691 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9692 | `		if( nArg > 1 ){` |
|        - |  9693 | `			/* Extract the error type */` |
|       12 |  9694 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9695 | `			switch( nErr ){` |
|        1 |  9696 | `			case 1:   /* E_ERROR */` |
|        - |  9697 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9698 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9699 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9700 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9701 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9702 | `				break;` |
|        1 |  9703 | `			case 2:   /* E_WARNING */` |
|        - |  9704 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9705 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9706 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9707 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9708 | `				break;` |
|        3 |  9709 | `			default:` |
|        8 |  9710 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9711 | `				break;` |
|        - |  9712 | `			}` |
|        5 |  9713 | `		}` |
|        - |  9714 | `		/* Report error */` |
|       12 |  9715 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9716 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9717 | `			return rc;` |
|        - |  9718 | `		}` |
|        - |  9719 | `		/* Return true */` |
|       12 |  9720 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9721 | `	}else{` |
|        - |  9722 | `		/* Missing arguments,return FALSE */` |
|        3 |  9723 | `		ph7_result_bool(pCtx,0);` |
|        - |  9724 | `	}` |
|       14 |  9725 | `	return rc;` |
|        8 |  9726 |  |
|        - |  9727 | `/*` |
|        - |  9728 | ` * int error_reporting([int $level])` |
|        - |  9729 | ` *  Sets which PHP errors are reported.` |
|        - |  9730 | ` * Parameters` |
|        - |  9731 | ` *  $level` |
|        - |  9732 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9733 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9734 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9735 | ` *   levels will not always behave as expected.` |
|        - |  9736 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9737 | ` *   in the predefined constants.` |
|        - |  9738 | ` * Return` |
|        - |  9739 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9740 | ` *   parameter is given.` |
|        - |  9741 | ` */` |
|       42 |  9742 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9743 |  |
|       44 |  9744 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9745 | `	int nOld;` |
|        - |  9746 | `	/* Extract the old reporting level */` |
|       44 |  9747 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       44 |  9748 | `	if( nArg > 0 ){` |
|        - |  9749 | `		int nNew;` |
|        - |  9750 | `		/* Extract the desired error reporting level */` |
|       36 |  9751 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       36 |  9752 | `		if( !nNew ){` |
|        - |  9753 | `			/* Do not report errors at all */` |
|        5 |  9754 | `			pVm->bErrReport = 0;` |
|        3 |  9755 | `		}else{` |
|        - |  9756 | `			/* Report all errors */` |
|       32 |  9757 | `			pVm->bErrReport = 1;` |
|        - |  9758 | `		}` |
|       17 |  9759 | `	}` |
|        - |  9760 | `	/* Return the old level */` |
|       44 |  9761 | `	ph7_result_int(pCtx,nOld);` |
|       44 |  9762 | `	return PH7_OK;` |
|        2 |  9763 |  |
|        - |  9764 | `/*` |
|        - |  9765 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9766 | ` *  Send an error message somewhere.` |
|        - |  9767 | ` * Parameter` |
|        - |  9768 | ` *  $message` |
|        - |  9769 | ` *   The error message that should be logged.` |
|        - |  9770 | ` *  $message_type` |
|        - |  9771 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9772 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9773 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9774 | ` *       This is the default option.` |
|        - |  9775 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9776 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9777 | ` *    2  No longer an option.` |
|        - |  9778 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9779 | ` *       to the end of the message string.` |
|        - |  9780 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9781 | ` *  $destination` |
|        - |  9782 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9783 | ` *  $extra_headers` |
|        - |  9784 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9785 | ` * Return` |
|        - |  9786 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9787 | ` * NOTE:` |
|        - |  9788 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9789 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9790 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9791 | ` *  Otherwise this function is no-op.` |
|        - |  9792 | ` */` |
|        4 |  9793 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9794 |  |
|        - |  9795 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9796 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9797 | `	int iType = 0;` |
|        5 |  9798 | `	if( nArg < 1 ){` |
|        - |  9799 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9800 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9801 | `		return PH7_OK;` |
|        - |  9802 | `	}` |
|        5 |  9803 | `	if( pVm->xErrLog  ){` |
|        - |  9804 | `		/* Invoke the user callback */` |
|      ! 0 |  9805 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9806 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9807 | `		if( nArg > 1 ){` |
|      ! 0 |  9808 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9809 | `			if( nArg > 2 ){` |
|      ! 0 |  9810 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9811 | `				if( nArg > 3 ){` |
|      ! 0 |  9812 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9813 | `				}` |
|      ! 0 |  9814 | `			}` |
|      ! 0 |  9815 | `		}` |
|      ! 0 |  9816 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9817 | `	}` |
|        - |  9818 | `	/* Retun TRUE */` |
|        5 |  9819 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9820 | `	return PH7_OK;` |
|        3 |  9821 |  |
|        - |  9822 | `/*` |
|        - |  9823 | ` * bool restore_exception_handler(void)` |
|        - |  9824 | ` *  Restores the previously defined exception handler function.` |
|        - |  9825 | ` * Parameter` |
|        - |  9826 | ` *  None` |
|        - |  9827 | ` * Return` |
|        - |  9828 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9829 | ` */` |
|        4 |  9830 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9831 |  |
|        5 |  9832 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9833 | `	ph7_value *pOld,*pNew;` |
|        - |  9834 | `	/* Point to the old and the new handler */` |
|        5 |  9835 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9836 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9837 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9838 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9839 | `		SXUNUSED(apArg);` |
|        - |  9840 | `		/* No installed handler,return FALSE */` |
|        5 |  9841 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9842 | `		return PH7_OK;` |
|        - |  9843 | `	}` |
|        - |  9844 | `	/* Copy the old handler */` |
|      ! 0 |  9845 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9846 | `	PH7_MemObjRelease(pOld);` |
|        - |  9847 | `	/* Return TRUE */` |
|      ! 0 |  9848 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9849 | `	return PH7_OK;` |
|        3 |  9850 |  |
|        - |  9851 | `/*` |
|        - |  9852 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9853 | ` *  Sets a user-defined exception handler function.` |
|        - |  9854 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9855 | ` * NOTE` |
|        - |  9856 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9857 | ` *  the satndard PHP engine.` |
|        - |  9858 | ` * Parameters` |
|        - |  9859 | ` *  $exception_handler` |
|        - |  9860 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9861 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9862 | ` *   that was thrown.` |
|        - |  9863 | ` *  Note:` |
|        - |  9864 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9865 | ` * Return` |
|        - |  9866 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9867 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9868 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9869 | ` */` |
|        4 |  9870 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9871 |  |
|        6 |  9872 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9873 | `	ph7_value *pOld,*pNew;` |
|        - |  9874 | `	/* Point to the old and the new handler */` |
|        6 |  9875 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9876 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9877 | `	/* Return the old handler */` |
|        6 |  9878 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9879 | `	if( nArg > 0 ){` |
|        6 |  9880 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9881 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9882 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9883 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9884 | `		}else{` |
|        6 |  9885 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9886 | `			/* Install the new handler */` |
|        6 |  9887 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9888 | `		}` |
|        2 |  9889 | `	}` |
|        6 |  9890 | `	return PH7_OK;` |
|        2 |  9891 |  |
|        - |  9892 | `/*` |
|        - |  9893 | ` * bool restore_error_handler(void)` |
|        - |  9894 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9895 | ` * Parameters:` |
|        - |  9896 | ` *  None.` |
|        - |  9897 | ` * Return` |
|        - |  9898 | ` *  Always TRUE.` |
|        - |  9899 | ` */` |
|        4 |  9900 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9901 |  |
|        5 |  9902 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9903 | `	ph7_value *pOld,*pNew;` |
|        - |  9904 | `	/* Point to the old and the new handler */` |
|        5 |  9905 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9906 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9907 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9908 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9909 | `		SXUNUSED(apArg);` |
|        - |  9910 | `		/* No installed callback,return FALSE */` |
|        5 |  9911 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9912 | `		return PH7_OK;` |
|        - |  9913 | `	}` |
|        - |  9914 | `	/* Copy the old callback */` |
|      ! 0 |  9915 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9916 | `	PH7_MemObjRelease(pOld);` |
|        - |  9917 | `	/* Return TRUE */` |
|      ! 0 |  9918 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9919 | `	return PH7_OK;` |
|        3 |  9920 |  |
|        - |  9921 | `/*` |
|        - |  9922 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9923 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9924 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9925 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9926 | ` *  Sets a user-defined error handler function.` |
|        - |  9927 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9928 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9929 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9930 | ` *  conditions (using trigger_error()).` |
|        - |  9931 | ` * Parameters` |
|        - |  9932 | ` *  $error_handler` |
|        - |  9933 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9934 | ` *   describing the error.` |
|        - |  9935 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9936 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9937 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9938 | ` *   The function can be shown as:` |
|        - |  9939 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9940 | ` *     errno` |
|        - |  9941 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9942 | ` *   errstr` |
|        - |  9943 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9944 | ` *   errfile` |
|        - |  9945 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9946 | ` *     was raised in, as a string.` |
|        - |  9947 | ` *  Note:` |
|        - |  9948 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9949 | ` * Return` |
|        - |  9950 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9951 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9952 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9953 | ` */` |
|     8822 |  9954 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9955 |  |
|     8824 |  9956 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9957 | `	ph7_value *pOld,*pNew;` |
|        - |  9958 | `	/* Point to the old and the new handler */` |
|     8824 |  9959 | `	pOld = &pVm->aErrCB[0];` |
|     8824 |  9960 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9961 | `	/* Return the old handler */` |
|     8824 |  9962 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 |  9963 | `	if( nArg > 0 ){` |
|     8824 |  9964 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9965 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 |  9966 | `			PH7_MemObjRelease(pNew);` |
|     4411 |  9967 | `			ph7_result_bool(pCtx,1);` |
|     2206 |  9968 | `		}else{` |
|     4414 |  9969 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9970 | `			/* Install the new handler */` |
|     4414 |  9971 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9972 | `		}` |
|     4411 |  9973 | `	}` |
|     8824 |  9974 | `	return PH7_OK;` |
|        2 |  9975 |  |
|        - |  9976 | `/*` |
|        - |  9977 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9978 | ` *  Generates a backtrace.` |
|        - |  9979 | ` * Paramaeter` |
|        - |  9980 | ` *  $options` |
|        - |  9981 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9982 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9983 | ` *   all the function/method arguments, to save memory.` |
|        - |  9984 | ` * $limit` |
|        - |  9985 | ` *   (Not Used)` |
|        - |  9986 | ` * Return` |
|        - |  9987 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9988 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9989 | ` *          Name        Type      Description` |
|        - |  9990 | ` *          ------      ------     -----------` |
|        - |  9991 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9992 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9993 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9994 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9995 | ` *          object      object    The current object.` |
|        - |  9996 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9997 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9998 | ` */` |
|      510 |  9999 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10000 |  |
|      512 | 10001 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10002 | `	ph7_value *pArray;` |
|        - | 10003 | `	ph7_class *pClass;` |
|        - | 10004 | `	ph7_value *pValue;` |
|        - | 10005 | `	SyString *pFile;` |
|        - | 10006 | `	/* Create a new array */` |
|      512 | 10007 | `	pArray = ph7_context_new_array(pCtx);` |
|      512 | 10008 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      512 | 10009 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10010 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10011 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10012 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10013 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10014 | `		SXUNUSED(apArg);` |
|      ! 0 | 10015 | `		return PH7_OK;` |
|        - | 10016 | `	}` |
|        - | 10017 | `	/* Dump running function name and it's arguments  */` |
|      512 | 10018 | `	if( pVm->pFrame->pParent ){` |
|      512 | 10019 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10020 | `		ph7_vm_func *pFunc;` |
|        - | 10021 | `		ph7_value *pArg;` |
|      512 | 10022 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      512 | 10023 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      512 | 10024 | `		if( pFrame->pParent && pFunc ){` |
|      512 | 10025 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      512 | 10026 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      512 | 10027 | `			ph7_value_reset_string_cursor(pValue);` |
|      255 | 10028 | `		}` |
|        - | 10029 | `		/* Function arguments */` |
|      512 | 10030 | `		pArg = ph7_context_new_array(pCtx);` |
|      512 | 10031 | `		if( pArg  ){` |
|        - | 10032 | `			ph7_value *pObj;` |
|        - | 10033 | `			VmSlot *aSlot;` |
|        - | 10034 | `			sxu32 n;` |
|        - | 10035 | `			/* Start filling the array with the given arguments */` |
|      512 | 10036 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2034 | 10037 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1524 | 10038 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1524 | 10039 | `				if( pObj ){` |
|     1524 | 10040 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      761 | 10041 | `				}` |
|      763 | 10042 | `			}` |
|        - | 10043 | `			/* Save the array */` |
|      512 | 10044 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      255 | 10045 | `		}` |
|      255 | 10046 | `	}` |
|      512 | 10047 | `	ph7_value_int(pValue,1);` |
|        - | 10048 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10049 | `	 * line numbers at run-time. )` |
|        - | 10050 | `	 */` |
|      512 | 10051 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10052 | `	/* Current processed script */` |
|      512 | 10053 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      512 | 10054 | `	if( pFile ){` |
|      512 | 10055 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      512 | 10056 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      512 | 10057 | `		ph7_value_reset_string_cursor(pValue);` |
|      255 | 10058 | `	}` |
|        - | 10059 | `	/* Top class */` |
|      512 | 10060 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      512 | 10061 | `	if( pClass ){` |
|      508 | 10062 | `		ph7_value_reset_string_cursor(pValue);` |
|      508 | 10063 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      508 | 10064 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      253 | 10065 | `	}` |
|        - | 10066 | `	/* Return the freshly created array */` |
|      512 | 10067 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10068 | `	/*` |
|        - | 10069 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10070 | `	 * as soon we return from this function.` |
|        - | 10071 | `	 */` |
|      512 | 10072 | `	return PH7_OK;` |
|      257 | 10073 |  |
|        - | 10074 | `/*` |
|        - | 10075 | ` * Generate a small backtrace.` |
|        - | 10076 | ` * Store the generated dump in the given BLOB` |
|        - | 10077 | ` */` |
|        4 | 10078 | `static int VmMiniBacktrace(` |
|        - | 10079 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10080 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10081 | `	)` |
|        1 | 10082 |  |
|        5 | 10083 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10084 | `	ph7_vm_func *pFunc;` |
|        - | 10085 | `	ph7_class *pClass;` |
|        - | 10086 | `	SyString *pFile;` |
|        - | 10087 | `	/* Called function */` |
|        5 | 10088 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10089 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10090 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10091 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10092 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10093 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10094 | `	}else{` |
|      ! 0 | 10095 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10096 | `	}` |
|        5 | 10097 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10098 | `	/* Current processed script */` |
|        5 | 10099 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10100 | `	if( pFile ){` |
|        5 | 10101 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10102 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10103 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10104 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10105 | `	}` |
|        - | 10106 | `	/* Top class */` |
|        5 | 10107 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10108 | `	if( pClass ){` |
|      ! 0 | 10109 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10110 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10111 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10112 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10113 | `	}` |
|        5 | 10114 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10115 | `	/* All done */` |
|        5 | 10116 | `	return SXRET_OK;` |
|        1 | 10117 |  |
|        - | 10118 | `/*` |
|        - | 10119 | ` * void debug_print_backtrace()` |
|        - | 10120 | ` *  Prints a backtrace` |
|        - | 10121 | ` * Parameters` |
|        - | 10122 | ` * None` |
|        - | 10123 | ` * Return` |
|        - | 10124 | ` * NULL` |
|        - | 10125 | ` */` |
|        2 | 10126 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10127 |  |
|        3 | 10128 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10129 | `	SyBlob sDump;` |
|        3 | 10130 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10131 | `	/* Generate the backtrace */` |
|        3 | 10132 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10133 | `	/* Output backtrace */` |
|        3 | 10134 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10135 | `	/* All done,cleanup */` |
|        3 | 10136 | `	SyBlobRelease(&sDump);` |
|        1 | 10137 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10138 | `	SXUNUSED(apArg);` |
|        3 | 10139 | `	return PH7_OK;` |
|        1 | 10140 |  |
|        - | 10141 | `/*` |
|        - | 10142 | ` * string debug_string_backtrace()` |
|        - | 10143 | ` *  Generate a backtrace` |
|        - | 10144 | ` * Parameters` |
|        - | 10145 | ` * None` |
|        - | 10146 | ` * Return` |
|        - | 10147 | ` *  A mini backtrace().` |
|        - | 10148 | ` * Note that this is a symisc extension.` |
|        - | 10149 | ` */` |
|        2 | 10150 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10151 |  |
|        3 | 10152 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10153 | `	SyBlob sDump;` |
|        3 | 10154 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10155 | `	/* Generate the backtrace */` |
|        3 | 10156 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10157 | `	/* Return the backtrace */` |
|        3 | 10158 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10159 | `	/* All done,cleanup */` |
|        3 | 10160 | `	SyBlobRelease(&sDump);` |
|        1 | 10161 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10162 | `	SXUNUSED(apArg);` |
|        3 | 10163 | `	return PH7_OK;` |
|        1 | 10164 |  |
|        - | 10165 | `/*` |
|        - | 10166 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10167 | ` * exception is triggered.` |
|        - | 10168 | ` */` |
|      472 | 10169 | `static sxi32 VmUncaughtException(` |
|        - | 10170 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10171 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10172 | `	)` |
|        1 | 10173 |  |
|        - | 10174 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10175 | `	int nArg = 1;` |
|        - | 10176 | `	sxi32 rc;` |
|      473 | 10177 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10178 | `		/* Nesting limit reached */` |
|      ! 0 | 10179 | `		return SXRET_OK;` |
|        - | 10180 | `	}` |
|        - | 10181 | `	/* Call any exception handler if available */` |
|      473 | 10182 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10183 | `	if( pThis ){` |
|        - | 10184 | `		/* Load the exception instance */` |
|      473 | 10185 | `		sArg.x.pOther = pThis;` |
|      473 | 10186 | `		pThis->iRef++;` |
|      473 | 10187 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10188 | `	}else{` |
|      ! 0 | 10189 | `		nArg = 0;` |
|        - | 10190 | `	}` |
|      473 | 10191 | `	apArg[0] = &sArg;` |
|        - | 10192 | `	/* Call the exception handler if available */` |
|      473 | 10193 | `	pVm->nExceptDepth++;` |
|      473 | 10194 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10195 | `	pVm->nExceptDepth--;` |
|      473 | 10196 | `	if( rc != SXRET_OK ){` |
|        - | 10197 | `		SyBlob sMsgBuf;` |
|      471 | 10198 | `		const char *zClass = "Exception";` |
|      471 | 10199 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10200 | `		const char *zMsg;` |
|        - | 10201 | `		sxu32 nMsg;` |
|        - | 10202 | `		const char *zFuncName;` |
|        - | 10203 | `		int nFuncLen;` |
|      471 | 10204 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10205 | `		if( pThis ){` |
|        - | 10206 | `			ph7_class_method *pGetMessage;` |
|        - | 10207 | `			ph7_value sMsg;` |
|        - | 10208 | `			const char *zTmp;` |
|        - | 10209 | `			int nTmp;` |
|      471 | 10210 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10211 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10212 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10213 | `			if( pGetMessage ){` |
|      471 | 10214 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10215 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10216 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10217 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10218 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10219 | `					}` |
|      235 | 10220 | `				}` |
|      471 | 10221 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10222 | `			}` |
|      235 | 10223 | `		}` |
|      471 | 10224 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10225 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10226 | `		}` |
|      471 | 10227 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10228 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10229 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10230 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10231 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10232 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10233 | `		rc = SXERR_ABORT;` |
|      235 | 10234 | `	}` |
|      473 | 10235 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10236 | `	return rc;` |
|      237 | 10237 |  |
|        - | 10238 | `/*` |
|        - | 10239 | ` * Throw a user exception.` |
|        - | 10240 | ` *` |
|        - | 10241 | ` * Exception dispatch follows this sequence:` |
|        - | 10242 | ` *` |
|        - | 10243 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10244 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10245 | ` *` |
|        - | 10246 | ` * 2. If NO catch matches:` |
|        - | 10247 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10248 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10249 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10250 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10251 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10252 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10253 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10254 | ` *` |
|        - | 10255 | ` * 3. If a catch DOES match:` |
|        - | 10256 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10257 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10258 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10259 | ` *       finally block.` |
|        - | 10260 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10261 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10262 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10263 | ` *       in pPendingException (step 2c).` |
|        - | 10264 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10265 | ` *    d. Run finally (if present).` |
|        - | 10266 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10267 | ` *       that handlers are restored and finally has run.` |
|        - | 10268 | ` */` |
|      514 | 10269 | `static sxi32 VmThrowException(` |
|        - | 10270 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10271 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10272 | `	)` |
|        2 | 10273 |  |
|        - | 10274 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10275 | `	ph7_exception **apException;` |
|        - | 10276 | `	ph7_exception *pException;` |
|        - | 10277 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10278 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10279 | `	pException = 0;` |
|      516 | 10280 | `	pCatch = 0;` |
|      516 | 10281 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10282 | `		ph7_exception_block *aCatch;` |
|        - | 10283 | `		ph7_class *pClass;` |
|        - | 10284 | `		sxu32 j;` |
|        - | 10285 | `		/* Locate the appropriate block to execute */` |
|       40 | 10286 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10287 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10288 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10289 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10290 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10291 | `			/* Extract the target class */` |
|       38 | 10292 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10293 | `			if( pClass == 0 ){` |
|        - | 10294 | `				/* No such class */` |
|      ! 0 | 10295 | `				continue;` |
|        - | 10296 | `			}` |
|       38 | 10297 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10298 | `				/* Catch block found,break immeditaley */` |
|       38 | 10299 | `				pCatch = &aCatch[j];` |
|       38 | 10300 | `				break;` |
|        - | 10301 | `			}` |
|      ! 0 | 10302 | `		}` |
|       19 | 10303 | `	}` |
|        - | 10304 | `	/* Execute the cached block if available */` |
|      516 | 10305 | `	if( pCatch == 0 ){` |
|        - | 10306 | `		sxi32 rc;` |
|        - | 10307 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10308 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10309 | `			pException->iFinallyDone = 1;` |
|        3 | 10310 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10311 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10312 | `				return SXERR_ABORT;` |
|        - | 10313 | `			}` |
|        1 | 10314 | `		}` |
|        - | 10315 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10316 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10317 | `			/* Re-throw to the outer handler */` |
|        3 | 10318 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10319 | `		}` |
|        - | 10320 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10321 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10322 | `		 * exception instead of reporting it uncaught.` |
|        - | 10323 | `		 */` |
|      478 | 10324 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10325 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10326 | `			 * by looking for a catch frame on the stack.` |
|        - | 10327 | `			 */` |
|      478 | 10328 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10329 | `			int inCatch = 0;` |
|      956 | 10330 | `			while( pF ){` |
|      484 | 10331 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 | 10332 | `					inCatch = 1;` |
|        6 | 10333 | `					break;` |
|        - | 10334 | `				}` |
|      479 | 10335 | `				pF = pF->pParent;` |
|        1 | 10336 | `			}` |
|      478 | 10337 | `			if( inCatch ){` |
|        - | 10338 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 | 10339 | `				pThis->iRef++;` |
|        6 | 10340 | `				pVm->pPendingException = pThis;` |
|        6 | 10341 | `				return SXRET_OK;` |
|        - | 10342 | `			}` |
|      236 | 10343 | `		}` |
|        - | 10344 | `		/* Truly uncaught */` |
|      473 | 10345 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10346 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10347 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10348 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10349 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10350 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10351 | `			}` |
|      ! 0 | 10352 | `		}` |
|      473 | 10353 | `		return rc;` |
|      ! 0 | 10354 | `	}else{` |
|       38 | 10355 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10356 | `		ph7_exception **apSaved = 0;` |
|        - | 10357 | `		sxu32 nSavedCount;` |
|        - | 10358 | `		sxi32 rc;` |
|       38 | 10359 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10360 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10361 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10362 | `		}` |
|        - | 10363 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10364 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10365 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10366 | `		 */` |
|       38 | 10367 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10368 | `		if( nSavedCount > 0 ){` |
|       11 | 10369 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10370 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10371 | `			if( apSaved ){` |
|       11 | 10372 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10373 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10374 | `				SySetReset(&pVm->aException);` |
|        3 | 10375 | `			}` |
|        3 | 10376 | `		}` |
|        - | 10377 | `		/* Create a private frame first */` |
|       38 | 10378 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10379 | `		if( rc == SXRET_OK ){` |
|       38 | 10380 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10381 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10382 | `			if( pObj ){` |
|       38 | 10383 | `				pThis->iRef++;` |
|       38 | 10384 | `				pObj->x.pOther = pThis;` |
|       38 | 10385 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10386 | `			}` |
|        - | 10387 | `			/* Execute the catch block */` |
|       38 | 10388 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10389 | `			/* Leave the frame */` |
|       38 | 10390 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10391 | `		}` |
|        - | 10392 | `		/* Restore the outer exception handlers */` |
|       38 | 10393 | `		if( apSaved ){` |
|        - | 10394 | `			sxu32 k;` |
|        - | 10395 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10396 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10397 | `			 * Restore the original outer entries.` |
|        - | 10398 | `			 */` |
|        8 | 10399 | `			SySetReset(&pVm->aException);` |
|       14 | 10400 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 | 10401 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10402 | `			}` |
|        8 | 10403 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10404 | `		}` |
|        - | 10405 | `		/* Execute the finally block after catch */` |
|       38 | 10406 | `		if( pException->iHasFinally ){` |
|       11 | 10407 | `			pException->iFinallyDone = 1;` |
|        - | 10408 | `			{` |
|       11 | 10409 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 | 10410 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10411 | `					return SXERR_ABORT;` |
|        - | 10412 | `				}` |
|        - | 10413 | `			}` |
|        5 | 10414 | `		}` |
|       38 | 10415 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10416 | `			return SXERR_ABORT;` |
|        - | 10417 | `		}` |
|        - | 10418 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10419 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10420 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10421 | `		 */` |
|       38 | 10422 | `		if( pVm->pPendingException ){` |
|        6 | 10423 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 | 10424 | `			pVm->pPendingException = 0;` |
|        6 | 10425 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10426 | `		}` |
|        - | 10427 | `	}` |
|        - | 10428 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10429 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10430 | `	 */` |
|       34 | 10431 | `	return SXRET_OK;` |
|      259 | 10432 |  |
|        - | 10433 | `/*` |
|        - | 10434 | ` * Section:` |
|        - | 10435 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10436 | ` * Status:` |
|        - | 10437 | ` *    Stable.` |
|        - | 10438 | ` */` |
|        - | 10439 | `/*` |
|        - | 10440 | ` * string ph7version(void)` |
|        - | 10441 | ` *  Returns the running version of the PH7 version.` |
|        - | 10442 | ` * Parameters` |
|        - | 10443 | ` *  None` |
|        - | 10444 | ` * Return` |
|        - | 10445 | ` * Current PH7 version.` |
|        - | 10446 | ` */` |
|        2 | 10447 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10448 |  |
|        1 | 10449 | `	SXUNUSED(nArg);` |
|        1 | 10450 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10451 | `	/* Current engine version */` |
|        3 | 10452 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10453 | `	return PH7_OK;` |
|        1 | 10454 |  |
|        - | 10455 | `/*` |
|        - | 10456 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10457 | ` */` |
|        - | 10458 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10459 | ` "<html><head>"\` |
|        - | 10460 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10461 | ` "<style type=\"text/css\">"\` |
|        - | 10462 | ` "div {"\` |
|        - | 10463 | `     "border: 1px solid #cccccc;"\` |
|        - | 10464 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10465 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10466 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10467 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10468 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10469 | `     "-o-border-radius: 10px;"\` |
|        - | 10470 | `     "border-radius: 10px;"\` |
|        - | 10471 | `     "padding-left: 2em;"\` |
|        - | 10472 | `     "background-color: white;"\` |
|        - | 10473 | `     "margin-left: auto;"\` |
|        - | 10474 | `     "font-family: verdana;"\` |
|        - | 10475 | `     "padding-right: 2em;"\` |
|        - | 10476 | `     "margin-right: auto;"\` |
|        - | 10477 | `     "}"\` |
|        - | 10478 | `     "body {"\` |
|        - | 10479 | `     "padding: 0.2em;"\` |
|        - | 10480 | `     "font-style: normal;"\` |
|        - | 10481 | `     "font-size: medium;"\` |
|        - | 10482 | `     "background-color: #f2f2f2;"\` |
|        - | 10483 | `     "}"\` |
|        - | 10484 | `     "hr {"\` |
|        - | 10485 | `     "border-style: solid none none;"\` |
|        - | 10486 | `     "border-width: 1px medium medium;"\` |
|        - | 10487 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10488 | `     "height: 1px;"\` |
|        - | 10489 | `     "}"\` |
|        - | 10490 | `     "a {"\` |
|        - | 10491 | `     "color: #3366cc;"\` |
|        - | 10492 | `     "text-decoration: none;"\` |
|        - | 10493 | `     "}"\` |
|        - | 10494 | `     "a:hover {"\` |
|        - | 10495 | `     "color: #999999;"\` |
|        - | 10496 | `     "}"\` |
|        - | 10497 | `     "a:active {"\` |
|        - | 10498 | `     "color: #663399;"\` |
|        - | 10499 | `     "}"\` |
|        - | 10500 | `     "h1 {"\` |
|        - | 10501 | `     "margin: 0;"\` |
|        - | 10502 | `     "padding: 0;"\` |
|        - | 10503 | `     "font-family: Verdana;"\` |
|        - | 10504 | `     "font-weight: bold;"\` |
|        - | 10505 | `     "font-style: normal;"\` |
|        - | 10506 | `     "font-size: medium;"\` |
|        - | 10507 | `     "text-transform: capitalize;"\` |
|        - | 10508 | `     "color: #0a328c;"\` |
|        - | 10509 | `     "}"\` |
|        - | 10510 | `     "p {"\` |
|        - | 10511 | `     "margin: 0 auto;"\` |
|        - | 10512 | `     "font-size: medium;"\` |
|        - | 10513 | `     "font-style: normal;"\` |
|        - | 10514 | `     "font-family: verdana;"\` |
|        - | 10515 | `     "}"\` |
|        - | 10516 | `"</style></head><body>"\` |
|        - | 10517 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10518 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10519 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10520 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10521 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10522 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10523 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10524 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10525 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10526 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10527 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10528 |  |
|        - | 10529 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10530 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10531 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10532 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10533 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10534 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10535 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10536 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10537 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10538 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10539 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10540 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10541 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10542 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10543 |  |
|        - | 10544 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10545 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10546 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10547 | `"&nbsp;*<br>"\` |
|        - | 10548 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10549 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10550 | `"&nbsp;* are met:<br>"\` |
|        - | 10551 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10552 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10553 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10554 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10555 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10556 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10557 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10558 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10559 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10560 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10561 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10562 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10563 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10564 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10565 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10566 | `"&nbsp;*<br>"\` |
|        - | 10567 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10568 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10569 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10570 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10571 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10572 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10573 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10574 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10575 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10576 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10577 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10578 | `"&nbsp;*/<br>"\` |
|        - | 10579 | `"</span></small></small></p>"\` |
|        - | 10580 | `"</div></body></html>"` |
|        - | 10581 | `/*` |
|        - | 10582 | ` * bool ph7credits(void)` |
|        - | 10583 | ` * bool ph7info(void)` |
|        - | 10584 | ` * bool ph7copyright(void)` |
|        - | 10585 | ` *  Prints out the credits for PH7 engine` |
|        - | 10586 | ` * Parameters` |
|        - | 10587 | ` *  None` |
|        - | 10588 | ` * Return` |
|        - | 10589 | ` *  Always TRUE` |
|        - | 10590 | ` */` |
|        2 | 10591 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10592 |  |
|        3 | 10593 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10594 | `	/* Expand the HTML page above*/` |
|        3 | 10595 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10596 | `	ph7_context_output_format(` |
|        1 | 10597 | `		pCtx,` |
|        - | 10598 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10599 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10600 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10601 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10602 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10603 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10604 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10605 | `#ifdef __WINNT__` |
|        - | 10606 | `		"Windows NT"` |
|        - | 10607 | `#elif defined(__UNIXES__)` |
|        - | 10608 | `		"UNIX-Like"` |
|        - | 10609 | `#else` |
|        - | 10610 | `		"Other OS"` |
|        - | 10611 | `#endif` |
|        - | 10612 | `		);` |
|        3 | 10613 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10614 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10615 | `	SXUNUSED(apArg);` |
|        - | 10616 | `	/* Return TRUE */` |
|        - | 10617 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10618 | `	return PH7_OK;` |
|        1 | 10619 |  |
|        - | 10620 | `/*` |
|        - | 10621 | ` * Section:` |
|        - | 10622 | ` *    URL related routines.` |
|        - | 10623 | ` * Status:` |
|        - | 10624 | ` *    Stable.` |
|        - | 10625 | ` */` |
|        - | 10626 | `/*` |
|        - | 10627 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10628 | ` *  Parse a URL and return its fields.` |
|        - | 10629 | ` * Parameters` |
|        - | 10630 | ` *  $url` |
|        - | 10631 | ` *   The URL to parse.` |
|        - | 10632 | ` * $component` |
|        - | 10633 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10634 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10635 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10636 | ` *  in which case the return value will be an integer).` |
|        - | 10637 | ` * Return` |
|        - | 10638 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10639 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10640 | ` *  this array are:` |
|        - | 10641 | ` *   scheme - e.g. http` |
|        - | 10642 | ` *   host` |
|        - | 10643 | ` *   port` |
|        - | 10644 | ` *   user` |
|        - | 10645 | ` *   pass` |
|        - | 10646 | ` *   path` |
|        - | 10647 | ` *   query - after the question mark ?` |
|        - | 10648 | ` *   fragment - after the hashmark #` |
|        - | 10649 | ` * Note:` |
|        - | 10650 | ` *  FALSE is returned on failure.` |
|        - | 10651 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10652 | ` *  with the standard PHP engine.` |
|        - | 10653 | ` */` |
|       28 | 10654 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10655 |  |
|        - | 10656 | `	const char *zStr; /* Input string */` |
|        - | 10657 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10658 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10659 | `	int nLen;` |
|        - | 10660 | `	sxi32 rc;` |
|       29 | 10661 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10662 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10663 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10664 | `		return PH7_OK;` |
|        - | 10665 | `	}` |
|        - | 10666 | `	/* Extract the given URI */` |
|       29 | 10667 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10668 | `	if( nLen < 1 ){` |
|        - | 10669 | `		/* Nothing to process,return FALSE */` |
|        3 | 10670 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10671 | `		return PH7_OK;` |
|        - | 10672 | `	}` |
|        - | 10673 | `	/* Get a parse */` |
|       27 | 10674 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10675 | `	if( rc != SXRET_OK ){` |
|        - | 10676 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10677 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10678 | `		return PH7_OK;` |
|        - | 10679 | `	}` |
|       27 | 10680 | `	if( nArg > 1 ){` |
|      ! 0 | 10681 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10682 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10683 | `		switch(nComponent){` |
|      ! 0 | 10684 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10685 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10686 | `			if( pComp->nByte < 1 ){` |
|        - | 10687 | `				/* No available value,return NULL */` |
|      ! 0 | 10688 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10689 | `			}else{` |
|      ! 0 | 10690 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10691 | `			}` |
|      ! 0 | 10692 | `			break;` |
|      ! 0 | 10693 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10694 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10695 | `			if( pComp->nByte < 1 ){` |
|        - | 10696 | `				/* No available value,return NULL */` |
|      ! 0 | 10697 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10698 | `			}else{` |
|      ! 0 | 10699 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10700 | `			}` |
|      ! 0 | 10701 | `			break;` |
|      ! 0 | 10702 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10703 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10704 | `			if( pComp->nByte < 1 ){` |
|        - | 10705 | `				/* No available value,return NULL */` |
|      ! 0 | 10706 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10707 | `			}else{` |
|      ! 0 | 10708 | `				int iPort = 0;` |
|        - | 10709 | `				/* Cast the value to integer */` |
|      ! 0 | 10710 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10711 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10712 | `			}` |
|      ! 0 | 10713 | `			break;` |
|      ! 0 | 10714 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10715 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10716 | `			if( pComp->nByte < 1 ){` |
|        - | 10717 | `				/* No available value,return NULL */` |
|      ! 0 | 10718 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10719 | `			}else{` |
|      ! 0 | 10720 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10721 | `			}` |
|      ! 0 | 10722 | `			break;` |
|      ! 0 | 10723 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10724 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10725 | `			if( pComp->nByte < 1 ){` |
|        - | 10726 | `				/* No available value,return NULL */` |
|      ! 0 | 10727 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10728 | `			}else{` |
|      ! 0 | 10729 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10730 | `			}` |
|      ! 0 | 10731 | `			break;` |
|      ! 0 | 10732 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10733 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10734 | `			if( pComp->nByte < 1 ){` |
|        - | 10735 | `				/* No available value,return NULL */` |
|      ! 0 | 10736 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10737 | `			}else{` |
|      ! 0 | 10738 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10739 | `			}` |
|      ! 0 | 10740 | `			break;` |
|      ! 0 | 10741 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10742 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10743 | `			if( pComp->nByte < 1 ){` |
|        - | 10744 | `				/* No available value,return NULL */` |
|      ! 0 | 10745 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10746 | `			}else{` |
|      ! 0 | 10747 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10748 | `			}` |
|      ! 0 | 10749 | `			break;` |
|      ! 0 | 10750 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10751 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10752 | `			if( pComp->nByte < 1 ){` |
|        - | 10753 | `				/* No available value,return NULL */` |
|      ! 0 | 10754 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10755 | `			}else{` |
|      ! 0 | 10756 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10757 | `			}` |
|      ! 0 | 10758 | `			break;` |
|      ! 0 | 10759 | `		default:` |
|        - | 10760 | `			/* No such entry,return NULL */` |
|      ! 0 | 10761 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10762 | `			break;` |
|        - | 10763 | `		}` |
|      ! 0 | 10764 | `	}else{` |
|        - | 10765 | `		ph7_value *pArray,*pValue;` |
|        - | 10766 | `		/* Return an associative array */` |
|       27 | 10767 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10768 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10769 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10770 | `			/* Out of memory */` |
|      ! 0 | 10771 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10772 | `			/* Return false */` |
|      ! 0 | 10773 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10774 | `			return PH7_OK;` |
|        - | 10775 | `		}` |
|        - | 10776 | `		/* Fill the array */` |
|       27 | 10777 | `		pComp = &sURI.sScheme;` |
|       27 | 10778 | `		if( pComp->nByte > 0 ){` |
|       19 | 10779 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10780 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10781 | `		}` |
|        - | 10782 | `		/* Reset the string cursor */` |
|       27 | 10783 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10784 | `		pComp = &sURI.sHost;` |
|       27 | 10785 | `		if( pComp->nByte > 0 ){` |
|       25 | 10786 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10787 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10788 | `		}` |
|        - | 10789 | `		/* Reset the string cursor */` |
|       27 | 10790 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10791 | `		pComp = &sURI.sPort;` |
|       27 | 10792 | `		if( pComp->nByte > 0 ){` |
|       11 | 10793 | `			int iPort = 0;/* cc warning */` |
|        - | 10794 | `			/* Convert to integer */` |
|       11 | 10795 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10796 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10797 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10798 | `		}` |
|        - | 10799 | `		/* Reset the string cursor */` |
|       27 | 10800 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10801 | `		pComp = &sURI.sUser;` |
|       27 | 10802 | `		if( pComp->nByte > 0 ){` |
|        7 | 10803 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10804 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10805 | `		}` |
|        - | 10806 | `		/* Reset the string cursor */` |
|       27 | 10807 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10808 | `		pComp = &sURI.sPass;` |
|       27 | 10809 | `		if( pComp->nByte > 0 ){` |
|        7 | 10810 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10811 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10812 | `		}` |
|        - | 10813 | `		/* Reset the string cursor */` |
|       27 | 10814 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10815 | `		pComp = &sURI.sPath;` |
|       27 | 10816 | `		if( pComp->nByte > 0 ){` |
|       17 | 10817 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10818 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10819 | `		}` |
|        - | 10820 | `		/* Reset the string cursor */` |
|       27 | 10821 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10822 | `		pComp = &sURI.sQuery;` |
|       27 | 10823 | `		if( pComp->nByte > 0 ){` |
|        5 | 10824 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10825 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10826 | `		}` |
|        - | 10827 | `		/* Reset the string cursor */` |
|       27 | 10828 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10829 | `		pComp = &sURI.sFragment;` |
|       27 | 10830 | `		if( pComp->nByte > 0 ){` |
|        5 | 10831 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10832 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10833 | `		}` |
|        - | 10834 | `		/* Return the created array */` |
|       27 | 10835 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10836 | `		/* NOTE:` |
|        - | 10837 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10838 | `		 * automatically as soon we return from this function.` |
|        - | 10839 | `		 */` |
|        - | 10840 | `	}` |
|        - | 10841 | `	/* All done */` |
|       27 | 10842 | `	return PH7_OK;` |
|       15 | 10843 |  |
|        - | 10844 | `/*` |
|        - | 10845 | ` * Section:` |
|        - | 10846 | ` *   Array related routines.` |
|        - | 10847 | ` * Status:` |
|        - | 10848 | ` *    Stable.` |
|        - | 10849 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10850 | ` *  Array related functions that need access to the underlying` |
|        - | 10851 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10852 | ` */` |
|        - | 10853 | `/*` |
|        - | 10854 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10855 | ` * of the following structure.` |
|        - | 10856 | ` */` |
|        - | 10857 | `struct compact_data` |
|        - | 10858 |  |
|        - | 10859 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10860 | `	int nRecCount;      /* Recursion count */` |
|        - | 10861 | `};` |
|        - | 10862 | `/*` |
|        - | 10863 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10864 | ` */` |
|      ! 0 | 10865 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10866 |  |
|      ! 0 | 10867 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10868 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10869 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10870 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10871 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10872 | `		SyString sVar;` |
|      ! 0 | 10873 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10874 | `		if( sVar.nByte > 0 ){` |
|        - | 10875 | `			/* Query the current frame */` |
|      ! 0 | 10876 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10877 | `			/* ^` |
|        - | 10878 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10879 | `			 */` |
|      ! 0 | 10880 | `			if( pKey ){` |
|        - | 10881 | `				/* Perform the insertion */` |
|      ! 0 | 10882 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10883 | `			}` |
|      ! 0 | 10884 | `		}` |
|      ! 0 | 10885 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10886 | `		int rc;` |
|        - | 10887 | `		/* Recursively traverse this array */` |
|      ! 0 | 10888 | `		pData->nRecCount++;` |
|      ! 0 | 10889 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10890 | `		pData->nRecCount--;` |
|      ! 0 | 10891 | `		return rc;` |
|        - | 10892 | `	}` |
|      ! 0 | 10893 | `	return SXRET_OK;` |
|      ! 0 | 10894 |  |
|        - | 10895 | `/*` |
|        - | 10896 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10897 | ` *  Create array containing variables and their values.` |
|        - | 10898 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10899 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10900 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10901 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10902 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10903 | ` * Parameters` |
|        - | 10904 | ` *  $varname` |
|        - | 10905 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10906 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10907 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10908 | ` *   it recursively.` |
|        - | 10909 | ` * Return` |
|        - | 10910 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10911 | ` */` |
|        2 | 10912 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10913 |  |
|        - | 10914 | `	ph7_value *pArray,*pObj;` |
|        3 | 10915 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10916 | `	const char *zName;` |
|        - | 10917 | `	SyString sVar;` |
|        - | 10918 | `	int i,nLen;` |
|        3 | 10919 | `	if( nArg < 1 ){` |
|        - | 10920 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10921 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10922 | `		return PH7_OK;` |
|        - | 10923 | `	}` |
|        - | 10924 | `	/* Create the array */` |
|        3 | 10925 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10926 | `	if( pArray == 0 ){` |
|        - | 10927 | `		/* Out of memory */` |
|      ! 0 | 10928 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10929 | `		/* Return NULL */` |
|      ! 0 | 10930 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10931 | `		return PH7_OK;` |
|        - | 10932 | `	}` |
|        - | 10933 | `	/* Perform the requested operation */` |
|        7 | 10934 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10935 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10936 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10937 | `				struct compact_data sData;` |
|      ! 0 | 10938 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10939 | `				/* Recursively walk the array */` |
|      ! 0 | 10940 | `				sData.nRecCount = 0;` |
|      ! 0 | 10941 | `				sData.pArray = pArray;` |
|      ! 0 | 10942 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10943 | `			}` |
|      ! 0 | 10944 | `		}else{` |
|        - | 10945 | `			/* Extract variable name */` |
|        5 | 10946 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10947 | `			if( nLen > 0 ){` |
|        5 | 10948 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10949 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10950 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10951 | `				if( pObj ){` |
|        5 | 10952 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10953 | `				}` |
|        2 | 10954 | `			}` |
|        - | 10955 | `		}` |
|        3 | 10956 | `	}` |
|        - | 10957 | `	/* Return the array */` |
|        3 | 10958 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10959 | `	return PH7_OK;` |
|        2 | 10960 |  |
|        - | 10961 | `/*` |
|        - | 10962 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10963 | ` * of the following structure.` |
|        - | 10964 | ` */` |
|        - | 10965 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10966 | `struct extract_aux_data` |
|        - | 10967 |  |
|        - | 10968 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10969 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10970 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10971 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10972 | `	int iFlags;           /* Control flags */` |
|        - | 10973 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10974 | `};` |
|        - | 10975 | `/* Forward declaration */` |
|        - | 10976 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10977 | `/*` |
|        - | 10978 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10979 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10980 | ` * Parameters` |
|        - | 10981 | ` * $var_array` |
|        - | 10982 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10983 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10984 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10985 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10986 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10987 | ` * $extract_type` |
|        - | 10988 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10989 | ` *  It can be one of the following values:` |
|        - | 10990 | ` *   EXTR_OVERWRITE` |
|        - | 10991 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10992 | ` *   EXTR_SKIP` |
|        - | 10993 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10994 | ` *   EXTR_PREFIX_SAME` |
|        - | 10995 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10996 | ` *   EXTR_PREFIX_ALL` |
|        - | 10997 | ` *       Prefix all variable names with prefix.` |
|        - | 10998 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10999 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11000 | ` *   EXTR_IF_EXISTS` |
|        - | 11001 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11002 | ` *       otherwise do nothing.` |
|        - | 11003 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11004 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11005 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11006 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11007 | ` *      the current symbol table.` |
|        - | 11008 | ` * $prefix` |
|        - | 11009 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11010 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11011 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11012 | ` *  underscore character.` |
|        - | 11013 | ` * Return` |
|        - | 11014 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11015 | ` */` |
|        4 | 11016 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11017 |  |
|        - | 11018 | `	extract_aux_data sAux;` |
|        - | 11019 | `	ph7_hashmap *pMap;` |
|        5 | 11020 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11021 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11022 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11023 | `		return PH7_OK;` |
|        - | 11024 | `	}` |
|        - | 11025 | `	/* Point to the target hashmap */` |
|        5 | 11026 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11027 | `	if( pMap->nEntry < 1 ){` |
|        - | 11028 | `		/* Empty map,return  0 */` |
|      ! 0 | 11029 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11030 | `		return PH7_OK;` |
|        - | 11031 | `	}` |
|        - | 11032 | `	/* Prepare the aux data */` |
|        5 | 11033 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11034 | `	if( nArg > 1 ){` |
|        3 | 11035 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11036 | `		if( nArg > 2 ){` |
|      ! 0 | 11037 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11038 | `		}` |
|        1 | 11039 | `	}` |
|        5 | 11040 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11041 | `	/* Invoke the worker callback */` |
|        5 | 11042 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11043 | `	/* Number of variables successfully imported */` |
|        5 | 11044 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11045 | `	return PH7_OK;` |
|        3 | 11046 |  |
|        - | 11047 | `/*` |
|        - | 11048 | ` * Worker callback for the [extract()] function defined` |
|        - | 11049 | ` * below.` |
|        - | 11050 | ` */` |
|        8 | 11051 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11052 |  |
|        9 | 11053 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11054 | `	int iFlags = pAux->iFlags;` |
|        9 | 11055 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11056 | `	ph7_value *pObj;` |
|        - | 11057 | `	SyString sVar;` |
|        9 | 11058 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11059 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11060 | `	}` |
|        - | 11061 | `	/* Perform a string cast */` |
|        9 | 11062 | `	PH7_MemObjToString(pKey);` |
|        9 | 11063 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11064 | `		/* Unavailable variable name */` |
|      ! 0 | 11065 | `		return SXRET_OK;` |
|        - | 11066 | `	}` |
|        9 | 11067 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11068 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11069 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11070 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11071 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11072 | `			);` |
|      ! 0 | 11073 | `	}else{` |
|       13 | 11074 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11075 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11076 | `	}` |
|        9 | 11077 | `	sVar.zString = pAux->zWorker;` |
|        - | 11078 | `	/* Try to extract the variable */` |
|        9 | 11079 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11080 | `	if( pObj ){` |
|        - | 11081 | `		/* Collision */` |
|        5 | 11082 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11083 | `			return SXRET_OK;` |
|        - | 11084 | `		}` |
|        5 | 11085 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11086 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11087 | `				/* Already prefixed */` |
|      ! 0 | 11088 | `				return SXRET_OK;` |
|        - | 11089 | `			}` |
|      ! 0 | 11090 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11091 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11092 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11093 | `				);` |
|      ! 0 | 11094 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11095 | `		}` |
|        3 | 11096 | `	}else{` |
|        - | 11097 | `		/* Create the variable */` |
|        5 | 11098 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11099 | `	}` |
|        9 | 11100 | `	if( pObj ){` |
|        - | 11101 | `		/* Overwrite the old value */` |
|        9 | 11102 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11103 | `		/* Increment counter */` |
|        9 | 11104 | `		pAux->iCount++;` |
|        4 | 11105 | `	}` |
|        9 | 11106 | `	return SXRET_OK;` |
|        5 | 11107 |  |
|        - | 11108 | `/*` |
|        - | 11109 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11110 | ` * defined below.` |
|        - | 11111 | ` */` |
|        2 | 11112 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11113 |  |
|        3 | 11114 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11115 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11116 | `	ph7_value *pObj;` |
|        - | 11117 | `	SyString sVar;` |
|        - | 11118 | `	/* Perform a string cast */` |
|        3 | 11119 | `	PH7_MemObjToString(pKey);` |
|        3 | 11120 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11121 | `		/* Unavailable variable name */` |
|      ! 0 | 11122 | `		return SXRET_OK;` |
|        - | 11123 | `	}` |
|        3 | 11124 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11125 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11126 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11127 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11128 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11129 | `			);` |
|        2 | 11130 | `	}else{` |
|      ! 0 | 11131 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11132 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11133 | `	}` |
|        3 | 11134 | `	sVar.zString = pAux->zWorker;` |
|        - | 11135 | `	/* Extract the variable */` |
|        3 | 11136 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11137 | `	if( pObj ){` |
|        3 | 11138 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11139 | `	}` |
|        3 | 11140 | `	return SXRET_OK;` |
|        2 | 11141 |  |
|        - | 11142 | `/*` |
|        - | 11143 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11144 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11145 | ` * Parameters` |
|        - | 11146 | ` * $types` |
|        - | 11147 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11148 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11149 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11150 | ` *  POST includes the POST uploaded file information.` |
|        - | 11151 | ` *  Note:` |
|        - | 11152 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11153 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11154 | ` * $prefix` |
|        - | 11155 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11156 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11157 | ` *  variable named $pref_userid.` |
|        - | 11158 | ` * Return` |
|        - | 11159 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11160 | ` */` |
|        2 | 11161 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11162 |  |
|        - | 11163 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11164 | `	extract_aux_data sAux;` |
|        - | 11165 | `	int nLen,nPrefixLen;` |
|        - | 11166 | `	ph7_value *pSuper;` |
|        - | 11167 | `	ph7_vm *pVm;` |
|        - | 11168 | `	/* By default import only $_GET variables  */` |
|        3 | 11169 | `	zImport = "G";` |
|        3 | 11170 | `	nLen = (int)sizeof(char);` |
|        3 | 11171 | `	zPrefix = 0;` |
|        3 | 11172 | `	nPrefixLen = 0;` |
|        3 | 11173 | `	if( nArg > 0 ){` |
|        3 | 11174 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11175 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11176 | `		}` |
|        3 | 11177 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11178 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11179 | `		}` |
|        1 | 11180 | `	}` |
|        - | 11181 | `	/* Point to the underlying VM */` |
|        3 | 11182 | `	pVm = pCtx->pVm;` |
|        - | 11183 | `	/* Initialize the aux data */` |
|        3 | 11184 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11185 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11186 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11187 | `	sAux.pVm = pVm;` |
|        - | 11188 | `	/* Extract */` |
|        3 | 11189 | `	zEnd = &zImport[nLen];` |
|        5 | 11190 | `	while( zImport < zEnd ){` |
|        3 | 11191 | `		int c = zImport[0];` |
|        3 | 11192 | `		pSuper = 0;` |
|        3 | 11193 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11194 | `			/* Import $_GET variables */` |
|        3 | 11195 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11196 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11197 | `			/* Import $_POST variables */` |
|      ! 0 | 11198 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11199 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11200 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11201 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11202 | `		}` |
|        3 | 11203 | `		if( pSuper ){` |
|        - | 11204 | `			/* Iterate throw array entries */` |
|        3 | 11205 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11206 | `		}` |
|        - | 11207 | `		/* Advance the cursor */` |
|        3 | 11208 | `		zImport++;` |
|        1 | 11209 | `	}` |
|        - | 11210 | `	/* All done,return TRUE*/` |
|        3 | 11211 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11212 | `	return PH7_OK;` |
|        1 | 11213 |  |
|        - | 11214 | `/*` |
|        - | 11215 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11216 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11217 | ` * information.` |
|        - | 11218 | ` */` |
|    10386 | 11219 | `static sxi32 VmEvalChunk(` |
|        - | 11220 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11221 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11222 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11223 | `	int iFlags,         /* Compile flag */` |
|        - | 11224 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11225 | `	)` |
|        2 | 11226 |  |
|        - | 11227 | `	SySet *pByteCode,aByteCode;` |
|        - | 11228 | `	SyBlob sSavedNs;` |
|    10388 | 11229 | `	ProcConsumer xErr = 0;` |
|    10388 | 11230 | `	void *pErrData = 0;` |
|        - | 11231 | `	/* Initialize bytecode container */` |
|    10388 | 11232 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10388 | 11233 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11234 | `	/* Reset the code generator */` |
|    10388 | 11235 | `	if( bTrueReturn ){` |
|        - | 11236 | `		/* Included file,log compile-time errors */` |
|     7637 | 11237 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 | 11238 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 | 11239 | `	}` |
|    10388 | 11240 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11241 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11242 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11243 | `	 * the caller's namespace is restored. */` |
|    10388 | 11244 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10388 | 11245 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10388 | 11246 | `	if( bTrueReturn ){` |
|        - | 11247 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 | 11248 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 | 11249 | `	}` |
|        - | 11250 | `	/* Swap bytecode container */` |
|    10388 | 11251 | `	pByteCode = pVm->pByteContainer;` |
|    10388 | 11252 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11253 | `	/* Compile the chunk */` |
|    10388 | 11254 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15581 | 11255 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11256 | `		/* Compilation error,return false */` |
|        3 | 11257 | `		if( pCtx ){` |
|        3 | 11258 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11259 | `		}` |
|        2 | 11260 | `	}else{` |
|        - | 11261 | `		/* Mount any newly defined classes */` |
|        - | 11262 | `		SyHashEntry *pEntry;` |
|        - | 11263 | `		ph7_class *pClass;` |
|        - | 11264 | `		ph7_value sResult; /* Return value */` |
|        - | 11265 | `		sxi32 rc;` |
|    10386 | 11266 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   315160 | 11267 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   299584 | 11268 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11269 | `			/* Only mount classes that haven't been mounted yet */` |
|   299584 | 11270 | `			if( !pClass->bMounted ){` |
|    73648 | 11271 | `				rc = VmMountUserClass(pVm,pClass);` |
|    73648 | 11272 | `				if( rc != SXRET_OK ){` |
|        - | 11273 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11274 | `					if( pCtx ){` |
|      ! 0 | 11275 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11276 | `					}` |
|      ! 0 | 11277 | `					goto Cleanup;` |
|        - | 11278 | `				}` |
|    36823 | 11279 | `			}` |
|        2 | 11280 | `		}` |
|    10386 | 11281 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11282 | `			/* Out of memory */` |
|      ! 0 | 11283 | `			if( pCtx ){` |
|      ! 0 | 11284 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11285 | `			}` |
|      ! 0 | 11286 | `			goto Cleanup;` |
|        - | 11287 | `		}` |
|    10386 | 11288 | `		if( bTrueReturn ){` |
|        - | 11289 | `			/* Assume a boolean true return value */` |
|     7637 | 11290 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 | 11291 | `		}else{` |
|        - | 11292 | `			/* Assume a null return value */` |
|     2750 | 11293 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11294 | `		}` |
|        - | 11295 | `		/* Execute the compiled chunk */` |
|    10386 | 11296 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10386 | 11297 | `		if( pCtx ){` |
|        - | 11298 | `			/* Set the execution result */` |
|     7650 | 11299 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 | 11300 | `		}` |
|    10386 | 11301 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11302 | `	}` |
|     5193 | 11303 | `Cleanup:` |
|        - | 11304 | `	/* Cleanup the mess left behind */` |
|    10388 | 11305 | `	pVm->pByteContainer = pByteCode;` |
|    10388 | 11306 | `	SySetRelease(&aByteCode);` |
|        - | 11307 | `	/* Restore caller's namespace state */` |
|    10388 | 11308 | `	SyBlobReset(&pVm->sNamespace);` |
|    10388 | 11309 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10388 | 11310 | `	SyBlobRelease(&sSavedNs);` |
|    10388 | 11311 | `	return SXRET_OK;` |
|        2 | 11312 |  |
|        - | 11313 | `/*` |
|        - | 11314 | ` * value eval(string $code)` |
|        - | 11315 | ` *   Evaluate a string as PHP code.` |
|        - | 11316 | ` * Parameter` |
|        - | 11317 | ` *  code: PHP code to evaluate.` |
|        - | 11318 | ` * Return` |
|        - | 11319 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11320 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11321 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11322 | ` */` |
|       16 | 11323 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11324 |  |
|        - | 11325 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 11326 | `	if( nArg < 1 ){` |
|        - | 11327 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11328 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11329 | `		return SXRET_OK;` |
|        - | 11330 | `	}` |
|        - | 11331 | `	/* Chunk to evaluate */` |
|       18 | 11332 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 11333 | `	if( sChunk.nByte < 1 ){` |
|        - | 11334 | `		/* Empty string,return NULL */` |
|        3 | 11335 | `		ph7_result_null(pCtx);` |
|        3 | 11336 | `		return SXRET_OK;` |
|        - | 11337 | `	}` |
|        - | 11338 | `	/* Eval the chunk */` |
|       16 | 11339 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 11340 | `	return SXRET_OK;` |
|       10 | 11341 |  |
|        - | 11342 | `/*` |
|        - | 11343 | ` * Check if a file path is already included.` |
|        - | 11344 | ` */` |
|    15268 | 11345 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 11346 |  |
|        - | 11347 | `	SyString *aEntries;` |
|        - | 11348 | `	sxu32 n;` |
|    15269 | 11349 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11350 | `	/* Perform a linear search */` |
| 58267061 | 11351 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 11352 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11353 | `			/* Already included */` |
|        7 | 11354 | `			return TRUE;` |
|        - | 11355 | `		}` |
| 29125897 | 11356 | `	}` |
|    15263 | 11357 | `	return FALSE;` |
|     7635 | 11358 |  |
|        - | 11359 | `/*` |
|        - | 11360 | ` * Push a file path in the appropriate VM container.` |
|        - | 11361 | ` */` |
|    17996 | 11362 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11363 |  |
|        - | 11364 | `	SyString sPath;` |
|        - | 11365 | `	char *zDup;` |
|        - | 11366 | `#ifdef __WINNT__` |
|        - | 11367 | `	char *zCur;` |
|        - | 11368 | `#endif` |
|        - | 11369 | `	sxi32 rc;` |
|    17998 | 11370 | `	if( nLen < 0 ){` |
|     2730 | 11371 | `		nLen = SyStrlen(zPath);` |
|     1364 | 11372 | `	}` |
|        - | 11373 | `	/* Duplicate the file path first */` |
|    17998 | 11374 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17998 | 11375 | `	if( zDup == 0 ){` |
|      ! 0 | 11376 | `		return SXERR_MEM;` |
|        - | 11377 | `	}` |
|        - | 11378 | `#ifdef __WINNT__` |
|        - | 11379 | `	/* Normalize path on windows` |
|        - | 11380 | `	 * Example:` |
|        - | 11381 | `	 *    Path/To/File.php` |
|        - | 11382 | `	 * becomes` |
|        - | 11383 | `	 *   path\to\file.php` |
|        - | 11384 | `	 */` |
|        2 | 11385 | `	zCur = zDup;` |
|        2 | 11386 | `	while( zCur[0] != 0 ){` |
|        2 | 11387 | `		if( zCur[0] == '/' ){` |
|        2 | 11388 | `			zCur[0] = '\\';` |
|        2 | 11389 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11390 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11391 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11392 | `		}` |
|        2 | 11393 | `		zCur++;` |
|        2 | 11394 | `	}` |
|        - | 11395 | `#endif` |
|        - | 11396 | `	/* Install the file path */` |
|    17998 | 11397 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17998 | 11398 | `	if( !bMain ){` |
|    15269 | 11399 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11400 | `			/* Already included */` |
|        7 | 11401 | `			*pNew = 0;` |
|        4 | 11402 | `		}else{` |
|        - | 11403 | `			/* Insert in the corresponding container */` |
|    15263 | 11404 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 11405 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11406 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11407 | `				return rc;` |
|        - | 11408 | `			}` |
|    15263 | 11409 | `			*pNew = 1;` |
|        - | 11410 | `		}` |
|     7634 | 11411 | `	}` |
|    17998 | 11412 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17998 | 11413 | `	return SXRET_OK;` |
|     9000 | 11414 |  |
|        - | 11415 | `/*` |
|        - | 11416 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11417 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11418 | ` * indicates failure.` |
|        - | 11419 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11420 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11421 | ` * operations.` |
|        - | 11422 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11423 | ` * this function is a no-op.` |
|        - | 11424 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11425 | ` * constructs for more information.` |
|        - | 11426 | ` */` |
|     7642 | 11427 | `static sxi32 VmExecIncludedFile(` |
|        - | 11428 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11429 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11430 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11431 | `	 )` |
|        2 | 11432 |  |
|        - | 11433 | `	sxi32 rc;` |
|        - | 11434 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11435 | `	const ph7_io_stream *pStream;` |
|        - | 11436 | `	SyBlob sContents;` |
|        - | 11437 | `	void *pHandle;` |
|        - | 11438 | `	ph7_vm *pVm;` |
|        - | 11439 | `	int isNew;` |
|        - | 11440 | `	/* Initialize fields */` |
|     7644 | 11441 | `	pVm = pCtx->pVm;` |
|     7644 | 11442 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 11443 | `	isNew = 0;` |
|        - | 11444 | `	/* Extract the associated stream */` |
|     7644 | 11445 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11446 | `	/*` |
|        - | 11447 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11448 | `	 * in a read-only mode.` |
|        - | 11449 | `	 */` |
|     7644 | 11450 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 11451 | `	if( pHandle == 0 ){` |
|        3 | 11452 | `		return SXERR_IO;` |
|        - | 11453 | `	}` |
|     7641 | 11454 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 11455 | `	if( IncludeOnce && !isNew ){` |
|        - | 11456 | `		/* Already included */` |
|        5 | 11457 | `		rc = SXERR_EXISTS;` |
|        3 | 11458 | `	}else{` |
|        - | 11459 | `		/* Read the whole file contents */` |
|     7637 | 11460 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 11461 | `		if( rc == SXRET_OK ){` |
|        - | 11462 | `			SyString sScript;` |
|        - | 11463 | `			/* Compile and execute the script */` |
|     7637 | 11464 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 11465 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 11466 | `		}` |
|        - | 11467 | `	}` |
|        - | 11468 | `	/* Pop from the set of included file */` |
|     7641 | 11469 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11470 | `	/* Close the handle */` |
|     7641 | 11471 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11472 | `	/* Release the working buffer */` |
|     7641 | 11473 | `	SyBlobRelease(&sContents);` |
|        - | 11474 | `#else` |
|        - | 11475 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11476 | `	SXUNUSED(pPath);` |
|        - | 11477 | `	SXUNUSED(IncludeOnce);` |
|        - | 11478 | `	rc = SXERR_IO;` |
|        - | 11479 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 11480 | `	return rc;` |
|     3823 | 11481 |  |
|        - | 11482 | `/*` |
|        - | 11483 | ` * string get_include_path(void)` |
|        - | 11484 | ` *  Gets the current include_path configuration option.` |
|        - | 11485 | ` * Parameter` |
|        - | 11486 | ` *  None` |
|        - | 11487 | ` * Return` |
|        - | 11488 | ` *  Included paths as a string` |
|        - | 11489 | ` */` |
|        2 | 11490 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11491 |  |
|        3 | 11492 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11493 | `	SyString *aEntry;` |
|        - | 11494 | `	int dir_sep;` |
|        - | 11495 | `	sxu32 n;` |
|        - | 11496 | `#ifdef __WINNT__` |
|        1 | 11497 | `	dir_sep = ';';` |
|        - | 11498 | `#else` |
|        - | 11499 | `	/* Assume UNIX path separator */` |
|        2 | 11500 | `	dir_sep = ':';` |
|        - | 11501 | `#endif` |
|        1 | 11502 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11503 | `	SXUNUSED(apArg);` |
|        - | 11504 | `	/* Point to the list of import paths */` |
|        3 | 11505 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11506 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11507 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11508 | `		if( n > 0 ){` |
|        - | 11509 | `			/* Append dir seprator */` |
|      ! 0 | 11510 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11511 | `		}` |
|        - | 11512 | `		/* Append path */` |
|        3 | 11513 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11514 | `	}` |
|        3 | 11515 | `	return PH7_OK;` |
|        1 | 11516 |  |
|        - | 11517 | `/*` |
|        - | 11518 | ` * string get_get_included_files(void)` |
|        - | 11519 | ` *  Gets the current include_path configuration option.` |
|        - | 11520 | ` * Parameter` |
|        - | 11521 | ` *  None` |
|        - | 11522 | ` * Return` |
|        - | 11523 | ` *  Included paths as a string` |
|        - | 11524 | ` */` |
|        2 | 11525 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11526 |  |
|        3 | 11527 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11528 | `	ph7_value *pArray,*pWorker;` |
|        - | 11529 | `	SyString *pEntry;` |
|        - | 11530 | `	int c,d;` |
|        - | 11531 | `	/* Create an array and a working value */` |
|        3 | 11532 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11533 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11534 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11535 | `		/* Out of memory,return null */` |
|      ! 0 | 11536 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11537 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11538 | `		SXUNUSED(apArg);` |
|      ! 0 | 11539 | `		return PH7_OK;` |
|        - | 11540 | `	}` |
|        3 | 11541 | `	c = d = '/';` |
|        - | 11542 | `#ifdef __WINNT__` |
|        1 | 11543 | `	d = '\\';` |
|        - | 11544 | `#endif` |
|        - | 11545 | `	/* Iterate throw entries */` |
|        3 | 11546 | `	SySetResetCursor(pFiles);` |
|     3689 | 11547 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11548 | `		const char *zBase,*zEnd;` |
|        - | 11549 | `		int iLen;` |
|        - | 11550 | `		/* reset the string cursor */` |
|     3687 | 11551 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11552 | `		/* Extract base name */` |
|     3687 | 11553 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11554 | `		/* Ignore trailing '/' */` |
|     5530 | 11555 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11556 | `			zEnd--;` |
|      ! 0 | 11557 | `		}` |
|     3687 | 11558 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 11559 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 11560 | `			zEnd--;` |
|        1 | 11561 | `		}` |
|     3687 | 11562 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 11563 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11564 | `		/* Copy entry name */` |
|     3687 | 11565 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11566 | `		/* Perform the insertion */` |
|     3687 | 11567 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11568 | `	}` |
|        - | 11569 | `	/* All done,return the created array */` |
|        3 | 11570 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11571 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11572 | `	 * by the engine as soon we return from this foreign` |
|        - | 11573 | `	 * function.` |
|        - | 11574 | `	 */` |
|        3 | 11575 | `	return PH7_OK;` |
|        2 | 11576 |  |
|        - | 11577 | `/*` |
|        - | 11578 | ` * include:` |
|        - | 11579 | ` * According to the PHP reference manual.` |
|        - | 11580 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11581 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11582 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11583 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11584 | ` *  and the current working directory before failing. The include()` |
|        - | 11585 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11586 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11587 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11588 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11589 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11590 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11591 | ` *  directory to find the requested file.` |
|        - | 11592 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11593 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11594 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11595 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11596 | ` */` |
|     7630 | 11597 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11598 |  |
|        - | 11599 | `	SyString sFile;` |
|        - | 11600 | `	sxi32 rc;` |
|     7632 | 11601 | `	if( nArg < 1 ){` |
|        - | 11602 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11603 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11604 | `		return SXRET_OK;` |
|        - | 11605 | `	}` |
|        - | 11606 | `	/* File to include */` |
|     7632 | 11607 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 11608 | `	if( sFile.nByte < 1 ){` |
|        - | 11609 | `		/* Empty string,return NULL */` |
|      ! 0 | 11610 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11611 | `		return SXRET_OK;` |
|        - | 11612 | `	}` |
|        - | 11613 | `	/* Open,compile and execute the desired script */` |
|     7632 | 11614 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 11615 | `	if( rc != SXRET_OK ){` |
|        - | 11616 | `		/* Emit a warning and return false */` |
|        3 | 11617 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11618 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11619 | `	}` |
|     7632 | 11620 | `	return SXRET_OK;` |
|     3817 | 11621 |  |
|        - | 11622 | `/*` |
|        - | 11623 | ` * include_once:` |
|        - | 11624 | ` *  According to the PHP reference manual.` |
|        - | 11625 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11626 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11627 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11628 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11629 | ` *   just once.` |
|        - | 11630 | ` */` |
|        4 | 11631 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11632 |  |
|        - | 11633 | `	SyString sFile;` |
|        - | 11634 | `	sxi32 rc;` |
|        5 | 11635 | `	if( nArg < 1 ){` |
|        - | 11636 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11637 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11638 | `		return SXRET_OK;` |
|        - | 11639 | `	}` |
|        - | 11640 | `	/* File to include */` |
|        5 | 11641 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11642 | `	if( sFile.nByte < 1 ){` |
|        - | 11643 | `		/* Empty string,return NULL */` |
|      ! 0 | 11644 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11645 | `		return SXRET_OK;` |
|        - | 11646 | `	}` |
|        - | 11647 | `	/* Open,compile and execute the desired script */` |
|        5 | 11648 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11649 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11650 | `		/* File already included,return TRUE */` |
|        3 | 11651 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11652 | `		return SXRET_OK;` |
|        - | 11653 | `	}` |
|        3 | 11654 | `	if( rc != SXRET_OK ){` |
|        - | 11655 | `		/* Emit a warning and return false */` |
|      ! 0 | 11656 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11657 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11658 | ` 	}` |
|        3 | 11659 | `	return SXRET_OK;` |
|        3 | 11660 |  |
|        - | 11661 | `/*` |
|        - | 11662 | ` * require.` |
|        - | 11663 | ` *  According to the PHP reference manual.` |
|        - | 11664 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11665 | ` *   also produce a fatal level error.` |
|        - | 11666 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11667 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11668 | ` */` |
|        4 | 11669 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11670 |  |
|        - | 11671 | `	SyString sFile;` |
|        - | 11672 | `	sxi32 rc;` |
|        5 | 11673 | `	if( nArg < 1 ){` |
|        - | 11674 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11675 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11676 | `		return SXRET_OK;` |
|        - | 11677 | `	}` |
|        - | 11678 | `	/* File to include */` |
|        5 | 11679 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11680 | `	if( sFile.nByte < 1 ){` |
|        - | 11681 | `		/* Empty string,return NULL */` |
|      ! 0 | 11682 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11683 | `		return SXRET_OK;` |
|        - | 11684 | `	}` |
|        - | 11685 | `	/* Open,compile and execute the desired script */` |
|        5 | 11686 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11687 | `	if( rc != SXRET_OK ){` |
|        - | 11688 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11689 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11690 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11691 | `		return PH7_ABORT;` |
|        - | 11692 | `	}` |
|        5 | 11693 | `	return SXRET_OK;` |
|        3 | 11694 |  |
|        - | 11695 | `/*` |
|        - | 11696 | ` * require_once:` |
|        - | 11697 | ` *  According to the PHP reference manual.` |
|        - | 11698 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11699 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11700 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11701 | ` *   and how it differs from its non _once siblings.` |
|        - | 11702 | ` */` |
|        4 | 11703 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11704 |  |
|        - | 11705 | `	SyString sFile;` |
|        - | 11706 | `	sxi32 rc;` |
|        5 | 11707 | `	if( nArg < 1 ){` |
|        - | 11708 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11709 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11710 | `		return SXRET_OK;` |
|        - | 11711 | `	}` |
|        - | 11712 | `	/* File to include */` |
|        5 | 11713 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11714 | `	if( sFile.nByte < 1 ){` |
|        - | 11715 | `		/* Empty string,return NULL */` |
|      ! 0 | 11716 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11717 | `		return SXRET_OK;` |
|        - | 11718 | `	}` |
|        - | 11719 | `	/* Open,compile and execute the desired script */` |
|        5 | 11720 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11721 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11722 | `		/* File already included,return TRUE */` |
|        3 | 11723 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11724 | `		return SXRET_OK;` |
|        - | 11725 | `	}` |
|        3 | 11726 | `	if( rc != SXRET_OK ){` |
|        - | 11727 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11728 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11729 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11730 | `		return PH7_ABORT;` |
|        - | 11731 | `	}` |
|        3 | 11732 | `	return SXRET_OK;` |
|        3 | 11733 |  |
|        - | 11734 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11735 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11736 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11737 | `/* Table of built-in VM functions. */` |
|        - | 11738 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 11739 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 11740 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 11741 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 11742 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 11743 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 11744 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 11745 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 11746 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 11747 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 11748 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 11749 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 11750 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 11751 | `	    /* Constants management */` |
|        - | 11752 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 11753 | `	{ "define",   vm_builtin_define               },` |
|        - | 11754 | `	{ "constant", vm_builtin_constant             },` |
|        - | 11755 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 11756 | `	   /* Class/Object functions */` |
|        - | 11757 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 11758 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 11759 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 11760 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 11761 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 11762 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 11763 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 11764 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 11765 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 11766 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 11767 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 11768 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 11769 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 11770 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 11771 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 11772 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 11773 | `	   /* Random numbers/strings generators */` |
|        - | 11774 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 11775 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 11776 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 11777 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 11778 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 11779 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11780 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11781 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 11782 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11783 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11784 | `	   /* Language constructs functions */` |
|        - | 11785 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 11786 | `	{ "print", vm_builtin_print                   },` |
|        - | 11787 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 11788 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 11789 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 11790 | `	  /* Variable handling functions */` |
|        - | 11791 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 11792 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 11793 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 11794 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 11795 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 11796 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 11797 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 11798 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 11799 | `	  /* Ouput control functions */` |
|        - | 11800 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 11801 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 11802 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 11803 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 11804 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 11805 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 11806 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 11807 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 11808 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 11809 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 11810 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 11811 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 11812 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 11813 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 11814 | `	  /* Assertion functions */` |
|        - | 11815 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 11816 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 11817 | `	  /* Error reporting functions */` |
|        - | 11818 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 11819 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 11820 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 11821 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 11822 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 11823 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 11824 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 11825 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 11826 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 11827 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 11828 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 11829 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 11830 | `	  /* Release info */` |
|        - | 11831 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 11832 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 11833 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 11834 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 11835 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 11836 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 11837 | `	  /* hashmap */` |
|        - | 11838 | `	{"compact",          vm_builtin_compact       },` |
|        - | 11839 | `	{"extract",          vm_builtin_extract       },` |
|        - | 11840 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 11841 | `	  /* URL related function */` |
|        - | 11842 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 11843 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 11844 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11845 | `	   /* XML processing functions */` |
|        - | 11846 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 11847 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 11848 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 11849 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 11850 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 11851 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 11852 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 11853 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 11854 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 11855 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 11856 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 11857 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 11858 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 11859 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 11860 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 11861 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 11862 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 11863 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 11864 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 11865 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 11866 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 11867 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11868 | `	   /* UTF-8 encoding/decoding */` |
|        - | 11869 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 11870 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 11871 | `	   /* Command line processing */` |
|        - | 11872 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 11873 | `	   /* JSON encoding/decoding */` |
|        - | 11874 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 11875 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 11876 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 11877 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 11878 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 11879 | `	   /* Files/URI inclusion facility */` |
|        - | 11880 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 11881 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 11882 | `	{ "include",      vm_builtin_include          },` |
|        - | 11883 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 11884 | `	{ "require",      vm_builtin_require          },` |
|        - | 11885 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 11886 | `};` |
|        - | 11887 | `/*` |
|        - | 11888 | ` * Register the built-in VM functions defined above.` |
|        - | 11889 | ` */` |
|     2476 | 11890 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 11891 |  |
|        - | 11892 | `	sxi32 rc;` |
|        - | 11893 | `	sxu32 n;` |
|   309502 | 11894 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 11895 | `		/* Note that these special functions have access` |
|        - | 11896 | `		 * to the underlying virtual machine as their` |
|        - | 11897 | `		 * private data.` |
|        - | 11898 | `		 */` |
|   307026 | 11899 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   307026 | 11900 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11901 | `			return rc;` |
|        - | 11902 | `		}` |
|   153514 | 11903 | `	}` |
|     2478 | 11904 | `	return SXRET_OK;` |
|     1240 | 11905 |  |
|        - | 11906 | `/*` |
|        - | 11907 | ` * Check if the given name refer to an installed class.` |
|        - | 11908 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 11909 | ` */` |
|    28926 | 11910 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 11911 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 11912 | `	const char *zName,  /* Name of the target class */` |
|        - | 11913 | `	sxu32 nByte,        /* zName length */` |
|        - | 11914 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 11915 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 11916 | `						 */` |
|        - | 11917 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 11918 | `	)` |
|        2 | 11919 |  |
|        - | 11920 | `	SyHashEntry *pEntry;` |
|        - | 11921 | `	ph7_class *pClass;` |
|    14463 | 11922 | `	SXUNUSED(iNest);` |
|        - | 11923 | `	/* Exact class lookup.` |
|        - | 11924 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 11925 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    28928 | 11926 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    28928 | 11927 | `	if( pEntry == 0 ){` |
|       10 | 11928 | `		return 0;` |
|        - | 11929 | `	}` |
|    28920 | 11930 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    28920 | 11931 | `	if( !iLoadable ){` |
|    27740 | 11932 | `		return pClass;` |
|        - | 11933 | `	}` |
|        - | 11934 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1182 | 11935 | `	while(pClass){` |
|     1182 | 11936 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1182 | 11937 | `			return pClass;` |
|        - | 11938 | `		}` |
|      ! 0 | 11939 | `		pClass = pClass->pNextName;` |
|      ! 0 | 11940 | `	}` |
|      ! 0 | 11941 | `	return 0;` |
|    14465 | 11942 |  |
|        - | 11943 | `/*` |
|        - | 11944 | ` * Reference Table Implementation` |
|        - | 11945 | ` * Status: stable <chm@symisc.net>` |
|        - | 11946 | ` * Intro` |
|        - | 11947 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 11948 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 11949 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 11950 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 11951 | ` *  Refer to the official for more information on this powerful` |
|        - | 11952 | ` *  extension.` |
|        - | 11953 | ` */` |
|        - | 11954 | `/*` |
|        - | 11955 | ` * Allocate a new reference entry.` |
|        - | 11956 | ` */` |
|  3014324 | 11957 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 11958 |  |
|        - | 11959 | `	VmRefObj *pRef;` |
|        - | 11960 | `	/* Allocate a new instance */` |
|  3014326 | 11961 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3014326 | 11962 | `	if( pRef == 0 ){` |
|      ! 0 | 11963 | `		return 0;` |
|        - | 11964 | `	}` |
|        - | 11965 | `	/* Zero the structure */` |
|  3014326 | 11966 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 11967 | `	/* Initialize fields */` |
|  3014326 | 11968 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3014326 | 11969 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3014326 | 11970 | `	pRef->nIdx = nIdx;` |
|  3014326 | 11971 | `	return pRef;` |
|  1507164 | 11972 |  |
|        - | 11973 | `/*` |
|        - | 11974 | ` * Default hash function used by the reference table` |
|        - | 11975 | ` * for lookup/insertion operations.` |
|        - | 11976 | ` */` |
| 16706497 | 11977 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 11978 |  |
|        - | 11979 | `	/* Calculate the hash based on the memory object index */` |
| 16706499 | 11980 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 11981 |  |
|        - | 11982 | `/*` |
|        - | 11983 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 11984 | ` * in the reference table.` |
|        - | 11985 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 11986 | ` * otherwise.` |
|        - | 11987 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11988 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11989 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11990 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11991 | ` * Refer to the official for more information on this powerful` |
|        - | 11992 | ` * extension.` |
|        - | 11993 | ` */` |
|  8994278 | 11994 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 11995 |  |
|        - | 11996 | `	VmRefObj *pRef;` |
|        - | 11997 | `	sxu32 nBucket;` |
|        - | 11998 | `	/* Point to the appropriate bucket */` |
|  8994280 | 11999 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12000 | `	/* Perform the lookup */` |
|  8994280 | 12001 | `	pRef = pVm->apRefObj[nBucket];` |
| 19319073 | 12002 | `	for(;;){` |
| 38625710 | 12003 | `		if( pRef == 0 ){` |
|  3092988 | 12004 | `			break;` |
|        - | 12005 | `		}` |
| 35532724 | 12006 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12007 | `			/* Entry found */` |
|  5901294 | 12008 | `			return pRef;` |
|        - | 12009 | `		}` |
|        - | 12010 | `		/* Point to the next entry */` |
| 29631432 | 12011 | `		pRef = pRef->pNextCollide;` |
|        2 | 12012 | `	}` |
|        - | 12013 | `	/* No such entry,return NULL */` |
|  3092988 | 12014 | `	return 0;` |
|  4497141 | 12015 |  |
|        - | 12016 | `/*` |
|        - | 12017 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12018 | ` *` |
|        - | 12019 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12020 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12021 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12022 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12023 | ` * Refer to the official for more information on this powerful` |
|        - | 12024 | ` * extension.` |
|        - | 12025 | ` */` |
|  3014324 | 12026 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12027 |  |
|        - | 12028 | `	sxu32 nBucket;` |
|  3014326 | 12029 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12030 | `		VmRefObj **apNew;` |
|        - | 12031 | `		sxu32 nNew;` |
|        - | 12032 | `		/* Allocate a larger table */` |
|     4232 | 12033 | `		nNew = pVm->nRefSize << 1;` |
|     4232 | 12034 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4232 | 12035 | `		if( apNew ){` |
|     4232 | 12036 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12037 | `			sxu32 n;` |
|        - | 12038 | `			/* Zero the structure */` |
|     4232 | 12039 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12040 | `			/* Rehash all referenced entries */` |
|  2843046 | 12041 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12042 | `				/* Remove old collision links */` |
|  2838816 | 12043 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12044 | `				/* Point to the appropriate bucket */` |
|  2838816 | 12045 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12046 | `				/* Insert the entry  */` |
|  2838816 | 12047 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2838816 | 12048 | `				if( apNew[nBucket] ){` |
|  2298896 | 12049 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12050 | `				}` |
|  2838816 | 12051 | `				apNew[nBucket] = pEntry;` |
|        - | 12052 | `				/* Point to the next entry */` |
|  2838816 | 12053 | `				pEntry = pEntry->pNext;` |
|  1419409 | 12054 | `			}` |
|        - | 12055 | `			/* Release the old table */` |
|     4232 | 12056 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12057 | `			/* Install the new one */` |
|     4232 | 12058 | `			pVm->apRefObj = apNew;` |
|     4232 | 12059 | `			pVm->nRefSize = nNew;` |
|     2115 | 12060 | `		}` |
|     2115 | 12061 | `	}` |
|        - | 12062 | `	/* Point to the appropriate bucket */` |
|  3014326 | 12063 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12064 | `	/* Insert the entry */` |
|  3014326 | 12065 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3014326 | 12066 | `	if( pVm->apRefObj[nBucket] ){` |
|  2496767 | 12067 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1248435 | 12068 | `	}` |
|  3014326 | 12069 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3014326 | 12070 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3014326 | 12071 | `	pVm->nRefUsed++;` |
|  3014326 | 12072 | `	return SXRET_OK;` |
|        2 | 12073 |  |
|        - | 12074 | `/*` |
|        - | 12075 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12076 | ` * the reference table.` |
|        - | 12077 | ` * This function is invoked when the user perform an unset` |
|        - | 12078 | ` * call [i.e: unset($var); ].` |
|        - | 12079 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12080 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12081 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12082 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12083 | ` * Refer to the official for more information on this powerful` |
|        - | 12084 | ` * extension.` |
|        - | 12085 | ` */` |
|  2979004 | 12086 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12087 |  |
|        - | 12088 | `	ph7_hashmap_node **apNode;` |
|        - | 12089 | `	SyHashEntry **apEntry;` |
|        - | 12090 | `	sxu32 n;` |
|        - | 12091 | `	/* Point to the reference table */` |
|  2979006 | 12092 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2979006 | 12093 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12094 | `	/* Unlink the entry from the reference table */` |
|  3063534 | 12095 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    84530 | 12096 | `		if( apEntry[n] ){` |
|    84480 | 12097 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42239 | 12098 | `		}` |
|    42266 | 12099 | `	}` |
|  5876174 | 12100 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2897170 | 12101 | `		if( apNode[n] ){` |
|     6794 | 12102 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3396 | 12103 | `		}` |
|  1448586 | 12104 | `	}` |
|  2979006 | 12105 | `	if( pRef->pPrevCollide ){` |
|  1119925 | 12106 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   560204 | 12107 | `	}else{` |
|  1859083 | 12108 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12109 | `	}` |
|  2979006 | 12110 | `	if( pRef->pNextCollide ){` |
|  1685632 | 12111 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   842958 | 12112 | `	}` |
|  2979006 | 12113 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12114 | `	/* Release the node */` |
|  2979006 | 12115 | `	SySetRelease(&pRef->aReference);` |
|  2979006 | 12116 | `	SySetRelease(&pRef->aArrEntries);` |
|  2979006 | 12117 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2979006 | 12118 | `	pVm->nRefUsed--;` |
|  2979006 | 12119 | `	return SXRET_OK;` |
|        2 | 12120 |  |
|        - | 12121 | `/*` |
|        - | 12122 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12123 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12124 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12125 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12126 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12127 | ` * Refer to the official for more information on this powerful` |
|        - | 12128 | ` * extension.` |
|        - | 12129 | ` */` |
|  3046174 | 12130 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12131 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12132 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12133 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12134 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12135 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12136 | `	)` |
|        2 | 12137 |  |
|  3046176 | 12138 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12139 | `	VmRefObj *pRef;` |
|        - | 12140 | `	/* Check if the referenced object already exists */` |
|  3046176 | 12141 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3046176 | 12142 | `	if( pRef == 0 ){` |
|        - | 12143 | `		/* Create a new entry */` |
|  3014326 | 12144 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3014326 | 12145 | `		if( pRef == 0 ){` |
|      ! 0 | 12146 | `			return SXERR_MEM;` |
|        - | 12147 | `		}` |
|  3014326 | 12148 | `		pRef->iFlags = iFlags;` |
|        - | 12149 | `		/* Install the entry */` |
|  3014326 | 12150 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1507162 | 12151 | `	}` |
|  3046176 | 12152 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3046176 | 12153 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12154 | `		VmSlot sRef;` |
|        - | 12155 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12156 | `		 * be deleted when we leave this frame.` |
|        - | 12157 | `		 */` |
|    78748 | 12158 | `		sRef.nIdx = nIdx;` |
|    78748 | 12159 | `		sRef.pUserData = pEntry;` |
|    78748 | 12160 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12161 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12162 | `		}` |
|    39373 | 12163 | `	}` |
|  3046176 | 12164 | `	if( pEntry ){` |
|        - | 12165 | `		/* Address of the hash-entry */` |
|   110406 | 12166 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    55202 | 12167 | `	}` |
|  3046176 | 12168 | `	if( pMapEntry ){` |
|        - | 12169 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2930798 | 12170 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1465398 | 12171 | `	}` |
|  3046176 | 12172 | `	return SXRET_OK;` |
|  1523089 | 12173 |  |
|        - | 12174 | `/*` |
|        - | 12175 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12176 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12177 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12178 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12179 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12180 | ` * Refer to the official for more information on this powerful` |
|        - | 12181 | ` * extension.` |
|        - | 12182 | ` */` |
|  2969094 | 12183 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12184 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12185 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12186 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12187 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12188 | `	)` |
|        2 | 12189 |  |
|        - | 12190 | `	VmRefObj *pRef;` |
|        - | 12191 | `	sxu32 n;` |
|        - | 12192 | `	/* Check if the referenced object already exists */` |
|  2969096 | 12193 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2969096 | 12194 | `	if( pRef == 0 ){` |
|        - | 12195 | `		/* Not such entry */` |
|    78658 | 12196 | `		return SXERR_NOTFOUND;` |
|        - | 12197 | `	}` |
|        - | 12198 | `	/* Remove the desired entry */` |
|  2890440 | 12199 | `	if( pEntry ){` |
|        - | 12200 | `		SyHashEntry **apEntry;` |
|       56 | 12201 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12202 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12203 | `			if( apEntry[n] == pEntry ){` |
|        - | 12204 | `				/* Nullify the entry */` |
|       56 | 12205 | `				apEntry[n] = 0;` |
|        - | 12206 | `				/*` |
|        - | 12207 | `				 * NOTE:` |
|        - | 12208 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12209 | `				 * we avoid wasting spaces.` |
|        - | 12210 | `				 */` |
|       27 | 12211 | `			}` |
|       79 | 12212 | `		}` |
|       27 | 12213 | `	}` |
|  2890440 | 12214 | `	if( pMapEntry ){` |
|        - | 12215 | `		ph7_hashmap_node **apNode;` |
|  2890386 | 12216 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5780864 | 12217 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2890480 | 12218 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12219 | `				/* nullify the entry */` |
|  2890386 | 12220 | `				apNode[n] = 0;` |
|  1445192 | 12221 | `			}` |
|  1445241 | 12222 | `		}` |
|  1445192 | 12223 | `	}` |
|  2890440 | 12224 | `	return SXRET_OK;` |
|  1484549 | 12225 |  |
|        - | 12226 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12227 | `/*` |
|        - | 12228 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12229 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12230 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12231 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12232 | ` * For more information on how to register IO stream devices,please` |
|        - | 12233 | ` * refer to the official documentation.` |
|        - | 12234 | ` */` |
|    23604 | 12235 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12236 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12237 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12238 | `	int nByte              /* *pzDevice length*/` |
|        - | 12239 | `	)` |
|        2 | 12240 |  |
|        - | 12241 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12242 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12243 | `	SyString sDev,sCur;` |
|        - | 12244 | `	sxu32 n,nEntry;` |
|        - | 12245 | `	int rc;` |
|        - | 12246 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23606 | 12247 | `	zNext = zCur = zIn = *pzDevice;` |
|    23606 | 12248 | `	zEnd = &zIn[nByte];` |
|  1506807 | 12249 | `	while( zIn < zEnd ){` |
|  1483205 | 12250 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12251 | `			/* Got one */` |
|        3 | 12252 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12253 | `			break;` |
|        - | 12254 | `		}` |
|        - | 12255 | `		/* Advance the cursor */` |
|  1483203 | 12256 | `		zIn++;` |
|        2 | 12257 | `	}` |
|    23606 | 12258 | `	if( zIn >= zEnd ){` |
|        - | 12259 | `		/* No such scheme,return the default stream */` |
|    23604 | 12260 | `		return pVm->pDefStream;` |
|        - | 12261 | `	}` |
|        3 | 12262 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12263 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12264 | `	SyStringFullTrim(&sDev);` |
|        - | 12265 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12266 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12267 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12268 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12269 | `		pStream = apStream[n];` |
|        3 | 12270 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12271 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12272 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12273 | `		if( rc == 0 ){` |
|        - | 12274 | `			/* Stream device found */` |
|        3 | 12275 | `			*pzDevice = zNext;` |
|        3 | 12276 | `			return pStream;` |
|        - | 12277 | `		}` |
|      ! 0 | 12278 | `	}` |
|        - | 12279 | `	/* No such stream,return NULL */` |
|      ! 0 | 12280 | `	return 0;` |
|    11804 | 12281 |  |
|        - | 12282 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12283 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12284 |  |
