# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5038/6598 lines (76.36%)

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
|   787900 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   787902 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   787868 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   787858 |   104 | `	return FALSE;` |
|   393974 |   105 |  |
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
|   502598 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   502600 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   502600 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   502596 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   502596 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   502596 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   502596 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   502596 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   502596 |   152 | `	pCons->xExpand = xExpand;` |
|   502596 |   153 | `	pCons->pUserData = pUserData;` |
|   502596 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   502596 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   502596 |   161 | `	return SXRET_OK;` |
|   251301 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1100408 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1100410 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1100410 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1100410 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1100410 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1100410 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1100410 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1100410 |   195 | `	pFunc->pVm   = pVm;` |
|  1100410 |   196 | `	pFunc->xFunc = xFunc;` |
|  1100410 |   197 | `	pFunc->pUserData = pUserData;` |
|  1100410 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1100410 |   200 | `	*ppOut = pFunc;` |
|  1100410 |   201 | `	return SXRET_OK;` |
|   550206 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1102724 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1102726 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1102726 |   223 | `	if( pEntry ){` |
|     2318 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2318 |   225 | `		pFunc->pUserData = pUserData;` |
|     2318 |   226 | `		pFunc->xFunc = xFunc;` |
|     2318 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2318 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1100410 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1100410 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1100410 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1100410 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1100410 |   243 | `	return SXRET_OK;` |
|   551364 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   158250 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   158252 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   158252 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   158252 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   158252 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   158252 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   158252 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   158252 |   270 | `	pFunc->iFlags = iFlags;` |
|   158252 |   271 | `	pFunc->pUserData = pUserData;` |
|   158252 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   158252 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   533592 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   533594 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    34144 |   293 | `		pName = &pFunc->sName;` |
|    17071 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   533594 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   533594 |   297 | `	if( pEntry ){` |
|   396216 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   396216 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      184 |   301 | `			pFunc->pNextName = pLink;` |
|      184 |   302 | `			pEntry->pUserData = pFunc;` |
|       91 |   303 | `		}` |
|   396216 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   137380 |   307 | `	pFunc->pNextName = 0;` |
|   137380 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   137380 |   309 | `	return rc;` |
|   266798 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    39138 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    39140 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    39140 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    39140 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    39110 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    39110 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    39110 |   334 | `	return rc;` |
|    19571 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3195328 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3195330 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3195330 |   352 | `	sInstr.iP1 = iP1;` |
|  3195330 |   353 | `	sInstr.iP2 = iP2;` |
|  3195330 |   354 | `	sInstr.p3  = p3;` |
|  3195330 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   184330 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    92164 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3195330 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3195330 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3195330 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   378936 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   378938 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   378938 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   378938 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   189468 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   189470 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   181676 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   181678 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   181678 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   957564 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   957566 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   172714 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   172716 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   618716 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   618718 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    26496 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    26498 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26498 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    26498 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26498 |   427 | `	return &aInstr[n - 2];` |
|    13250 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    16024 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16026 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16026 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16026 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16026 |   447 | `	pFrame->pUserData = pUserData;` |
|    16026 |   448 | `	pFrame->pThis = pThis;` |
|    16026 |   449 | `	pFrame->pVm = pVm;` |
|    16026 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16026 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16026 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16026 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16026 |   454 | `	return pFrame;` |
|     8014 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    15982 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    15984 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15984 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    15984 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    15984 |   476 | `	pVm->pFrame = pFrame;` |
|    15984 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13408 |   479 | `		*ppFrame = pFrame;` |
|     6703 |   480 | `	}` |
|    15984 |   481 | `	return SXRET_OK;` |
|     7993 |   482 |  |
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
|    13406 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13408 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13408 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13408 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13408 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13344 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    92744 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    79402 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39702 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13344 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    92800 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    79458 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39730 |   545 | `			}` |
|     6671 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13408 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13408 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13408 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13408 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13408 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6703 |   554 | `	}` |
|    13408 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6376962 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6377240 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6376964 |   566 | `	return pFrame;` |
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
|   111512 |   684 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   685 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   686 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	ph7_class_method *pMeth;` |
|        - |   690 | `	ph7_class_attr *pAttr;` |
|        - |   691 | `	SyHashEntry *pEntry;` |
|        - |   692 | `	sxi32 rc;` |
|        - |   693 | `	/* Reset the loop cursor */` |
|   111514 |   694 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   695 | `	/* Process only static and constant attribute */` |
|   437858 |   696 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   697 | `		/* Extract the current attribute */` |
|   270590 |   698 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   270590 |   699 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   111514 |   721 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   722 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   723 | `		 */` |
|    52450 |   724 | `		return SXRET_OK;` |
|        - |   725 | `	}` |
|        - |   726 | `	/* Create constructor alias if not yet done */` |
|    59066 |   727 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   728 | `		/* User constructor with the same base class name */` |
|     5210 |   729 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5210 |   730 | `		if( pEntry ){` |
|      ! 0 |   731 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   732 | `			/* Create the alias */` |
|      ! 0 |   733 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   734 | `		}` |
|     2604 |   735 | `	}` |
|        - |   736 | `	/* Install the methods now */` |
|    59066 |   737 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   588056 |   738 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   499460 |   739 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   499460 |   740 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   499452 |   741 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   499452 |   742 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   743 | `				return rc;` |
|        - |   744 | `			}` |
|   249725 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    59066 |   748 | `	pClass->bMounted = TRUE;` |
|    59066 |   749 | `	return SXRET_OK;` |
|    55758 |   750 |  |
|        - |   751 | `/*` |
|        - |   752 | ` * Allocate a private frame for attributes of the given` |
|        - |   753 | ` * class instance (Object in the PHP jargon).` |
|        - |   754 | ` */` |
|     1206 |   755 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   756 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   757 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   758 | `	)` |
|        2 |   759 |  |
|     1208 |   760 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   761 | `	ph7_class_attr *pAttr;` |
|        - |   762 | `	SyHashEntry *pEntry;` |
|        - |   763 | `	sxi32 rc;` |
|        - |   764 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1208 |   765 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4920 |   766 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   767 | `		VmClassAttr *pVmAttr;` |
|        - |   768 | `		/* Extract the current attribute */` |
|     3714 |   769 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3714 |   770 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3714 |   771 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   772 | `			return SXERR_MEM;` |
|        - |   773 | `		}` |
|     3714 |   774 | `		pVmAttr->pAttr = pAttr;` |
|     3714 |   775 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   776 | `			ph7_value *pMemObj;` |
|        - |   777 | `			/* Reserve a memory object for this attribute */` |
|     3708 |   778 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3708 |   779 | `			if( pMemObj == 0 ){` |
|      ! 0 |   780 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   781 | `				return SXERR_MEM;` |
|        - |   782 | `			}` |
|     3708 |   783 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3708 |   784 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   785 | `				/* Initialize attribute default value (any complex expression) */` |
|     1196 |   786 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      597 |   787 | `			}` |
|     3708 |   788 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3708 |   789 | `			if( rc != SXRET_OK ){` |
|        - |   790 | `				VmSlot sSlot;` |
|        - |   791 | `				/* Restore memory object */` |
|      ! 0 |   792 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   793 | `				sSlot.pUserData = 0;` |
|      ! 0 |   794 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `			/* Install attribute in the reference table */` |
|     3708 |   799 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1855 |   800 | `		}else{` |
|        - |   801 | `			/* Install static/constant attribute */` |
|        8 |   802 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   803 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   804 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   805 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   806 | `				return SXERR_MEM;` |
|        - |   807 | `			}` |
|        - |   808 | `		}` |
|        2 |   809 | `	}` |
|     1208 |   810 | `	return SXRET_OK;` |
|      605 |   811 |  |
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
|   366598 |   823 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   824 |  |
|        - |   825 | `	ph7_value *pObj;` |
|        - |   826 | `	sxi32 rc;` |
|   366600 |   827 | `	if( pIndex ){` |
|        - |   828 | `		/* Object index in the object table */` |
|   358872 |   829 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   179435 |   830 | `	}` |
|        - |   831 | `	/* Reserve a slot for the new object */` |
|   366600 |   832 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   366600 |   833 | `	if( rc != SXRET_OK ){` |
|        - |   834 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   835 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   836 | `		 */` |
|      ! 0 |   837 | `		return 0;` |
|        - |   838 | `	}` |
|   366600 |   839 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   366600 |   840 | `	return pObj;` |
|   183301 |   841 |  |
|        - |   842 | `/*` |
|        - |   843 | ` * Reserve a memory object.` |
|        - |   844 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   845 | ` */` |
|  2142010 |   846 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   847 |  |
|        - |   848 | `	ph7_value *pObj;` |
|        - |   849 | `	sxi32 rc;` |
|  2142012 |   850 | `	if( pIndex ){` |
|        - |   851 | `		/* Object index in the object table */` |
|  2142012 |   852 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071005 |   853 | `	}` |
|        - |   854 | `	/* Reserve a slot for the new object */` |
|  2142012 |   855 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2142012 |   856 | `	if( rc != SXRET_OK ){` |
|        - |   857 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   858 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   859 | `		 */` |
|      ! 0 |   860 | `		return 0;` |
|        - |   861 | `	}` |
|  2142012 |   862 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2142012 |   863 | `	return pObj;` |
|  1071007 |   864 |  |
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
|        - |   950 | `	"class ErrorException extends Exception { "\` |
|        - |   951 | `	"protected $severity;"\` |
|        - |   952 | `	"public function __construct(string $message = null,"\` |
|        - |   953 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   954 | `	"   if( isset($message) ){"\` |
|        - |   955 | `	"	  $this->message = $message;"\` |
|        - |   956 | `	"   }"\` |
|        - |   957 | `	"   $this->severity = $severity;"\` |
|        - |   958 | `	"   $this->code = $code;"\` |
|        - |   959 | `	"   $this->file = $filename;"\` |
|        - |   960 | `	"   $this->line = $lineno;"\` |
|        - |   961 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   962 | `	"   if( isset($previous) ){"\` |
|        - |   963 | `	"     $this->previous = $previous;"\` |
|        - |   964 | `	"   }"\` |
|        - |   965 | `	"}"\` |
|        - |   966 | `	"public function getSeverity(){"\` |
|        - |   967 | `	"   return $this->severity;"\` |
|        - |   968 | `    "}"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"interface Iterator {"\` |
|        - |   971 | `	"public function current();"\` |
|        - |   972 | `	"public function key();"\` |
|        - |   973 | `	"public function next();"\` |
|        - |   974 | `	"public function rewind();"\` |
|        - |   975 | `	"public function valid();"\` |
|        - |   976 | `	"}"\` |
|        - |   977 | `	"interface IteratorAggregate {"\` |
|        - |   978 | `	"public function getIterator();"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"interface Serializable {"\` |
|        - |   981 | `	"public function serialize();"\` |
|        - |   982 | `	"public function unserialize(string $serialized);"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"/* Directory releated IO */"\` |
|        - |   985 | `	"class Directory {"\` |
|        - |   986 | `	"public $handle = null;"\` |
|        - |   987 | `	"public $path  = null;"\` |
|        - |   988 | `	"public function __construct(string $path)"\` |
|        - |   989 | `	"{"\` |
|        - |   990 | `	"   $this->handle = opendir($path);"\` |
|        - |   991 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   992 | `	"      $this->path = $path;"\` |
|        - |   993 | `	"   }"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"public function __destruct()"\` |
|        - |   996 | `	"{"\` |
|        - |   997 | `	"  if( $this->handle != null ){"\` |
|        - |   998 | `	"       closedir($this->handle);"\` |
|        - |   999 | `	"  }"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"public function read()"\` |
|        - |  1002 | `	"{"\` |
|        - |  1003 | `	"    return readdir($this->handle);"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"public function rewind()"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"    rewinddir($this->handle);"\` |
|        - |  1008 | `	"}"\` |
|        - |  1009 | `	"public function close()"\` |
|        - |  1010 | `	"{"\` |
|        - |  1011 | `	"    closedir($this->handle);"\` |
|        - |  1012 | `	"    $this->handle = null;"\` |
|        - |  1013 | `	"}"\` |
|        - |  1014 | `	"}"\` |
|        - |  1015 | `	"class Fiber {"\` |
|        - |  1016 | `	"  private $__ctx;"\` |
|        - |  1017 | `	"  private $__callable;"\` |
|        - |  1018 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1019 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1020 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1021 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1022 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1023 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1024 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1025 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1026 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1027 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1028 | `	"}"\` |
|        - |  1029 | `	"class Generator implements Iterator {"\` |
|        - |  1030 | `	"  private $__ctx;"\` |
|        - |  1031 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1032 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1033 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1034 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1035 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1036 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1037 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1038 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1039 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1040 | `	"}"\` |
|        - |  1041 | `	"class stdClass{"\` |
|        - |  1042 | `	"  public $value;"\` |
|        - |  1043 | `	" /* Magic methods */"\` |
|        - |  1044 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1045 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1046 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1047 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1048 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"function dir(string $path){"\` |
|        - |  1051 | `	"   return new Directory($path);"\` |
|        - |  1052 | `	"}"\` |
|        - |  1053 | `	"function Dir(string $path){"\` |
|        - |  1054 | `	"   return new Directory($path);"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1057 | `    "{"\` |
|        - |  1058 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1059 | `	"  $aDir = array();"\` |
|        - |  1060 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1061 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1062 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1063 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1064 | `	"   }"\` |
|        - |  1065 | `	"  closedir($pHandle);"\` |
|        - |  1066 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1067 | `	"      rsort($aDir);"\` |
|        - |  1068 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1069 | `	"      sort($aDir);"\` |
|        - |  1070 | `	"  }"\` |
|        - |  1071 | `	"  return $aDir;"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1074 | `	"/* Open the target directory */"\` |
|        - |  1075 | `	"$zDir = dirname($pattern);"\` |
|        - |  1076 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1077 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1078 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1079 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1080 | `	"	return FALSE;"\` |
|        - |  1081 | `	"}"\` |
|        - |  1082 | `	"$pattern = basename($pattern);"\` |
|        - |  1083 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1084 | `	"/* Loop throw available entries */"\` |
|        - |  1085 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1086 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1087 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1088 | `	"	if( $rc ){"\` |
|        - |  1089 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1090 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1091 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1092 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1093 | `	"		  }"\` |
|        - |  1094 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1095 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1096 | `	"		 continue;"\` |
|        - |  1097 | `	"	   }"\` |
|        - |  1098 | `	"	   /* Add the entry */"\` |
|        - |  1099 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1100 | `	"	}"\` |
|        - |  1101 | `	" }"\` |
|        - |  1102 | `	"/* Close the handle */"\` |
|        - |  1103 | `	"closedir($pHandle);"\` |
|        - |  1104 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1105 | `	"  /* Sort the array */"\` |
|        - |  1106 | `	"  sort($pArray);"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1109 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1110 | `	"  $pArray[] = $pattern;"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"/* Return the created array */"\` |
|        - |  1113 | `	"return $pArray;"\` |
|        - |  1114 | `   "}"\` |
|        - |  1115 | `   "/* Creates a temporary file */"\` |
|        - |  1116 | `   "function tmpfile(){"\` |
|        - |  1117 | `   "  /* Extract the temp directory */"\` |
|        - |  1118 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1119 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1120 | `   "    /* Use the current dir */"\` |
|        - |  1121 | `   "    $zTempDir = '.';"\` |
|        - |  1122 | `   "  }"\` |
|        - |  1123 | `   "  /* Create the file */"\` |
|        - |  1124 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1125 | `   "  return $pHandle;"\` |
|        - |  1126 | `   "}"\` |
|        - |  1127 | `   "/* Creates a temporary filename */"\` |
|        - |  1128 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1129 | `   "{"\` |
|        - |  1130 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1131 | `   "}"\` |
|        - |  1132 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1133 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1134 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1135 | `   "/* Copy arguments */"\` |
|        - |  1136 | `   "$nArgs = func_num_args();"\` |
|        - |  1137 | `   "$pNew = array();"\` |
|        - |  1138 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1139 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1140 | `    "}"\` |
|        - |  1141 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1142 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1143 | `	"/* Erase */"\` |
|        - |  1144 | `	"array_erase($pArray);"\` |
|        - |  1145 | `	"/* Unshift */"\` |
|        - |  1146 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1147 | `	"return sizeof($pArray);"\` |
|        - |  1148 | `    "}"\` |
|        - |  1149 | `	"function array_merge_recursive(){"\` |
|        - |  1150 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1151 | `    "$arrays = func_get_args();"\` |
|        - |  1152 | `    "$narrays = count($arrays);"\` |
|        - |  1153 | `    "$ret = array();"\` |
|        - |  1154 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1155 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1156 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1157 | `	 " }"\` |
|        - |  1158 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1159 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1160 | `     "  if( $keyIsInt ) {"\` |
|        - |  1161 | `     "   $ret[] = $value;"\` |
|        - |  1162 | `     "  } else {"\` |
|        - |  1163 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1164 | `     "    $cur = $ret[$key];"\` |
|        - |  1165 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1166 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1167 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1168 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1169 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1170 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1171 | `     "    } else {"\` |
|        - |  1172 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1173 | `     "    }"\` |
|        - |  1174 | `     "   } else {"\` |
|        - |  1175 | `     "    $ret[$key] = $value;"\` |
|        - |  1176 | `     "   }"\` |
|        - |  1177 | `     "  }"\` |
|        - |  1178 | `     " }"\` |
|        - |  1179 | `	 " }"\` |
|        - |  1180 | `	 " return $ret;"\` |
|        - |  1181 | `    "}"\` |
|        - |  1182 | `	"function max(){"\` |
|        - |  1183 | `    "  $pArgs = func_get_args();"\` |
|        - |  1184 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1185 | `	"  return null;"\` |
|        - |  1186 | `    " }"\` |
|        - |  1187 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1188 | `    " $pArg = $pArgs[0];"\` |
|        - |  1189 | `	" if( !is_array($pArg) ){"\` |
|        - |  1190 | `	"   return $pArg; "\` |
|        - |  1191 | `	" }"\` |
|        - |  1192 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1193 | `	"   return null;"\` |
|        - |  1194 | `	" }"\` |
|        - |  1195 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1196 | `	" reset($pArg);"\` |
|        - |  1197 | `	" $max = current($pArg);"\` |
|        - |  1198 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1199 | `	"   if( $val > $max ){"\` |
|        - |  1200 | `	"     $max = $val;"\` |
|        - |  1201 | `    " }"\` |
|        - |  1202 | `	" }"\` |
|        - |  1203 | `	" return $max;"\` |
|        - |  1204 | `    " }"\` |
|        - |  1205 | `    " $max = $pArgs[0];"\` |
|        - |  1206 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1207 | `    " $val = $pArgs[$i];"\` |
|        - |  1208 | `	"if( $val > $max ){"\` |
|        - |  1209 | `	" $max = $val;"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `    " }"\` |
|        - |  1212 | `	" return $max;"\` |
|        - |  1213 | `    "}"\` |
|        - |  1214 | `	"function min(){"\` |
|        - |  1215 | `    "  $pArgs = func_get_args();"\` |
|        - |  1216 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1217 | `	"  return null;"\` |
|        - |  1218 | `    " }"\` |
|        - |  1219 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1220 | `    " $pArg = $pArgs[0];"\` |
|        - |  1221 | `	" if( !is_array($pArg) ){"\` |
|        - |  1222 | `	"   return $pArg; "\` |
|        - |  1223 | `	" }"\` |
|        - |  1224 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1225 | `	"   return null;"\` |
|        - |  1226 | `	" }"\` |
|        - |  1227 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1228 | `	" reset($pArg);"\` |
|        - |  1229 | `	" $min = current($pArg);"\` |
|        - |  1230 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1231 | `	"   if( $val < $min ){"\` |
|        - |  1232 | `	"     $min = $val;"\` |
|        - |  1233 | `    " }"\` |
|        - |  1234 | `	" }"\` |
|        - |  1235 | `	" return $min;"\` |
|        - |  1236 | `    " }"\` |
|        - |  1237 | `    " $min = $pArgs[0];"\` |
|        - |  1238 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1239 | `    " $val = $pArgs[$i];"\` |
|        - |  1240 | `	"if( $val < $min ){"\` |
|        - |  1241 | `	" $min = $val;"\` |
|        - |  1242 | `	" }"\` |
|        - |  1243 | `    " }"\` |
|        - |  1244 | `	" return $min;"\` |
|        - |  1245 | `	"}"\` |
|        - |  1246 | `	"function fileowner(string $file){"\` |
|        - |  1247 | `    " $a = stat($file);"\` |
|        - |  1248 | `	" if( !is_array($a) ){"\` |
|        - |  1249 | `	"	return false;"\` |
|        - |  1250 | `	" }"\` |
|        - |  1251 | `	" return $a['uid'];"\` |
|        - |  1252 | `    "}"\` |
|        - |  1253 | `    "function filegroup(string $file){"\` |
|        - |  1254 | `	" $a = stat($file);"\` |
|        - |  1255 | `	" if( !is_array($a) ){"\` |
|        - |  1256 | `	"	return false;"\` |
|        - |  1257 | `	" }"\` |
|        - |  1258 | `	" return $a['gid'];"\` |
|        - |  1259 | `    "}"\` |
|        - |  1260 | `	 "function fileinode(string $file){"\` |
|        - |  1261 | `	" $a = stat($file);"\` |
|        - |  1262 | `	" if( !is_array($a) ){"\` |
|        - |  1263 | `	"	return false;"\` |
|        - |  1264 | `	" }"\` |
|        - |  1265 | `	" return $a['ino'];"\` |
|        - |  1266 | `    "}"` |
|        - |  1267 |  |
|        - |  1268 | `/*` |
|        - |  1269 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1270 | ` * start compiling the target PHP program.` |
|        - |  1271 | ` */` |
|     2576 |  1272 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1273 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1274 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1275 | `	 )` |
|        2 |  1276 |  |
|        - |  1277 | `	SyString sBuiltin;` |
|        - |  1278 | `	ph7_value *pObj;` |
|        - |  1279 | `	sxi32 rc;` |
|        - |  1280 | `	/* Zero the structure */` |
|     2578 |  1281 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1282 | `	/* Initialize VM fields */` |
|     2578 |  1283 | `	pVm->pEngine = &(*pEngine);` |
|     2578 |  1284 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1285 | `	/* Instructions containers */` |
|     2578 |  1286 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2578 |  1287 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2578 |  1288 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1289 | `	/* Object containers */` |
|     2578 |  1290 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2578 |  1291 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1292 | `	/* Virtual machine internal containers */` |
|     2578 |  1293 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2578 |  1294 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2578 |  1295 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2578 |  1296 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2578 |  1297 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2578 |  1298 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2578 |  1299 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2578 |  1300 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2578 |  1301 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2578 |  1302 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2578 |  1303 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2578 |  1304 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2578 |  1305 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2578 |  1306 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2578 |  1307 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2578 |  1308 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2578 |  1309 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2578 |  1310 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2578 |  1311 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2578 |  1312 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2578 |  1313 | `	pVm->pPendingException = 0;` |
|        - |  1314 | `	/* Configuration containers */` |
|     2578 |  1315 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2578 |  1316 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2578 |  1317 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2578 |  1318 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2578 |  1319 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2578 |  1320 | `	pVm->iResponseStatus = 200;` |
|     2578 |  1321 | `	pVm->bHeadersSent = 0;` |
|     2578 |  1322 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1323 | `	/* Error callbacks containers */` |
|     2578 |  1324 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2578 |  1325 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2578 |  1326 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2578 |  1327 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2578 |  1328 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1329 | `	/* Set a default recursion limit */` |
|        - |  1330 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2578 |  1331 | `	pVm->nMaxDepth = 32;` |
|        - |  1332 | `#else` |
|        - |  1333 | `	pVm->nMaxDepth = 16;` |
|        - |  1334 | `#endif` |
|        - |  1335 | `	/* Default assertion flags */` |
|     2578 |  1336 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1337 | `	/* JSON return status */` |
|     2578 |  1338 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1339 | `	/* PRNG context */` |
|     2578 |  1340 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1341 | `	/* Install the null constant */` |
|     2578 |  1342 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2578 |  1343 | `	if( pObj == 0 ){` |
|      ! 0 |  1344 | `		rc = SXERR_MEM;` |
|      ! 0 |  1345 | `		goto Err;` |
|        - |  1346 | `	}` |
|     2578 |  1347 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1348 | `	/* Install the boolean TRUE constant */` |
|     2578 |  1349 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2578 |  1350 | `	if( pObj == 0 ){` |
|      ! 0 |  1351 | `		rc = SXERR_MEM;` |
|      ! 0 |  1352 | `		goto Err;` |
|        - |  1353 | `	}` |
|     2578 |  1354 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1355 | `	/* Install the boolean FALSE constant */` |
|     2578 |  1356 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2578 |  1357 | `	if( pObj == 0 ){` |
|      ! 0 |  1358 | `		rc = SXERR_MEM;` |
|      ! 0 |  1359 | `		goto Err;` |
|        - |  1360 | `	}` |
|     2578 |  1361 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1362 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1363 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1364 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2578 |  1365 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2578 |  1366 | `	if( pObj == 0 ){` |
|      ! 0 |  1367 | `		rc = SXERR_MEM;` |
|      ! 0 |  1368 | `		goto Err;` |
|        - |  1369 | `	}` |
|     2578 |  1370 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1371 | `	/* Create the global frame */` |
|     2578 |  1372 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2578 |  1373 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1374 | `		goto Err;` |
|        - |  1375 | `	}` |
|        - |  1376 | `	/* Initialize the code generator */` |
|     2578 |  1377 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2578 |  1378 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1379 | `		goto Err;` |
|        - |  1380 | `	}` |
|        - |  1381 | `	/* VM correctly initialized,set the magic number */` |
|     2578 |  1382 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2578 |  1383 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1384 | `	/* Compile the built-in library */` |
|     2578 |  1385 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1386 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2578 |  1387 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1388 | `	/* Register Fiber internal C functions */` |
|     2578 |  1389 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2578 |  1390 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2578 |  1391 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2578 |  1392 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2578 |  1393 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2578 |  1394 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2578 |  1395 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2578 |  1396 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2578 |  1397 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2578 |  1398 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1399 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2578 |  1400 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2578 |  1401 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2578 |  1402 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2578 |  1403 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2578 |  1404 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2578 |  1405 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2578 |  1406 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2578 |  1407 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2578 |  1408 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2578 |  1409 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1410 | `	/* Reset the code generator */` |
|     2578 |  1411 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2578 |  1412 | `	return SXRET_OK;` |
|      ! 0 |  1413 | `Err:` |
|      ! 0 |  1414 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1415 | `	return rc;` |
|     1290 |  1416 |  |
|        - |  1417 | `/*` |
|        - |  1418 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1419 | ` * routine which store the output in an internal blob.` |
|        - |  1420 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1421 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1422 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1423 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1424 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1425 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1426 | ` * to finish executing and extracting the output.` |
|        - |  1427 | ` */` |
|       38 |  1428 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1429 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1430 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1431 | `	void *pUserData     /* User private data */` |
|        - |  1432 | `	)` |
|      ! 0 |  1433 |  |
|        - |  1434 | `	 sxi32 rc;` |
|        - |  1435 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1436 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1437 | `	 return rc;` |
|      ! 0 |  1438 |  |
|        - |  1439 | `/*` |
|        - |  1440 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1441 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1442 | ` */` |
|    14108 |  1443 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1444 |  |
|    14110 |  1445 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14110 |  1446 | `	if( xCons != VmObConsumer ){` |
|     6260 |  1447 | `		pVm->nOutputLen += nLen;` |
|     6260 |  1448 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      820 |  1449 | `			pVm->bHeadersSent = 1;` |
|      409 |  1450 | `		}` |
|     3129 |  1451 | `	}` |
|    14110 |  1452 |  |
|        - |  1453 | `#define VM_STACK_GUARD 16` |
|        - |  1454 | `/*` |
|        - |  1455 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1456 | ` * our compiled PHP program.` |
|        - |  1457 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1458 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1459 | ` */` |
|    32890 |  1460 | `static ph7_value * VmNewOperandStack(` |
|        - |  1461 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1462 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1463 | `	)` |
|        2 |  1464 |  |
|        - |  1465 | `	ph7_value *pStack;` |
|        - |  1466 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1467 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1468 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1469 | `  ** on the maximum stack depth required.` |
|        - |  1470 | `  **` |
|        - |  1471 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1472 | `  */` |
|    32892 |  1473 | `	nInstr += VM_STACK_GUARD;` |
|    32892 |  1474 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32892 |  1475 | `	if( pStack == 0 ){` |
|      ! 0 |  1476 | `		return 0;` |
|        - |  1477 | `	}` |
|        - |  1478 | `	/* Initialize the operand stack */` |
|  2058308 |  1479 | `	while( nInstr > 0 ){` |
|  2025418 |  1480 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2025418 |  1481 | `		--nInstr;` |
|        2 |  1482 | `	}` |
|        - |  1483 | `	/* Ready for bytecode execution */` |
|    32892 |  1484 | `	return pStack;` |
|    16447 |  1485 |  |
|        - |  1486 | `/* Forward declaration */` |
|        - |  1487 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1488 | `/*` |
|        - |  1489 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1490 | ` * This routine gets called by the PH7 engine after` |
|        - |  1491 | ` * successful compilation of the target PHP program.` |
|        - |  1492 | ` */` |
|     2316 |  1493 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1494 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1495 | `	)` |
|        2 |  1496 |  |
|        - |  1497 | `	SyHashEntry *pEntry;` |
|        - |  1498 | `	sxi32 rc;` |
|     2318 |  1499 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1500 | `		/* Initialize your VM first */` |
|      ! 0 |  1501 | `		return SXERR_CORRUPT;` |
|        - |  1502 | `	}` |
|        - |  1503 | `	/* Mark the VM ready for byte-code execution */` |
|     2318 |  1504 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1505 | `	/* Release the code generator now we have compiled our program */` |
|     2318 |  1506 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1507 | `	/* Emit the DONE instruction */` |
|     2318 |  1508 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2318 |  1509 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1510 | `		return SXERR_MEM;` |
|        - |  1511 | `	}` |
|        - |  1512 | `	/* Script return value */` |
|     2318 |  1513 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1514 | `	/* Allocate a new operand stack */` |
|     2318 |  1515 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2318 |  1516 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1517 | `		return SXERR_MEM;` |
|        - |  1518 | `	}` |
|        - |  1519 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1520 | `	 * private data. */` |
|     2318 |  1521 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2318 |  1522 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1523 | `	/* Allocate the reference table */` |
|     2318 |  1524 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2318 |  1525 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2318 |  1526 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1527 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1528 | `		return SXERR_MEM;` |
|        - |  1529 | `	}` |
|        - |  1530 | `	/* Zero the reference table */` |
|     2318 |  1531 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1532 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2318 |  1533 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2318 |  1534 | `	if( rc != SXRET_OK ){` |
|        - |  1535 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1536 | `		return rc;` |
|        - |  1537 | `	}` |
|        - |  1538 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2318 |  1539 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2318 |  1540 | `	if( rc != SXRET_OK ){` |
|        - |  1541 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1542 | `		return rc;` |
|        - |  1543 | `	}` |
|        - |  1544 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2318 |  1545 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1546 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2318 |  1547 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1548 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2318 |  1549 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1550 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1551 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2318 |  1552 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2318 |  1553 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1554 | `#endif` |
|        - |  1555 | `	/* Initialize and install static and constants class attributes */` |
|     2318 |  1556 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    37234 |  1557 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    34918 |  1558 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    34918 |  1559 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1560 | `			return rc;` |
|        - |  1561 | `		}` |
|        2 |  1562 | `	}` |
|        - |  1563 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2318 |  1564 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1565 | `	/* VM is ready for bytecode execution */` |
|     2318 |  1566 | `	return SXRET_OK;` |
|     1160 |  1567 |  |
|        - |  1568 | `/*` |
|        - |  1569 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1570 | ` */` |
|      ! 0 |  1571 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1572 |  |
|      ! 0 |  1573 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1574 | `		return SXERR_CORRUPT;` |
|        - |  1575 | `	}` |
|        - |  1576 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1577 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1578 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1579 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1580 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1581 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1582 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1583 | `	pVm->bHttpContext = 0;` |
|        - |  1584 | `	/* Set the ready flag */` |
|      ! 0 |  1585 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1586 | `	return SXRET_OK;` |
|      ! 0 |  1587 |  |
|        - |  1588 | `/*` |
|        - |  1589 | ` * Release a Virtual Machine.` |
|        - |  1590 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1591 | ` */` |
|     2308 |  1592 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1593 |  |
|        - |  1594 | `	/* Set the stale magic number */` |
|     2310 |  1595 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1596 | `	/* Release the private memory subsystem */` |
|     2310 |  1597 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2310 |  1598 | `	return SXRET_OK;` |
|        2 |  1599 |  |
|        - |  1600 | `/*` |
|        - |  1601 | ` * Initialize a foreign function call context.` |
|        - |  1602 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1603 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1604 | ` * functions.` |
|        - |  1605 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1606 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1607 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1608 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1609 | ` */` |
|   580034 |  1610 | `static sxi32 VmInitCallContext(` |
|        - |  1611 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1612 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1613 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1614 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1615 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1616 | `	)` |
|        2 |  1617 |  |
|   580036 |  1618 | `	pOut->pFunc = pFunc;` |
|   580036 |  1619 | `	pOut->pVm   = pVm;` |
|   580036 |  1620 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   580036 |  1621 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1622 | `	/* Assume a null return value */` |
|   580036 |  1623 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   580036 |  1624 | `	pOut->pRet = pRet;` |
|   580036 |  1625 | `	pOut->iFlags = iFlags;` |
|   580036 |  1626 | `	return SXRET_OK;` |
|        2 |  1627 |  |
|        - |  1628 | `/*` |
|        - |  1629 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1630 | ` * left behind.` |
|        - |  1631 | ` */` |
|   580034 |  1632 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1633 |  |
|        - |  1634 | `	sxu32 n;` |
|   580036 |  1635 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7056 |  1636 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20134 |  1637 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13080 |  1638 | `			if( apObj[n] == 0 ){` |
|        - |  1639 | `				/* Already released */` |
|      298 |  1640 | `				continue;` |
|        - |  1641 | `			}` |
|    12784 |  1642 | `			PH7_MemObjRelease(apObj[n]);` |
|    12784 |  1643 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6393 |  1644 | `		}` |
|     7056 |  1645 | `		SySetRelease(&pCtx->sVar);` |
|     3527 |  1646 | `	}` |
|   580036 |  1647 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1648 | `		ph7_aux_data *aAux;` |
|        - |  1649 | `		void *pChunk;` |
|        - |  1650 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1651 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1652 | `		 */` |
|        9 |  1653 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1654 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1655 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1656 | `			/* Release the chunk */` |
|       25 |  1657 | `			if( pChunk ){` |
|       25 |  1658 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1659 | `			}` |
|       13 |  1660 | `		}` |
|        9 |  1661 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1662 | `	}` |
|   580036 |  1663 |  |
|        - |  1664 | `/*` |
|        - |  1665 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1666 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1667 | ` */` |
|      296 |  1668 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1669 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1670 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1671 | `	)` |
|        2 |  1672 |  |
|      298 |  1673 | `	if( pValue == 0 ){` |
|        - |  1674 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1675 | `		return;` |
|        - |  1676 | `	}` |
|      298 |  1677 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1678 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1679 | `		sxu32 n;` |
|     1054 |  1680 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1681 | `			if( apObj[n] == pValue ){` |
|      298 |  1682 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1683 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1684 | `				/* Mark as released */` |
|      298 |  1685 | `				apObj[n] = 0;` |
|      298 |  1686 | `				break;` |
|        - |  1687 | `			}` |
|      380 |  1688 | `		}` |
|      148 |  1689 | `	}` |
|      150 |  1690 |  |
|        - |  1691 | `/*` |
|        - |  1692 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1693 | ` */` |
|  3353488 |  1694 | `static void VmPopOperand(` |
|        - |  1695 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1696 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1697 | `	)` |
|        2 |  1698 |  |
|  3353490 |  1699 | `	ph7_value *pTos = *ppTos;` |
|  7130066 |  1700 | `	while( nPop > 0 ){` |
|  3776578 |  1701 | `		PH7_MemObjRelease(pTos);` |
|  3776578 |  1702 | `		pTos--;` |
|  3776578 |  1703 | `		nPop--;` |
|        2 |  1704 | `	}` |
|        - |  1705 | `	/* Top of the stack */` |
|  3353490 |  1706 | `	*ppTos = pTos;` |
|  3353490 |  1707 |  |
|        - |  1708 | `/*` |
|        - |  1709 | ` * Reserve a memory object.` |
|        - |  1710 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1711 | ` */` |
|  3058150 |  1712 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1713 |  |
|  3058152 |  1714 | `	ph7_value *pObj = 0;` |
|        - |  1715 | `	VmSlot *pSlot;` |
|        - |  1716 | `	sxu32 nIdx;` |
|        - |  1717 | `	/* Check for a free slot */` |
|  3058152 |  1718 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3058152 |  1719 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3058152 |  1720 | `	if( pSlot ){` |
|   916142 |  1721 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   916142 |  1722 | `		nIdx = pSlot->nIdx;` |
|   458070 |  1723 | `	}` |
|  3058152 |  1724 | `	if( pObj == 0 ){` |
|        - |  1725 | `		/* Reserve a new memory object */` |
|  2142012 |  1726 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2142012 |  1727 | `		if( pObj == 0 ){` |
|      ! 0 |  1728 | `			return 0;` |
|        - |  1729 | `		}` |
|  1071005 |  1730 | `	}` |
|        - |  1731 | `	/* Set a null default value */` |
|  3058152 |  1732 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3058152 |  1733 | `	pObj->nIdx = nIdx;` |
|  3058152 |  1734 | `	return pObj;` |
|  1529077 |  1735 |  |
|        - |  1736 | `/*` |
|        - |  1737 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1738 | ` */` |
|    30012 |  1739 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1740 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1741 | `	const char *zKey,  /* Entry key */` |
|        - |  1742 | `	sxu32 nByte,       /* Key length */` |
|        - |  1743 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1744 | `	)` |
|        2 |  1745 |  |
|        - |  1746 | `	ph7_value sKey;` |
|        - |  1747 | `	sxi32 rc;` |
|    30014 |  1748 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30014 |  1749 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1750 | `	/* Perform the insertion */` |
|    30014 |  1751 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30014 |  1752 | `	PH7_MemObjRelease(&sKey);` |
|    30014 |  1753 | `	return rc;` |
|        2 |  1754 |  |
|        - |  1755 | `/*` |
|        - |  1756 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1757 | ` * Return a pointer to the variable value on success.` |
|        - |  1758 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1759 | ` */` |
|  3122836 |  1760 | `static ph7_value * VmExtractMemObj(` |
|        - |  1761 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1762 | `	const SyString *pName, /* Variable name */` |
|        - |  1763 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1764 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1765 | `	)` |
|        2 |  1766 |  |
|  3122838 |  1767 | `	int bNullify = FALSE;` |
|        - |  1768 | `	SyHashEntry *pEntry;` |
|        - |  1769 | `	VmFrame *pFrame;` |
|        - |  1770 | `	ph7_value *pObj;` |
|        - |  1771 | `	sxu32 nIdx;` |
|        - |  1772 | `	sxi32 rc;` |
|        - |  1773 | `	/* Point to the top active frame */` |
|  3122838 |  1774 | `	pFrame = pVm->pFrame;` |
|  3122838 |  1775 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1776 | `	/* Perform the lookup */` |
|  3122838 |  1777 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1778 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1779 | `		pName = &sAnnon;` |
|        - |  1780 | `		/* Always nullify the object */` |
|      ! 0 |  1781 | `		bNullify = TRUE;` |
|      ! 0 |  1782 | `		bDup = FALSE;` |
|      ! 0 |  1783 | `	}` |
|        - |  1784 | `	/* Check the superglobals table first */` |
|  3122838 |  1785 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3122838 |  1786 | `	if( pEntry == 0 ){` |
|        - |  1787 | `		/* Query the top active frame */` |
|  3122798 |  1788 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3122798 |  1789 | `		if( pEntry == 0 ){` |
|    86300 |  1790 | `			char *zName = (char *)pName->zString;` |
|        - |  1791 | `			VmSlot sLocal;` |
|    86300 |  1792 | `			if( !bCreate ){` |
|        - |  1793 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1794 | `				return 0;` |
|        - |  1795 | `			}` |
|        - |  1796 | `			/* No such variable,automatically create a new one and install` |
|        - |  1797 | `			 * it in the current frame.` |
|        - |  1798 | `			 */` |
|    86264 |  1799 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    86264 |  1800 | `			if( pObj == 0 ){` |
|      ! 0 |  1801 | `				return 0;` |
|        - |  1802 | `			}` |
|    86264 |  1803 | `			nIdx = pObj->nIdx;` |
|    86264 |  1804 | `			if( bDup ){` |
|        - |  1805 | `				/* Duplicate name */` |
|      168 |  1806 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1807 | `				if( zName == 0 ){` |
|      ! 0 |  1808 | `					return 0;` |
|        - |  1809 | `				}` |
|       83 |  1810 | `			}` |
|        - |  1811 | `			/* Link to the top active VM frame */` |
|    86264 |  1812 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    86264 |  1813 | `			if( rc != SXRET_OK ){` |
|        - |  1814 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1815 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1816 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1817 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1818 | `				return 0;` |
|        - |  1819 | `			}` |
|    86264 |  1820 | `			if( pFrame->pParent != 0 ){` |
|        - |  1821 | `				/* Local variable */` |
|    79438 |  1822 | `				sLocal.nIdx = nIdx;` |
|    79438 |  1823 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39720 |  1824 | `			}else{` |
|        - |  1825 | `				/* Register in the $GLOBALS array */` |
|     6828 |  1826 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1827 | `			}` |
|        - |  1828 | `			/* Install in the reference table */` |
|    86264 |  1829 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1830 | `			/* Save object index */` |
|    86264 |  1831 | `			pObj->nIdx = nIdx;` |
|    43133 |  1832 | `		}else{` |
|        - |  1833 | `			/* Extract variable contents */` |
|  3036500 |  1834 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3036500 |  1835 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3036500 |  1836 | `			if( bNullify && pObj ){` |
|      ! 0 |  1837 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1838 | `			}` |
|        - |  1839 | `		}` |
|  1561492 |  1840 | `	}else{` |
|        - |  1841 | `		/* Superglobal */` |
|       42 |  1842 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1843 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1844 | `	}` |
|  3122802 |  1845 | `	return pObj;` |
|  1561530 |  1846 |  |
|        - |  1847 | `/*` |
|        - |  1848 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1849 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1850 | ` */` |
|     2620 |  1851 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1852 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1853 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1854 | `	sxu32 nByte        /* zName length */` |
|        - |  1855 | `	)` |
|        2 |  1856 |  |
|        - |  1857 | `	SyHashEntry *pEntry;` |
|        - |  1858 | `	ph7_value *pValue;` |
|        - |  1859 | `	sxu32 nIdx;` |
|        - |  1860 | `	/* Query the superglobal table */` |
|     2622 |  1861 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2622 |  1862 | `	if( pEntry == 0 ){` |
|        - |  1863 | `		/* No such entry */` |
|      ! 0 |  1864 | `		return 0;` |
|        - |  1865 | `	}` |
|        - |  1866 | `	/* Extract the superglobal index in the global object pool */` |
|     2622 |  1867 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1868 | `	/* Extract the variable value  */` |
|     2622 |  1869 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2622 |  1870 | `	return pValue;` |
|     1312 |  1871 |  |
|        - |  1872 | `/*` |
|        - |  1873 | ` * Perform a raw hashmap insertion.` |
|        - |  1874 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1875 | ` */` |
|     2650 |  1876 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1877 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1878 | `	const char *zKey,   /* Entry key */` |
|        - |  1879 | `	int nKeylen,        /* zKey length*/` |
|        - |  1880 | `	const char *zData,  /* Entry data */` |
|        - |  1881 | `	int nLen            /* zData length */` |
|        - |  1882 | `	)` |
|        2 |  1883 |  |
|        - |  1884 | `	ph7_value sKey,sValue;` |
|        - |  1885 | `	sxi32 rc;` |
|     2652 |  1886 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2652 |  1887 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2652 |  1888 | `	if( zKey ){` |
|     2630 |  1889 | `		if( nKeylen < 0 ){` |
|     2578 |  1890 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1288 |  1891 | `		}` |
|     2630 |  1892 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1314 |  1893 | `	}` |
|     2652 |  1894 | `	if( zData ){` |
|     2652 |  1895 | `		if( nLen < 0 ){` |
|        - |  1896 | `			/* Compute length automatically */` |
|      144 |  1897 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1898 | `		}` |
|     2652 |  1899 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1325 |  1900 | `	}` |
|        - |  1901 | `	/* Perform the insertion */` |
|     2652 |  1902 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2652 |  1903 | `	PH7_MemObjRelease(&sKey);` |
|     2652 |  1904 | `	PH7_MemObjRelease(&sValue);` |
|     2652 |  1905 | `	return rc;` |
|        2 |  1906 |  |
|        - |  1907 | `/*` |
|        - |  1908 | ` * Configure a working virtual machine instance.` |
|        - |  1909 | ` *` |
|        - |  1910 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1911 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1912 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1913 | ` * The second argument to this function is an integer configuration option` |
|        - |  1914 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1915 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1916 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1917 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1918 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1919 | ` */` |
|    37386 |  1920 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1921 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1922 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1923 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1924 | `	)` |
|        2 |  1925 |  |
|    37388 |  1926 | `	sxi32 rc = SXRET_OK;` |
|    37388 |  1927 | `	switch(nOp){` |
|     1150 |  1928 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2302 |  1929 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2302 |  1930 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1931 | `		/* VM output consumer callback */` |
|        - |  1932 | `#ifdef UNTRUST` |
|        - |  1933 | `		if( xConsumer == 0 ){` |
|        - |  1934 | `			rc = SXERR_CORRUPT;` |
|        - |  1935 | `			break;` |
|        - |  1936 | `		}` |
|        - |  1937 | `#endif` |
|        - |  1938 | `		/* Install the output consumer */` |
|     2302 |  1939 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2302 |  1940 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2302 |  1941 | `		break;` |
|        - |  1942 | `							   }` |
|     1158 |  1943 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1944 | `		/* Import path */` |
|        - |  1945 | `		  const char *zPath;` |
|        - |  1946 | `		  SyString sPath;` |
|     2318 |  1947 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1948 | `#if defined(UNTRUST)` |
|        - |  1949 | `		  if( zPath == 0 ){` |
|        - |  1950 | `			  rc = SXERR_EMPTY;` |
|        - |  1951 | `			  break;` |
|        - |  1952 | `		  }` |
|        - |  1953 | `#endif` |
|     2318 |  1954 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1955 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1956 | `#ifdef __WINNT__` |
|        2 |  1957 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1958 | `#endif` |
|     4634 |  1959 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1960 | `		  /* Remove leading and trailing white spaces */` |
|     2318 |  1961 | `		  SyStringFullTrim(&sPath);` |
|     2318 |  1962 | `		  if( sPath.nByte > 0 ){` |
|        - |  1963 | `			  /* Store the path in the corresponding conatiner */` |
|     2318 |  1964 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1158 |  1965 | `		  }` |
|     2318 |  1966 | `		  break;` |
|        - |  1967 | `									 }` |
|     1158 |  1968 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1969 | `		/* Run-Time Error report */` |
|     2318 |  1970 | `		pVm->bErrReport = 1;` |
|     2318 |  1971 | `		break;` |
|      ! 0 |  1972 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1973 | `		/* Recursion depth */` |
|      ! 0 |  1974 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1975 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1976 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1977 | `		}` |
|      ! 0 |  1978 | `		break;` |
|        - |  1979 | `									   }` |
|      ! 0 |  1980 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1981 | `		/* VM output length in bytes */` |
|      ! 0 |  1982 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1983 | `#ifdef UNTRUST` |
|        - |  1984 | `		if( pOut == 0 ){` |
|        - |  1985 | `			rc = SXERR_CORRUPT;` |
|        - |  1986 | `			break;` |
|        - |  1987 | `		}` |
|        - |  1988 | `#endif` |
|      ! 0 |  1989 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1990 | `		break;` |
|        - |  1991 | `							   }` |
|        - |  1992 |  |
|    11580 |  1993 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1994 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1995 | `		/* Create a new superglobal/global variable */` |
|    23162 |  1996 | `		const char *zName = va_arg(ap,const char *);` |
|    23162 |  1997 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1998 | `		SyHashEntry *pEntry;` |
|        - |  1999 | `		ph7_value *pObj;` |
|        - |  2000 | `		sxu32 nByte;` |
|        - |  2001 | `		sxu32 nIdx;` |
|        - |  2002 | `#ifdef UNTRUST` |
|        - |  2003 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2004 | `			rc = SXERR_CORRUPT;` |
|        - |  2005 | `			break;` |
|        - |  2006 | `		}` |
|        - |  2007 | `#endif` |
|    23162 |  2008 | `		nByte = SyStrlen(zName);` |
|    23162 |  2009 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2010 | `			/* Check if the superglobal is already installed */` |
|    23162 |  2011 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11582 |  2012 | `		}else{` |
|        - |  2013 | `			/* Query the top active VM frame */` |
|      ! 0 |  2014 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2015 | `		}` |
|    23162 |  2016 | `		if( pEntry ){` |
|        - |  2017 | `			/* Variable already installed */` |
|      ! 0 |  2018 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2019 | `			/* Extract contents */` |
|      ! 0 |  2020 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2021 | `			if( pObj ){` |
|        - |  2022 | `				/* Overwrite old contents */` |
|      ! 0 |  2023 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2024 | `			}` |
|      ! 0 |  2025 | `		}else{` |
|        - |  2026 | `			/* Install a new variable */` |
|    23162 |  2027 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23162 |  2028 | `			if( pObj == 0 ){` |
|      ! 0 |  2029 | `				rc = SXERR_MEM;` |
|      ! 0 |  2030 | `				break;` |
|        - |  2031 | `			}` |
|    23162 |  2032 | `			nIdx = pObj->nIdx;` |
|        - |  2033 | `			/* Copy value */` |
|    23162 |  2034 | `			PH7_MemObjStore(pValue,pObj);` |
|    23162 |  2035 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2036 | `				/* Install the superglobal */` |
|    23162 |  2037 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11582 |  2038 | `			}else{` |
|        - |  2039 | `				/* Install in the current frame */` |
|      ! 0 |  2040 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2041 | `			}` |
|    23162 |  2042 | `			if( rc == SXRET_OK ){` |
|        - |  2043 | `				SyHashEntry *pRef;` |
|    23162 |  2044 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23162 |  2045 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11582 |  2046 | `				}else{` |
|      ! 0 |  2047 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2048 | `				}` |
|        - |  2049 | `				/* Install in the reference table */` |
|    23162 |  2050 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23162 |  2051 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2052 | `					/* Register in the $GLOBALS array */` |
|    23162 |  2053 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11580 |  2054 | `				}` |
|    11580 |  2055 | `			}` |
|        - |  2056 | `		}` |
|    23162 |  2057 | `		break;` |
|        - |  2058 | `									}` |
|     1288 |  2059 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2060 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2061 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2062 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2063 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2065 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2578 |  2066 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2578 |  2067 | `		const char *zValue = va_arg(ap,const char *);` |
|     2578 |  2068 | `		int nLen = va_arg(ap,int);` |
|        - |  2069 | `		ph7_hashmap *pMap;` |
|        - |  2070 | `		ph7_value *pValue;` |
|     2578 |  2071 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2072 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2073 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2577 |  2074 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2075 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2076 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2576 |  2077 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2078 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2079 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2576 |  2080 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2081 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2082 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2576 |  2083 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2084 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2085 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2576 |  2086 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2087 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2088 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2089 | `		}else{` |
|        - |  2090 | `			/* Extract the $_SERVER superglobal */` |
|     2576 |  2091 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2092 | `		}` |
|     2578 |  2093 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2094 | `			/* No such entry */` |
|      ! 0 |  2095 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2096 | `			break;` |
|        - |  2097 | `		}` |
|        - |  2098 | `		/* Point to the hashmap */` |
|     2578 |  2099 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2100 | `		/* Perform the insertion */` |
|     2578 |  2101 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2578 |  2102 | `		break;` |
|        - |  2103 | `								   }` |
|       11 |  2104 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2105 | `		/* Script arguments */` |
|       24 |  2106 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2107 | `		ph7_hashmap *pMap;` |
|        - |  2108 | `		ph7_value *pValue;` |
|        - |  2109 | `		sxu32 n;` |
|       24 |  2110 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2111 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2112 | `			break;` |
|        - |  2113 | `		}` |
|        - |  2114 | `		/* Extract the $argv array */` |
|       24 |  2115 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2116 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2117 | `			/* No such entry */` |
|      ! 0 |  2118 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2119 | `			break;` |
|        - |  2120 | `		}` |
|        - |  2121 | `		/* Point to the hashmap */` |
|       24 |  2122 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2123 | `		/* Perform the insertion */` |
|       24 |  2124 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2125 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2126 | `		if( rc == SXRET_OK ){` |
|       24 |  2127 | `			if( pMap->nEntry > 1 ){` |
|        - |  2128 | `				/* Append space separator first */` |
|       18 |  2129 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2130 | `			}` |
|       24 |  2131 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2132 | `		}` |
|       24 |  2133 | `		break;` |
|        - |  2134 | `								  }` |
|      ! 0 |  2135 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2136 | `		/* error_log() consumer */` |
|      ! 0 |  2137 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2138 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2139 | `		break;` |
|        - |  2140 | `										}` |
|      ! 0 |  2141 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2142 | `		/* Script return value */` |
|      ! 0 |  2143 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2144 | `#ifdef UNTRUST` |
|        - |  2145 | `		if( ppValue == 0 ){` |
|        - |  2146 | `			rc = SXERR_CORRUPT;` |
|        - |  2147 | `			break;` |
|        - |  2148 | `		}` |
|        - |  2149 | `#endif` |
|      ! 0 |  2150 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2151 | `		break;` |
|        - |  2152 | `								   }` |
|     2316 |  2153 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2154 | `		/* Register an IO stream device */` |
|     4634 |  2155 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2156 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6948 |  2157 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4634 |  2158 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2159 | `				/* Invalid stream */` |
|      ! 0 |  2160 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2161 | `				break;` |
|        - |  2162 | `		}` |
|     4634 |  2163 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2164 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2318 |  2165 | `			pVm->pDefStream = pStream;` |
|     1158 |  2166 | `		}` |
|        - |  2167 | `		/* Insert in the appropriate container */` |
|     4634 |  2168 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4634 |  2169 | `		break;` |
|        - |  2170 | `								  }` |
|        8 |  2171 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2172 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2173 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2174 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2175 | `#ifdef UNTRUST` |
|        - |  2176 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2177 | `			rc = SXERR_CORRUPT;` |
|        - |  2178 | `			break;` |
|        - |  2179 | `		}` |
|        - |  2180 | `#endif` |
|       16 |  2181 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2182 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2183 | `		break;` |
|        - |  2184 | `									   }` |
|        8 |  2185 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2186 | `		/* Raw HTTP request*/` |
|       16 |  2187 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2188 | `		int nByte = va_arg(ap,int);` |
|       16 |  2189 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2190 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2191 | `			break;` |
|        - |  2192 | `		}` |
|       16 |  2193 | `		if( nByte < 0 ){` |
|        - |  2194 | `			/* Compute length automatically */` |
|      ! 0 |  2195 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2196 | `		}` |
|        - |  2197 | `		/* Process the request */` |
|       16 |  2198 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2199 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2200 | `		if( rc == SXRET_OK ){` |
|       16 |  2201 | `			pVm->bHttpContext = 1;` |
|        8 |  2202 | `		}` |
|       16 |  2203 | `		break;` |
|        - |  2204 | `									}` |
|        8 |  2205 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2206 | `		/* Extract HTTP response status code */` |
|       16 |  2207 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2208 | `		if( pStatus ){` |
|       16 |  2209 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2210 | `		}` |
|       16 |  2211 | `		break;` |
|        - |  2212 | `										}` |
|        8 |  2213 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2214 | `		/* Iterate response headers via callback */` |
|        - |  2215 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2216 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2217 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2218 | `		if( xCallback ){` |
|       16 |  2219 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2220 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2221 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2222 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2223 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2224 | `							   pUserData);` |
|       12 |  2225 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2226 | `					break;` |
|        - |  2227 | `				}` |
|        6 |  2228 | `			}` |
|        8 |  2229 | `		}` |
|       16 |  2230 | `		break;` |
|        - |  2231 | `										 }` |
|      ! 0 |  2232 | `	default:` |
|        - |  2233 | `		/* Unknown configuration option */` |
|      ! 0 |  2234 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2235 | `		break;` |
|        - |  2236 | `	}` |
|    37388 |  2237 | `	return rc;` |
|        2 |  2238 |  |
|        - |  2239 | `/* Forward declaration */` |
|        - |  2240 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2241 | `/*` |
|        - |  2242 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2243 | ` * format.` |
|        - |  2244 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2245 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2246 | ` * (STDOUT).` |
|        - |  2247 | ` */` |
|        2 |  2248 | `static sxi32 VmByteCodeDump(` |
|        - |  2249 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2250 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2251 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2252 | `	)` |
|        1 |  2253 |  |
|        - |  2254 | `	static const char zDump[] = {` |
|        - |  2255 | `		"====================================================\n"` |
|        - |  2256 | `		"PH7 VM Dump\n"` |
|        - |  2257 | `		"====================================================\n"` |
|        - |  2258 | `	};` |
|        - |  2259 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2260 | `	sxi32 rc = SXRET_OK;` |
|        - |  2261 | `	sxu32 n;` |
|        - |  2262 | `	/* Point to the PH7 instructions */` |
|        3 |  2263 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2264 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2265 | `	n = 0;` |
|        3 |  2266 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2267 | `	/* Dump instructions */` |
|        7 |  2268 | `	for(;;){` |
|       15 |  2269 | `		if( pInstr >= pEnd ){` |
|        - |  2270 | `			/* No more instructions */` |
|        3 |  2271 | `			break;` |
|        - |  2272 | `		}` |
|        - |  2273 | `		/* Format and call the consumer callback */` |
|       19 |  2274 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2275 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2276 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2277 | `		if( rc != SXRET_OK ){` |
|        - |  2278 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2279 | `			return rc;` |
|        - |  2280 | `		}` |
|       13 |  2281 | `		++n;` |
|       13 |  2282 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2283 | `	}` |
|        3 |  2284 | `	return rc;` |
|        2 |  2285 |  |
|        - |  2286 | `/* Forward declaration */` |
|        - |  2287 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2288 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2289 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2290 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2291 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2292 | `/*` |
|        - |  2293 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2294 | ` * consumer callback.` |
|        - |  2295 | ` */` |
|      544 |  2296 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2297 |  |
|      545 |  2298 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      545 |  2299 | `	sxi32 rc = SXRET_OK;` |
|        - |  2300 | `	/* Append a new line */` |
|        - |  2301 | `#ifdef __WINNT__` |
|        1 |  2302 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2303 | `#else` |
|      544 |  2304 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2305 | `#endif` |
|        - |  2306 | `	/* Invoke the output consumer callback */` |
|      545 |  2307 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      545 |  2308 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      545 |  2309 | `	return rc;` |
|        1 |  2310 |  |
|        - |  2311 | `/*` |
|        - |  2312 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2313 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2314 | ` * information.` |
|        - |  2315 | ` */` |
|      132 |  2316 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2317 |  |
|      134 |  2318 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2319 | `		ph7_value apArg[4];` |
|        - |  2320 | `		ph7_value *apArgPtr[4];` |
|        - |  2321 | `		ph7_value sResult;` |
|        - |  2322 | `		SyString sErr;` |
|        - |  2323 | `		/* Prepare arguments */` |
|       61 |  2324 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2325 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2326 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2327 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2328 | `		if( pFile ){` |
|       61 |  2329 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2330 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2331 | `		}else{` |
|      ! 0 |  2332 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2333 | `		}` |
|       61 |  2334 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2335 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2336 | `		/* Set up pointer array */` |
|       61 |  2337 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2338 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2339 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2340 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2341 | `		/* Call the handler */` |
|       61 |  2342 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2343 | `		/* Check return value */` |
|       61 |  2344 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2345 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2346 | `		}` |
|        - |  2347 | `		/* Release */` |
|       61 |  2348 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2349 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2350 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2351 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2352 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2353 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2354 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2355 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2356 | `	}` |
|        - |  2357 | `	/* No handler, always call error handler */` |
|       73 |  2358 | `	return TRUE;` |
|       68 |  2359 |  |
|       96 |  2360 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2361 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2362 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2363 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2364 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2365 | `	)` |
|        2 |  2366 |  |
|       98 |  2367 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2368 | `	SyString *pFile;` |
|        - |  2369 | `	char *zErr;` |
|       98 |  2370 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2371 | `	if( !pVm->bErrReport ){` |
|        - |  2372 | `		/* Don't bother reporting errors */` |
|        3 |  2373 | `		return SXRET_OK;` |
|        - |  2374 | `	}` |
|        - |  2375 | `	/* Reset the working buffer */` |
|       96 |  2376 | `	SyBlobReset(pWorker);` |
|        - |  2377 | `	/* Peek the processed file if available */` |
|       96 |  2378 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2379 | `	if( pFile ){` |
|        - |  2380 | `		/* Append file name */` |
|       96 |  2381 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2382 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2383 | `	}` |
|        - |  2384 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2385 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2386 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2387 | `	 * E_DEPRECATED). */` |
|       96 |  2388 | `	zErr = "Error:  ";` |
|       96 |  2389 | `	switch(iErr){` |
|       18 |  2390 | `	case PH7_CTX_WARNING:` |
|       38 |  2391 | `		zErr = "Warning:  ";` |
|       38 |  2392 | `		break;` |
|        6 |  2393 | `	case PH7_CTX_NOTICE:` |
|       14 |  2394 | `		zErr = "Notice:  ";` |
|       12 |  2395 | `		break;` |
|       23 |  2396 | `	default:` |
|        - |  2397 | `		/* keep iErr unchanged */` |
|       46 |  2398 | `		break;` |
|        - |  2399 | `	}` |
|       96 |  2400 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2401 | `	if( pFuncName ){` |
|        - |  2402 | `		/* Append function name first */` |
|       23 |  2403 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2404 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2405 | `	}` |
|       96 |  2406 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2407 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2408 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2409 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2410 | `	}` |
|       96 |  2411 | `	return rc;` |
|       50 |  2412 |  |
|        - |  2413 | `/*` |
|        - |  2414 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2415 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2416 | ` * information.` |
|        - |  2417 | ` */` |
|       38 |  2418 | `static sxi32 VmThrowErrorAp(` |
|        - |  2419 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2420 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2421 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2422 | `	const char *zFormat, /* Format message */` |
|        - |  2423 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2424 | `	)` |
|        2 |  2425 |  |
|       40 |  2426 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2427 | `	SyBlob sMsg;` |
|        - |  2428 | `	SyString *pFile;` |
|        - |  2429 | `	char *zErr;` |
|       40 |  2430 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2431 | `	if( !pVm->bErrReport ){` |
|        - |  2432 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2433 | `		return SXRET_OK;` |
|        - |  2434 | `	}` |
|        - |  2435 | `	/* Reset the working buffer */` |
|       40 |  2436 | `	SyBlobReset(pWorker);` |
|        - |  2437 | `	/* Peek the processed file if available */` |
|       40 |  2438 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2439 | `	if( pFile ){` |
|        - |  2440 | `		/* Append file name */` |
|       40 |  2441 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2442 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2443 | `	}` |
|        - |  2444 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2445 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2446 | `	 * the correct errno value. */` |
|       40 |  2447 | `	zErr = "Error:  ";` |
|       40 |  2448 | `	switch(iErr){` |
|        4 |  2449 | `	case PH7_CTX_WARNING:` |
|        9 |  2450 | `		zErr = "Warning:  ";` |
|        9 |  2451 | `		break;` |
|        3 |  2452 | `	case PH7_CTX_NOTICE:` |
|        7 |  2453 | `		zErr = "Notice:  ";` |
|        6 |  2454 | `		break;` |
|       12 |  2455 | `	default:` |
|        - |  2456 | `		/* do not change iErr */` |
|       24 |  2457 | `		break;` |
|        - |  2458 | `	}` |
|       40 |  2459 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2460 | `	if( pFuncName ){` |
|        - |  2461 | `		/* Append function name first */` |
|       26 |  2462 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2463 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2464 | `	}` |
|        - |  2465 | `	/* Format the raw message */` |
|       40 |  2466 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2467 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2468 | `	/* Check if a user error handler is installed */` |
|       40 |  2469 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2470 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2471 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2472 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2473 | `	}` |
|       40 |  2474 | `	SyBlobRelease(&sMsg);` |
|       40 |  2475 | `	return rc;` |
|       21 |  2476 |  |
|        - |  2477 | `/*` |
|        - |  2478 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2479 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2480 | ` * information.` |
|        - |  2481 | ` * ------------------------------------` |
|        - |  2482 | ` * Simple boring wrapper function.` |
|        - |  2483 | ` * ------------------------------------` |
|        - |  2484 | ` */` |
|       14 |  2485 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2486 |  |
|        - |  2487 | `	va_list ap;` |
|        - |  2488 | `	sxi32 rc;` |
|       15 |  2489 | `	va_start(ap,zFormat);` |
|       15 |  2490 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2491 | `	va_end(ap);` |
|       15 |  2492 | `	return rc;` |
|        1 |  2493 |  |
|        - |  2494 | `/*` |
|        - |  2495 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2496 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2497 | ` * information.` |
|        - |  2498 | ` * ------------------------------------` |
|        - |  2499 | ` * Simple boring wrapper function.` |
|        - |  2500 | ` * ------------------------------------` |
|        - |  2501 | ` */` |
|       24 |  2502 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2503 |  |
|        - |  2504 | `	sxi32 rc;` |
|       26 |  2505 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2506 | `	return rc;` |
|        2 |  2507 |  |
|        - |  2508 | `/*` |
|        - |  2509 | ` * Resolve function context from the current frame.` |
|        - |  2510 | ` */` |
|      934 |  2511 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2512 |  |
|        - |  2513 | `	VmFrame *pFrame;` |
|        - |  2514 | `	ph7_vm_func *pFunc;` |
|      935 |  2515 | `	*pzFuncName = 0;` |
|      935 |  2516 | `	*pnFuncLen = 0;` |
|      935 |  2517 | `	pFrame = pVm->pFrame;` |
|      935 |  2518 | `	if( pFrame == 0 ){` |
|      ! 0 |  2519 | `		return;` |
|        - |  2520 | `	}` |
|      935 |  2521 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2522 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2523 | `		return;` |
|        - |  2524 | `	}` |
|        7 |  2525 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2526 | `	if( pFunc == 0 ){` |
|      ! 0 |  2527 | `		return;` |
|        - |  2528 | `	}` |
|        7 |  2529 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2530 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2531 |  |
|        - |  2532 | `/*` |
|        - |  2533 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2534 | ` */` |
|      470 |  2535 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2536 |  |
|        - |  2537 | `	SyBlob sOut;` |
|        - |  2538 | `	SyString *pFile;` |
|      471 |  2539 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2540 | `		return PH7_OK;` |
|        - |  2541 | `	}` |
|      471 |  2542 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2543 | `		zClass = "Exception";` |
|      ! 0 |  2544 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2545 | `	}` |
|      471 |  2546 | `	if( zMsg == 0 ){` |
|      ! 0 |  2547 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2548 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2549 | `	}` |
|      471 |  2550 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2551 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2552 | `	}` |
|      471 |  2553 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2554 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2555 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2556 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2557 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2558 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2559 | `	if( pFile ){` |
|      471 |  2560 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2561 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2562 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2563 | `	}` |
|      471 |  2564 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2565 | `	if( pFile ){` |
|      471 |  2566 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2567 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2568 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2569 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2570 | `		}else{` |
|      465 |  2571 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2572 | `		}` |
|      235 |  2573 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2574 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2575 | `	}else{` |
|      ! 0 |  2576 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2577 | `	}` |
|      471 |  2578 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2579 | `	if( pFile ){` |
|      471 |  2580 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2581 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2582 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2583 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2584 | `	}` |
|      471 |  2585 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2586 | `	SyBlobRelease(&sOut);` |
|      471 |  2587 | `	return PH7_ABORT;` |
|      236 |  2588 |  |
|        - |  2589 | `/*` |
|        - |  2590 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2591 | ` */` |
|      472 |  2592 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2593 |  |
|        - |  2594 | `	ph7_vm *pVm;` |
|        - |  2595 | `	ph7_class *pClass;` |
|        - |  2596 | `	ph7_class_instance *pThis;` |
|        - |  2597 | `	ph7_class_method *pCons;` |
|        - |  2598 | `	ph7_value sArg;` |
|        - |  2599 | `	ph7_value *apArg[1];` |
|        - |  2600 | `	SyBlob sMsg;` |
|        - |  2601 | `	SyString sMsgStr;` |
|        - |  2602 | `	VmFrame *pFrame;` |
|        - |  2603 | `	va_list ap;` |
|        - |  2604 | `	sxi32 rc;` |
|        - |  2605 |  |
|      474 |  2606 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2607 | `		return PH7_ABORT;` |
|        - |  2608 | `	}` |
|      474 |  2609 | `	pVm = pCtx->pVm;` |
|      474 |  2610 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2611 | `		zClass = "Error";` |
|      ! 0 |  2612 | `	}` |
|      474 |  2613 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      474 |  2614 | `	if( pClass == 0 ){` |
|      ! 0 |  2615 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2616 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2617 | `			zClass` |
|        - |  2618 | `			);` |
|        - |  2619 | `	}` |
|      474 |  2620 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      474 |  2621 | `	if( pThis == 0 ){` |
|      ! 0 |  2622 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2623 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2624 | `			);` |
|        - |  2625 | `	}` |
|        - |  2626 |  |
|      474 |  2627 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      474 |  2628 | `	va_start(ap,zFormat);` |
|      474 |  2629 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      474 |  2630 | `	va_end(ap);` |
|        - |  2631 |  |
|      474 |  2632 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      474 |  2633 | `	if( pCons ){` |
|      474 |  2634 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      474 |  2635 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      474 |  2636 | `		apArg[0] = &sArg;` |
|      474 |  2637 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      474 |  2638 | `		PH7_MemObjRelease(&sArg);` |
|      236 |  2639 | `	}` |
|      474 |  2640 | `	SyBlobRelease(&sMsg);` |
|        - |  2641 |  |
|      474 |  2642 | `	pFrame = pVm->pFrame;` |
|      474 |  2643 | `	if( pFrame ){` |
|      474 |  2644 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      474 |  2645 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      236 |  2646 | `	}` |
|      474 |  2647 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      474 |  2648 | `	PH7_ClassInstanceUnref(pThis);` |
|      474 |  2649 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2650 | `		return PH7_ABORT;` |
|        - |  2651 | `	}` |
|       12 |  2652 | `	return PH7_EXCEPTION;` |
|      238 |  2653 |  |
|        - |  2654 | `/*` |
|        - |  2655 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2656 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2657 | ` */` |
|      ! 0 |  2658 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2659 |  |
|        - |  2660 | `	ph7_vm *pVm;` |
|        - |  2661 | `	SyBlob sMsg;` |
|      ! 0 |  2662 | `	const char *zFuncName = 0;` |
|      ! 0 |  2663 | `	int nFuncLen = 0;` |
|        - |  2664 | `	va_list ap;` |
|        - |  2665 | `	sxi32 rc;` |
|        - |  2666 |  |
|      ! 0 |  2667 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2668 | `		return PH7_OK;` |
|        - |  2669 | `	}` |
|      ! 0 |  2670 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2671 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2672 | `		zClass = "Error";` |
|      ! 0 |  2673 | `	}` |
|        - |  2674 |  |
|      ! 0 |  2675 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2676 |  |
|      ! 0 |  2677 | `	va_start(ap,zFormat);` |
|      ! 0 |  2678 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2679 | `	va_end(ap);` |
|        - |  2680 |  |
|      ! 0 |  2681 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2682 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2683 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2684 | `	}` |
|      ! 0 |  2685 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2686 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2687 | `	}` |
|      ! 0 |  2688 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2689 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2690 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2691 | `	return rc;` |
|      ! 0 |  2692 |  |
|        - |  2693 | `/*` |
|        - |  2694 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2695 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2696 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2697 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2698 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2699 | ` * when VmByteCodeExec returns.` |
|        - |  2700 | ` */` |
|      132 |  2701 | `static sxi32 VmSuspendCtx(` |
|        - |  2702 | `	ph7_vm *pVm,` |
|        - |  2703 | `	ph7_exec_ctx *pCtx,` |
|        - |  2704 | `	sxi32 pc,` |
|        - |  2705 | `	sxi32 nTos` |
|        - |  2706 | `	)` |
|        2 |  2707 |  |
|       66 |  2708 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  2709 | `	pCtx->pc = pc;` |
|      134 |  2710 | `	pCtx->nTos = nTos;` |
|      134 |  2711 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  2712 | `	return PH7_SUSPEND;` |
|        2 |  2713 |  |
|        - |  2714 | `/*` |
|        - |  2715 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2716 | ` *` |
|        - |  2717 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2718 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2719 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2720 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2721 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2722 | ` * then the program execution is halted.` |
|        - |  2723 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2724 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2725 | ` * or to reset the VM to it's initial state.` |
|        - |  2726 | ` */` |
|    32976 |  2727 | `static sxi32 VmByteCodeExec(` |
|        - |  2728 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2729 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2730 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2731 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2732 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2733 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2734 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2735 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2736 | `	)` |
|        2 |  2737 |  |
|        - |  2738 | `	VmInstr *pInstr;` |
|        - |  2739 | `	ph7_value *pTos;` |
|        - |  2740 | `	SySet aArg;` |
|        - |  2741 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2742 | `	sxi32 pc;` |
|        - |  2743 | `	sxi32 rc;` |
|        - |  2744 | `	/* Argument container */` |
|    32978 |  2745 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32978 |  2746 | `	if( nTos < 0 ){` |
|    30928 |  2747 | `		pTos = &pStack[-1];` |
|    15465 |  2748 | `	}else{` |
|     2052 |  2749 | `		pTos = &pStack[nTos];` |
|        - |  2750 | `	}` |
|    32978 |  2751 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32978 |  2752 | `	pc = nPc;` |
|        - |  2753 | `	/* Execute as much as we can */` |
|  5017646 |  2754 | `	for(;;){` |
|        - |  2755 | `		/* Fetch the instruction to execute */` |
| 10034590 |  2756 | `		pInstr = &aInstr[pc];` |
| 10034590 |  2757 | `		rc = SXRET_OK;` |
|        - |  2758 | `/*` |
|        - |  2759 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2760 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2761 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2762 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2763 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2764 | ` */` |
| 10034590 |  2765 | `		switch(pInstr->iOp){` |
|        - |  2766 | `/*` |
|        - |  2767 | ` * DONE: P1 * *` |
|        - |  2768 | ` *` |
|        - |  2769 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2770 | ` * and return immediately.` |
|        - |  2771 | ` */` |
|    16177 |  2772 | `case PH7_OP_DONE:` |
|    32356 |  2773 | `	if( pInstr->iP1 ){` |
|        - |  2774 | `#ifdef UNTRUST` |
|        - |  2775 | `		if( pTos < pStack ){` |
|        - |  2776 | `			goto Abort;` |
|        - |  2777 | `		}` |
|        - |  2778 | `#endif` |
|    18762 |  2779 | `		if( pLastRef ){` |
|    12234 |  2780 | `			*pLastRef = pTos->nIdx;` |
|     6116 |  2781 | `		}` |
|    18762 |  2782 | `		if( pResult ){` |
|        - |  2783 | `			/* Execution result */` |
|    17818 |  2784 | `			PH7_MemObjStore(pTos,pResult);` |
|     8908 |  2785 | `		}` |
|    18762 |  2786 | `		VmPopOperand(&pTos,1);` |
|    22976 |  2787 | `	}else if( pLastRef ){` |
|        - |  2788 | `		/* Nothing referenced */` |
|     1028 |  2789 | `		*pLastRef = SXU32_HIGH;` |
|      513 |  2790 | `	}` |
|        - |  2791 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2792 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2793 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2794 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2795 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2796 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2797 | `	 * block can override it.` |
|        - |  2798 | `	 */` |
|    32358 |  2799 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2800 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2801 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2802 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2803 | `		pExc->pFrame = 0;` |
|        3 |  2804 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2805 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2806 | `			pExc->iFinallyDone = 1;` |
|        - |  2807 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2808 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2809 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2810 | `				goto Abort;` |
|        - |  2811 | `			}` |
|        1 |  2812 | `		}` |
|        1 |  2813 | `	}` |
|    32356 |  2814 | `	goto Done;` |
|        - |  2815 | `/*` |
|        - |  2816 | ` * HALT: P1 * *` |
|        - |  2817 | ` *` |
|        - |  2818 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2819 | ` * and abort immediately.` |
|        - |  2820 | ` */` |
|        4 |  2821 | `case PH7_OP_HALT:` |
|        9 |  2822 | `	if( pInstr->iP1 ){` |
|        - |  2823 | `#ifdef UNTRUST` |
|        - |  2824 | `		if( pTos < pStack ){` |
|        - |  2825 | `			goto Abort;` |
|        - |  2826 | `		}` |
|        - |  2827 | `#endif` |
|        9 |  2828 | `		if( pLastRef ){` |
|      ! 0 |  2829 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2830 | `		}` |
|        9 |  2831 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2832 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2833 | `				/* Output the exit message */` |
|        7 |  2834 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2835 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2836 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2837 | `			}` |
|        7 |  2838 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2839 | `			/* Record exit status */` |
|        5 |  2840 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2841 | `		}` |
|        9 |  2842 | `		VmPopOperand(&pTos,1);` |
|        4 |  2843 | `	}else if( pLastRef ){` |
|        - |  2844 | `		/* Nothing referenced */` |
|      ! 0 |  2845 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2846 | `	}` |
|        - |  2847 | `	/* Check if we're in an included file context */` |
|        9 |  2848 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2849 | `		/* Terminate the entire process */` |
|        9 |  2850 | `		exit(pVm->iExitStatus);` |
|        - |  2851 | `	}` |
|      ! 0 |  2852 | `	goto Abort;` |
|        - |  2853 | `/*` |
|        - |  2854 | ` * JMP: * P2 *` |
|        - |  2855 | ` *` |
|        - |  2856 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2857 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2858 | ` */` |
|   216415 |  2859 | `case PH7_OP_JMP:` |
|   432876 |  2860 | `	pc = pInstr->iP2 - 1;` |
|   432876 |  2861 | `	break;` |
|        - |  2862 | `/*` |
|        - |  2863 | ` * JZ: P1 P2 *` |
|        - |  2864 | ` *` |
|        - |  2865 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2866 | ` * entry in the stack if P1 is zero.` |
|        - |  2867 | ` */` |
|   506013 |  2868 | `case PH7_OP_JZ:` |
|        - |  2869 | `#ifdef UNTRUST` |
|        - |  2870 | `	if( pTos < pStack ){` |
|        - |  2871 | `		goto Abort;` |
|        - |  2872 | `	}` |
|        - |  2873 | `#endif` |
|        - |  2874 | `	/* Get a boolean value */` |
|  1012116 |  2875 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2876 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2877 | `	}` |
|  1012116 |  2878 | `	if( !pTos->x.iVal ){` |
|        - |  2879 | `		/* Take the jump */` |
|   511976 |  2880 | `		pc = pInstr->iP2 - 1;` |
|   255987 |  2881 | `	}` |
|  1012116 |  2882 | `	if( !pInstr->iP1 ){` |
|   804530 |  2883 | `		VmPopOperand(&pTos,1);` |
|   402286 |  2884 | `	}` |
|  1012116 |  2885 | `	break;` |
|        - |  2886 | `/*` |
|        - |  2887 | ` * JNZ: P1 P2 *` |
|        - |  2888 | ` *` |
|        - |  2889 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2890 | ` * entry in the stack if P1 is zero.` |
|        - |  2891 | ` */` |
|    53538 |  2892 | `case PH7_OP_JNZ:` |
|        - |  2893 | `#ifdef UNTRUST` |
|        - |  2894 | `	if( pTos < pStack ){` |
|        - |  2895 | `		goto Abort;` |
|        - |  2896 | `	}` |
|        - |  2897 | `#endif` |
|        - |  2898 | `	/* Get a boolean value */` |
|   107078 |  2899 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2900 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2901 | `	}` |
|   107078 |  2902 | `	if( pTos->x.iVal ){` |
|        - |  2903 | `		/* Take the jump */` |
|     4560 |  2904 | `		pc = pInstr->iP2 - 1;` |
|     2279 |  2905 | `	}` |
|   107078 |  2906 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2907 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2908 | `	}` |
|   107078 |  2909 | `	break;` |
|        - |  2910 | `/*` |
|        - |  2911 | ` * NOOP: * * *` |
|        - |  2912 | ` *` |
|        - |  2913 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2914 | ` * destination.` |
|        - |  2915 | ` */` |
|      ! 0 |  2916 | `case PH7_OP_NOOP:` |
|      ! 0 |  2917 | `	break;` |
|        - |  2918 | `/*` |
|        - |  2919 | ` * POP: P1 * *` |
|        - |  2920 | ` *` |
|        - |  2921 | ` * Pop P1 elements from the operand stack.` |
|        - |  2922 | ` */` |
|   394644 |  2923 | `case PH7_OP_POP: {` |
|   789334 |  2924 | `	sxi32 n = pInstr->iP1;` |
|   789334 |  2925 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2926 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2927 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2928 | `	}` |
|   789334 |  2929 | `	VmPopOperand(&pTos,n);` |
|   789334 |  2930 | `	break;` |
|        - |  2931 | `				 }` |
|        - |  2932 | `/*` |
|        - |  2933 | ` * DUP: * * *` |
|        - |  2934 | ` *` |
|        - |  2935 | ` * Duplicate the top of the stack.` |
|        - |  2936 | ` */` |
|       35 |  2937 | `case PH7_OP_DUP:` |
|        - |  2938 | `#ifdef UNTRUST` |
|        - |  2939 | `	if( pTos < pStack ){` |
|        - |  2940 | `		goto Abort;` |
|        - |  2941 | `	}` |
|        - |  2942 | `#endif` |
|       72 |  2943 | `	pTos++;` |
|       72 |  2944 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2945 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2946 | `	break;` |
|        - |  2947 | `/*` |
|        - |  2948 | ` * NSSWITCH: * * P3` |
|        - |  2949 | ` *` |
|        - |  2950 | ` * Switch the active namespace at runtime.` |
|        - |  2951 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2952 | ` */` |
|     6528 |  2953 | `case PH7_OP_NSSWITCH:` |
|    13058 |  2954 | `	SyBlobReset(&pVm->sNamespace);` |
|    13058 |  2955 | `	if( pInstr->p3 ){` |
|       90 |  2956 | `		const char *zNs = (const char *)pInstr->p3;` |
|       90 |  2957 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       44 |  2958 | `	}` |
|        - |  2959 | `	/* Clear namespace-scoped use-const imports */` |
|    13058 |  2960 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13058 |  2961 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13058 |  2962 | `	break;` |
|        - |  2963 | `/* OP_USECONST P1 * P3` |
|        - |  2964 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  2965 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  2966 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  2967 | ` */` |
|        7 |  2968 | `case PH7_OP_USECONST: {` |
|       16 |  2969 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  2970 | `	if( azPair ){` |
|       16 |  2971 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  2972 | `	}` |
|       16 |  2973 | `	break;` |
|        - |  2974 | `				}` |
|        - |  2975 | `/*` |
|        - |  2976 | ` * CVT_INT: * * *` |
|        - |  2977 | ` *` |
|        - |  2978 | ` * Force the top of the stack to be an integer.` |
|        - |  2979 | ` */` |
|       35 |  2980 | `case PH7_OP_CVT_INT:` |
|        - |  2981 | `#ifdef UNTRUST` |
|        - |  2982 | `	if( pTos < pStack ){` |
|        - |  2983 | `		goto Abort;` |
|        - |  2984 | `	}` |
|        - |  2985 | `#endif` |
|       72 |  2986 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2987 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2988 | `	}` |
|        - |  2989 | `	/* Invalidate any prior representation */` |
|       72 |  2990 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2991 | `	break;` |
|        - |  2992 | `/*` |
|        - |  2993 | ` * CVT_REAL: * * *` |
|        - |  2994 | ` *` |
|        - |  2995 | ` * Force the top of the stack to be a real.` |
|        - |  2996 | ` */` |
|        4 |  2997 | `case PH7_OP_CVT_REAL:` |
|        - |  2998 | `#ifdef UNTRUST` |
|        - |  2999 | `	if( pTos < pStack ){` |
|        - |  3000 | `		goto Abort;` |
|        - |  3001 | `	}` |
|        - |  3002 | `#endif` |
|        9 |  3003 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3004 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3005 | `	}` |
|        - |  3006 | `	/* Invalidate any prior representation */` |
|        9 |  3007 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3008 | `	break;` |
|        - |  3009 | `/*` |
|        - |  3010 | ` * CVT_STR: * * *` |
|        - |  3011 | ` *` |
|        - |  3012 | ` * Force the top of the stack to be a string.` |
|        - |  3013 | ` */` |
|      146 |  3014 | `case PH7_OP_CVT_STR:` |
|        - |  3015 | `#ifdef UNTRUST` |
|        - |  3016 | `	if( pTos < pStack ){` |
|        - |  3017 | `		goto Abort;` |
|        - |  3018 | `	}` |
|        - |  3019 | `#endif` |
|      294 |  3020 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3021 | `		PH7_MemObjToString(pTos);` |
|      146 |  3022 | `	}` |
|      294 |  3023 | `	break;` |
|        - |  3024 | `/*` |
|        - |  3025 | ` * CVT_BOOL: * * *` |
|        - |  3026 | ` *` |
|        - |  3027 | ` * Force the top of the stack to be a boolean.` |
|        - |  3028 | ` */` |
|        5 |  3029 | `case PH7_OP_CVT_BOOL:` |
|        - |  3030 | `#ifdef UNTRUST` |
|        - |  3031 | `	if( pTos < pStack ){` |
|        - |  3032 | `		goto Abort;` |
|        - |  3033 | `	}` |
|        - |  3034 | `#endif` |
|       11 |  3035 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3036 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3037 | `	}` |
|       11 |  3038 | `	break;` |
|        - |  3039 | `/*` |
|        - |  3040 | ` * CVT_NULL: * * *` |
|        - |  3041 | ` *` |
|        - |  3042 | ` * Nullify the top of the stack.` |
|        - |  3043 | ` */` |
|        3 |  3044 | `case PH7_OP_CVT_NULL:` |
|        - |  3045 | `#ifdef UNTRUST` |
|        - |  3046 | `	if( pTos < pStack ){` |
|        - |  3047 | `		goto Abort;` |
|        - |  3048 | `	}` |
|        - |  3049 | `#endif` |
|        7 |  3050 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3051 | `	break;` |
|        - |  3052 | `/*` |
|        - |  3053 | ` * CVT_NUMC: * * *` |
|        - |  3054 | ` *` |
|        - |  3055 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3056 | ` */` |
|      ! 0 |  3057 | `case PH7_OP_CVT_NUMC:` |
|        - |  3058 | `#ifdef UNTRUST` |
|        - |  3059 | `	if( pTos < pStack ){` |
|        - |  3060 | `		goto Abort;` |
|        - |  3061 | `	}` |
|        - |  3062 | `#endif` |
|        - |  3063 | `	/* Force a numeric cast */` |
|      ! 0 |  3064 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3065 | `	break;` |
|        - |  3066 | `/*` |
|        - |  3067 | ` * CVT_ARRAY: * * *` |
|        - |  3068 | ` *` |
|        - |  3069 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3070 | ` */` |
|       10 |  3071 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3072 | `#ifdef UNTRUST` |
|        - |  3073 | `	if( pTos < pStack ){` |
|        - |  3074 | `		goto Abort;` |
|        - |  3075 | `	}` |
|        - |  3076 | `#endif` |
|        - |  3077 | `	/* Force a hashmap cast */` |
|       21 |  3078 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3079 | `	if( rc != SXRET_OK ){` |
|        - |  3080 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3081 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3082 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3083 | `	}` |
|       21 |  3084 | `	break;` |
|        - |  3085 | `/*` |
|        - |  3086 | ` * CVT_OBJ: * * *` |
|        - |  3087 | ` *` |
|        - |  3088 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3089 | ` */` |
|        8 |  3090 | `case PH7_OP_CVT_OBJ:` |
|        - |  3091 | `#ifdef UNTRUST` |
|        - |  3092 | `	if( pTos < pStack ){` |
|        - |  3093 | `		goto Abort;` |
|        - |  3094 | `	}` |
|        - |  3095 | `#endif` |
|       17 |  3096 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3097 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3098 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3099 | `	}` |
|       17 |  3100 | `	break;` |
|        - |  3101 | `/*` |
|        - |  3102 | ` * ERR_CTRL * * *` |
|        - |  3103 | ` *` |
|        - |  3104 | ` * Error control operator.` |
|        - |  3105 | ` */` |
|    13102 |  3106 | `case PH7_OP_ERR_CTRL:` |
|        - |  3107 | `	/*` |
|        - |  3108 | `	 * TICKET 1433-038:` |
|        - |  3109 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3110 | `	 * use the public API,to control error output.` |
|        - |  3111 | `	 */` |
|    26204 |  3112 | `	break;` |
|        - |  3113 | `/*` |
|        - |  3114 | ` * IS_A * * *` |
|        - |  3115 | ` *` |
|        - |  3116 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3117 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3118 | ` * holding a class name or an object).` |
|        - |  3119 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3120 | ` */` |
|       23 |  3121 | `case PH7_OP_IS_A:{` |
|       48 |  3122 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3123 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3124 | `#ifdef UNTRUST` |
|        - |  3125 | `	if( pNos < pStack ){` |
|        - |  3126 | `		goto Abort;` |
|        - |  3127 | `	}` |
|        - |  3128 | `#endif` |
|       48 |  3129 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3130 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3131 | `		ph7_class *pClass = 0;` |
|        - |  3132 | `		/* Extract the target class */` |
|       46 |  3133 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3134 | `			/* Instance already loaded */` |
|      ! 0 |  3135 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3136 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3137 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3138 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3139 | `			/* Handle self/static/parent keywords */` |
|       46 |  3140 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3141 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3142 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3143 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3144 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3145 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3146 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3147 | `					pClass = pSelf->pBase;` |
|        2 |  3148 | `				}` |
|        3 |  3149 | `			}else{` |
|       36 |  3150 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3151 | `			}` |
|       22 |  3152 | `		}` |
|       46 |  3153 | `		if( pClass ){` |
|        - |  3154 | `			/* Perform the query */` |
|       46 |  3155 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3156 | `		}` |
|       22 |  3157 | `	}` |
|        - |  3158 | `	/* Push result */` |
|       48 |  3159 | `	VmPopOperand(&pTos,1);` |
|       48 |  3160 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3161 | `	pTos->x.iVal = iRes;` |
|       48 |  3162 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3163 | `	break;` |
|        - |  3164 | `				 }` |
|        - |  3165 |  |
|        - |  3166 | `/*` |
|        - |  3167 | ` * LOADC P1 P2 *` |
|        - |  3168 | ` *` |
|        - |  3169 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3170 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3171 | ` */` |
|   845201 |  3172 | `case PH7_OP_LOADC: {` |
|        - |  3173 | `	ph7_value *pObj;` |
|        - |  3174 | `	/* Reserve a room */` |
|  1690448 |  3175 | `	pTos++;` |
|  2527450 |  3176 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1690448 |  3177 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3178 | `			SyHashEntry *pEntry;` |
|        - |  3179 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3180 | `			{` |
|        - |  3181 | `				SyHashEntry *pConstImport;` |
|    24734 |  3182 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    16488 |  3183 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16490 |  3184 | `				if( pConstImport ){` |
|       11 |  3185 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3186 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3187 | `					if( pEntry ){` |
|       11 |  3188 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3189 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3190 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3191 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3192 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3193 | `						break;` |
|        - |  3194 | `					}` |
|        - |  3195 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3196 | `				}` |
|        - |  3197 | `			}` |
|        - |  3198 | `			/* Candidate for expansion via user defined callbacks */` |
|    16480 |  3199 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16480 |  3200 | `			if( pEntry ){` |
|    16476 |  3201 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3202 | `				/* Set a NULL default value */` |
|    16476 |  3203 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16476 |  3204 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3205 | `				/* Invoke the callback and deal with the expanded value */` |
|    16476 |  3206 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3207 | `				/* Mark as constant */` |
|    16476 |  3208 | `				pTos->nIdx = SXU32_HIGH;` |
|    16476 |  3209 | `				break;` |
|        - |  3210 | `			}` |
|        - |  3211 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3212 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3213 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3214 | `			{` |
|        6 |  3215 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3216 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3217 | `				sxu32 j;` |
|        6 |  3218 | `				int isQualified = 0;` |
|       32 |  3219 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3220 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3221 | `				}` |
|        6 |  3222 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3223 | `					/* Try current_namespace\name */` |
|      ! 0 |  3224 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3225 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3226 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3227 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3228 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3229 | `					if( pEntry ){` |
|      ! 0 |  3230 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3231 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3232 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3233 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3234 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3235 | `						break;` |
|        - |  3236 | `					}` |
|        - |  3237 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3238 | `				}` |
|        6 |  3239 | `				if( isQualified ){` |
|        - |  3240 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3241 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3242 | `					SyBlob sErr;` |
|        3 |  3243 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3244 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3245 | `					if( pErrFile ){` |
|        3 |  3246 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3247 | `					}` |
|        3 |  3248 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3249 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3250 | `					SyBlobRelease(&sErr);` |
|        3 |  3251 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3252 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3253 | `					goto LoadC_Done;` |
|        - |  3254 | `				}` |
|        - |  3255 | `			}` |
|        1 |  3256 | `		}` |
|  1673962 |  3257 | `		PH7_MemObjLoad(pObj,pTos);` |
|   837004 |  3258 | `	}else{` |
|        - |  3259 | `		/* Set a NULL value */` |
|      ! 0 |  3260 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3261 | `	}` |
|   836959 |  3262 | `LoadC_Done:` |
|        - |  3263 | `	/* Mark as constant */` |
|  1673964 |  3264 | `	pTos->nIdx = SXU32_HIGH;` |
|  1673964 |  3265 | `	break;` |
|        - |  3266 | `				  }` |
|        - |  3267 | `/*` |
|        - |  3268 | ` * LOAD: P1 * P3` |
|        - |  3269 | ` *` |
|        - |  3270 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3271 | ` * from the P3 operand.` |
|        - |  3272 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3273 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3274 | ` */` |
|  1356077 |  3275 | `case PH7_OP_LOAD:{` |
|        - |  3276 | `	ph7_value *pObj;` |
|        - |  3277 | `	SyString sName;` |
|  2712376 |  3278 | `	if( pInstr->p3 == 0 ){` |
|        - |  3279 | `		/* Take the variable name from the top of the stack */` |
|        - |  3280 | `#ifdef UNTRUST` |
|        - |  3281 | `		if( pTos < pStack ){` |
|        - |  3282 | `			goto Abort;` |
|        - |  3283 | `		}` |
|        - |  3284 | `#endif` |
|        - |  3285 | `		/* Force a string cast */` |
|       19 |  3286 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3287 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3288 | `		}` |
|       19 |  3289 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3290 | `	}else{` |
|  2712358 |  3291 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3292 | `		/* Reserve a room for the target object */` |
|  2712358 |  3293 | `		pTos++;` |
|        - |  3294 | `	}` |
|        - |  3295 | `	/* Extract the requested memory object */` |
|  2712376 |  3296 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2712376 |  3297 | `	if( pObj == 0 ){` |
|       26 |  3298 | `		if( pInstr->iP1 ){` |
|        - |  3299 | `			/* Variable not found,load NULL */` |
|       26 |  3300 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3301 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3302 | `			}else{` |
|       26 |  3303 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3304 | `			}` |
|       26 |  3305 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1356091 |  3306 | `			break;` |
|      ! 0 |  3307 | `		}else{` |
|        - |  3308 | `			/* Fatal error */` |
|      ! 0 |  3309 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3310 | `			goto Abort;` |
|        - |  3311 | `		}` |
|        - |  3312 | `	}` |
|        - |  3313 | `	/* Load variable contents */` |
|  2712352 |  3314 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2712352 |  3315 | `	pTos->nIdx = pObj->nIdx;` |
|  2712352 |  3316 | `	break;` |
|        - |  3317 | `				   }` |
|        - |  3318 | `/*` |
|        - |  3319 | ` * LOAD_MAP P1 * *` |
|        - |  3320 | ` *` |
|        - |  3321 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3322 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3323 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3324 | ` */` |
|    18833 |  3325 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3326 | `	ph7_hashmap *pMap;` |
|        - |  3327 | `	/* Allocate a new hashmap instance */` |
|    37668 |  3328 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    37668 |  3329 | `	if( pMap == 0 ){` |
|      ! 0 |  3330 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3331 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3332 | `		goto Abort;` |
|        - |  3333 | `	}` |
|    37668 |  3334 | `	if( pInstr->iP1 > 0 ){` |
|     2276 |  3335 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3336 | `		/* Perform the insertion */` |
|     6960 |  3337 | `		while( pEntry < pTos ){` |
|     4686 |  3338 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3339 | `				/* Insertion by reference */` |
|      142 |  3340 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3341 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3342 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3343 | `					);` |
|       48 |  3344 | `			}else{` |
|        - |  3345 | `				/* Standard insertion */` |
|     6887 |  3346 | `				PH7_HashmapInsert(pMap,` |
|     4590 |  3347 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2295 |  3348 | `					&pEntry[1]` |
|        - |  3349 | `				);` |
|        - |  3350 | `			}` |
|        - |  3351 | `			/* Next pair on the stack */` |
|     4686 |  3352 | `			pEntry += 2;` |
|        2 |  3353 | `		}` |
|        - |  3354 | `		/* Pop P1 elements */` |
|     2276 |  3355 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1137 |  3356 | `	}` |
|        - |  3357 | `	/* Push the hashmap */` |
|    37668 |  3358 | `	pTos++;` |
|    37668 |  3359 | `	pTos->nIdx = SXU32_HIGH;` |
|    37668 |  3360 | `	pTos->x.pOther = pMap;` |
|    37668 |  3361 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    37668 |  3362 | `	break;` |
|        - |  3363 | `					  }` |
|        - |  3364 | `/*` |
|        - |  3365 | ` * LOAD_LIST: P1 * *` |
|        - |  3366 | ` *` |
|        - |  3367 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3368 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3369 | ` * Caveats:` |
|        - |  3370 | ` *  This implementation support only a single nesting level.` |
|        - |  3371 | ` */` |
|       26 |  3372 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3373 | `	ph7_value *pEntry;` |
|       53 |  3374 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3375 | `		/* Empty list,break immediately */` |
|      ! 0 |  3376 | `		break;` |
|        - |  3377 | `	}` |
|       53 |  3378 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3379 | `#ifdef UNTRUST` |
|        - |  3380 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3381 | `		goto Abort;` |
|        - |  3382 | `	}` |
|        - |  3383 | `#endif` |
|       53 |  3384 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       49 |  3385 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3386 | `		ph7_hashmap_node *pNode;` |
|        - |  3387 | `		ph7_value sKey,*pObj;` |
|        - |  3388 | `		/* Start Copying */` |
|       49 |  3389 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      153 |  3390 | `		while( pEntry <= pTos ){` |
|      105 |  3391 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       97 |  3392 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       97 |  3393 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       97 |  3394 | `					if( rc == SXRET_OK ){` |
|        - |  3395 | `						/* Store node value */` |
|       97 |  3396 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       49 |  3397 | `					}else{` |
|        - |  3398 | `						/* Nullify the variable */` |
|      ! 0 |  3399 | `						PH7_MemObjRelease(pObj);` |
|        - |  3400 | `					}` |
|       48 |  3401 | `				}` |
|       48 |  3402 | `			}` |
|      105 |  3403 | `			sKey.x.iVal++; /* Next numeric index */` |
|      105 |  3404 | `			pEntry++;` |
|        1 |  3405 | `		}` |
|       24 |  3406 | `	}` |
|       53 |  3407 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       53 |  3408 | `	break;` |
|        - |  3409 | `					   }` |
|        - |  3410 | `/*` |
|        - |  3411 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3412 | ` *` |
|        - |  3413 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3414 | ` * from the stack.` |
|        - |  3415 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3416 | ` * instead.` |
|        - |  3417 | ` */` |
|   217103 |  3418 | `case PH7_OP_LOAD_IDX: {` |
|   434252 |  3419 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   434252 |  3420 | `	ph7_hashmap *pMap = 0;` |
|        - |  3421 | `	ph7_value *pIdx;` |
|   434252 |  3422 | `	pIdx = 0;` |
|   434252 |  3423 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3424 | `		if( !pInstr->iP2){` |
|        - |  3425 | `			/* No available index,load NULL */` |
|      ! 0 |  3426 | `			if( pTos >= pStack ){` |
|      ! 0 |  3427 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3428 | `			}else{` |
|        - |  3429 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3430 | `				pTos++;` |
|      ! 0 |  3431 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3432 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3433 | `			}` |
|        - |  3434 | `			/* Emit a notice */` |
|      ! 0 |  3435 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3436 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3437 | `			break;` |
|        - |  3438 | `		}` |
|      ! 0 |  3439 | `	}else{` |
|   434252 |  3440 | `		pIdx = pTos;` |
|   434252 |  3441 | `		pTos--;` |
|        - |  3442 | `	}` |
|   434252 |  3443 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3444 | `		/* String access */` |
|   340676 |  3445 | `		if( pIdx ){` |
|        - |  3446 | `			sxu32 nOfft;` |
|   340676 |  3447 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3448 | `				/* Force an int cast */` |
|      ! 0 |  3449 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3450 | `			}` |
|   340676 |  3451 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340676 |  3452 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3453 | `				/* Invalid offset,load null */` |
|      ! 0 |  3454 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3455 | `			}else{` |
|   340676 |  3456 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340676 |  3457 | `				int c = zData[nOfft];` |
|   340676 |  3458 | `				PH7_MemObjRelease(pTos);` |
|   340676 |  3459 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340676 |  3460 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3461 | `			}` |
|   170361 |  3462 | `		}else{` |
|        - |  3463 | `			/* No available index,load NULL */` |
|      ! 0 |  3464 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3465 | `		}` |
|   340676 |  3466 | `		break;` |
|        - |  3467 | `	}` |
|    93578 |  3468 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3469 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3470 | `			ph7_value *pObj;` |
|      ! 0 |  3471 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3472 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3473 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3474 | `			}` |
|      ! 0 |  3475 | `		}` |
|      ! 0 |  3476 | `	}` |
|    93578 |  3477 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    93578 |  3478 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    93578 |  3479 | `		if( pInstr->iP2 ){` |
|        - |  3480 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3481 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3482 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3483 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3484 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3485 | `		}` |
|        - |  3486 | `		/* Point to the hashmap */` |
|    93578 |  3487 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    93578 |  3488 | `		if( pIdx ){` |
|        - |  3489 | `			/* Load the desired entry */` |
|    93578 |  3490 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    46788 |  3491 | `		}` |
|    93578 |  3492 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3493 | `			/* Create a new empty entry */` |
|      265 |  3494 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3495 | `			if( rc == SXRET_OK ){` |
|        - |  3496 | `				/* Point to the last inserted entry */` |
|      265 |  3497 | `				pNode = pMap->pLast;` |
|      132 |  3498 | `			}` |
|      132 |  3499 | `		}` |
|    46788 |  3500 | `	}` |
|    93578 |  3501 | `	if( pIdx ){` |
|    93578 |  3502 | `		PH7_MemObjRelease(pIdx);` |
|    46788 |  3503 | `	}` |
|    93578 |  3504 | `	if( rc == SXRET_OK ){` |
|        - |  3505 | `		/* Load entry contents */` |
|    42704 |  3506 | `		if( pMap->iRef < 2 ){` |
|        - |  3507 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3508 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3509 | `			 */` |
|       24 |  3510 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3511 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3512 | `		}else{` |
|    42682 |  3513 | `			pTos->nIdx = pNode->nValIdx;` |
|    42682 |  3514 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    42682 |  3515 | `			PH7_HashmapUnref(pMap);` |
|        - |  3516 | `		}` |
|    21353 |  3517 | `	}else{` |
|        - |  3518 | `		/* No such entry,load NULL */` |
|    50876 |  3519 | `		PH7_MemObjRelease(pTos);` |
|    50876 |  3520 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3521 | `	}` |
|    93578 |  3522 | `	break;` |
|        - |  3523 | `					  }` |
|        - |  3524 | `/*` |
|        - |  3525 | ` * LOAD_CLOSURE * * P3` |
|        - |  3526 | ` *` |
|        - |  3527 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3528 | ` * name in the stack.` |
|        - |  3529 | ` */` |
|        4 |  3530 | `case PH7_OP_LOAD_CLOSURE:{` |
|        9 |  3531 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        9 |  3532 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3533 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3534 | `		ph7_vm_func *pClosure;` |
|        - |  3535 | `		char *zName;` |
|        - |  3536 | `		sxu32 mLen;` |
|        - |  3537 | `		sxu32 n;` |
|        - |  3538 | `		/* Create a new VM function */` |
|        9 |  3539 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3540 | `		/* Generate an unique closure name */` |
|        9 |  3541 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        9 |  3542 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3543 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3544 | `			goto Abort;` |
|        - |  3545 | `		}` |
|        9 |  3546 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        9 |  3547 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3548 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3549 | `		}` |
|        - |  3550 | `		/* Zero the stucture */` |
|        9 |  3551 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3552 | `		/* Perform a structure assignment on read-only items */` |
|        9 |  3553 | `		pClosure->aArgs = pFunc->aArgs;` |
|        9 |  3554 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        9 |  3555 | `		pClosure->aStatic = pFunc->aStatic;` |
|        9 |  3556 | `		pClosure->iFlags = pFunc->iFlags;` |
|        9 |  3557 | `		pClosure->pUserData = pFunc->pUserData;` |
|        9 |  3558 | `		pClosure->sSignature = pFunc->sSignature;` |
|        9 |  3559 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|        9 |  3560 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|        9 |  3561 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3562 | `		/* Register the closure */` |
|        9 |  3563 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3564 | `		/* Set up closure environment */` |
|        9 |  3565 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        9 |  3566 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       27 |  3567 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3568 | `			ph7_value *pValue;` |
|       19 |  3569 | `			pEnv = &aEnv[n];` |
|       19 |  3570 | `			sEnv.sName  = pEnv->sName;` |
|       19 |  3571 | `			sEnv.iFlags = pEnv->iFlags;` |
|       19 |  3572 | `			sEnv.nIdx = SXU32_HIGH;` |
|       19 |  3573 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       19 |  3574 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3575 | `				/* Pass by reference */` |
|      ! 0 |  3576 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3577 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3578 | `					);` |
|      ! 0 |  3579 | `			}` |
|        - |  3580 | `			/* Standard pass by value */` |
|       19 |  3581 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       19 |  3582 | `			if( pValue ){` |
|        - |  3583 | `				/* Copy imported value */` |
|       11 |  3584 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3585 | `			}` |
|        - |  3586 | `			/* Insert the imported variable */` |
|       19 |  3587 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       10 |  3588 | `		}` |
|        - |  3589 | `		/* Finally,load the closure name on the stack */` |
|        9 |  3590 | `		pTos++;` |
|        9 |  3591 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3592 | `	}` |
|        9 |  3593 | `	break;` |
|        - |  3594 | `						 }` |
|        - |  3595 | `/*` |
|        - |  3596 | ` * STORE * P2 P3` |
|        - |  3597 | ` *` |
|        - |  3598 | ` * Perform a store (Assignment) operation.` |
|        - |  3599 | ` */` |
|   115845 |  3600 | `case PH7_OP_STORE: {` |
|        - |  3601 | `	ph7_value *pObj;` |
|        - |  3602 | `	SyString sName;` |
|        - |  3603 | `#ifdef UNTRUST` |
|        - |  3604 | `	if( pTos < pStack ){` |
|        - |  3605 | `		goto Abort;` |
|        - |  3606 | `	}` |
|        - |  3607 | `#endif` |
|   231692 |  3608 | `	if( pInstr->iP2 ){` |
|        - |  3609 | `		sxu32 nIdx;` |
|        - |  3610 | `		/* Member store operation */` |
|     2974 |  3611 | `		nIdx = pTos->nIdx;` |
|     2974 |  3612 | `		VmPopOperand(&pTos,1);` |
|     2974 |  3613 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3614 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3615 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3616 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3617 | `		}else{` |
|        - |  3618 | `			/* Point to the desired memory object */` |
|     2970 |  3619 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2970 |  3620 | `			if( pObj ){` |
|        - |  3621 | `				/* Perform the store operation */` |
|     2970 |  3622 | `				PH7_MemObjStore(pTos,pObj);` |
|     1484 |  3623 | `			}` |
|        - |  3624 | `		}` |
|   117333 |  3625 | `		break;` |
|   228720 |  3626 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3627 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3628 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3629 | `			/* Force a string cast */` |
|      ! 0 |  3630 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3631 | `		}` |
|        7 |  3632 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3633 | `		pTos--;` |
|        - |  3634 | `#ifdef UNTRUST` |
|        - |  3635 | `		if( pTos < pStack  ){` |
|        - |  3636 | `			goto Abort;` |
|        - |  3637 | `		}` |
|        - |  3638 | `#endif` |
|        4 |  3639 | `	}else{` |
|   228714 |  3640 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3641 | `	}` |
|        - |  3642 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   228720 |  3643 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   228720 |  3644 | `	if( pObj == 0 ){` |
|      ! 0 |  3645 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3646 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3647 | `		goto Abort;` |
|        - |  3648 | `	}` |
|   228720 |  3649 | `	if( !pInstr->p3 ){` |
|        7 |  3650 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3651 | `	}` |
|        - |  3652 | `	/* Perform the store operation */` |
|   228720 |  3653 | `	PH7_MemObjStore(pTos,pObj);` |
|   228720 |  3654 | `	break;` |
|        - |  3655 | `				   }` |
|        - |  3656 | `/*` |
|        - |  3657 | ` * STORE_IDX:   P1 * P3` |
|        - |  3658 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3659 | ` *` |
|        - |  3660 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3661 | ` */` |
|    83797 |  3662 | `case PH7_OP_STORE_IDX:` |
|        - |  3663 | `case PH7_OP_STORE_IDX_REF: {` |
|   167596 |  3664 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3665 | `	ph7_value *pKey;` |
|        - |  3666 | `	sxu32 nIdx;` |
|   167596 |  3667 | `	if( pInstr->iP1 ){` |
|        - |  3668 | `		/* Key is next on stack */` |
|    58070 |  3669 | `		pKey = pTos;` |
|    58070 |  3670 | `		pTos--;` |
|    29036 |  3671 | `	}else{` |
|   109528 |  3672 | `		pKey = 0;` |
|        - |  3673 | `	}` |
|   167596 |  3674 | `	nIdx = pTos->nIdx;` |
|   167596 |  3675 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3676 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3677 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3678 | `		 * checking true sharing count, then re-add after separation. */` |
|   167544 |  3679 | `		if( nIdx != SXU32_HIGH ){` |
|   167544 |  3680 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   251315 |  3681 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   167544 |  3682 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3683 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3684 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3685 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3686 | `				 * refcounts if the backing array was already separated. */` |
|   167544 |  3687 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   167544 |  3688 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   167544 |  3689 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   167544 |  3690 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   167544 |  3691 | `					pTos->x.pOther = pMap;` |
|    83773 |  3692 | `				}else{` |
|        - |  3693 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3694 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3695 | `					pMap = pCur;` |
|        - |  3696 | `				}` |
|    83773 |  3697 | `			}else{` |
|      ! 0 |  3698 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3699 | `			}` |
|    83773 |  3700 | `		}else{` |
|      ! 0 |  3701 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3702 | `		}` |
|   167544 |  3703 | `		if( pMap->iRef < 2 ){` |
|        - |  3704 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3705 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3706 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3707 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3708 | `			pMap->iRef = 2;` |
|      ! 0 |  3709 | `		}` |
|    83773 |  3710 | `	}else{` |
|        - |  3711 | `		ph7_value *pObj;` |
|       53 |  3712 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3713 | `		if( pObj == 0 ){` |
|      ! 0 |  3714 | `			if( pKey ){` |
|      ! 0 |  3715 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3716 | `			}` |
|      ! 0 |  3717 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3718 | `			break;` |
|        - |  3719 | `		}` |
|        - |  3720 | `		/* Phase#1: Load the array */` |
|       53 |  3721 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3722 | `			VmPopOperand(&pTos,1);` |
|       53 |  3723 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3724 | `				/* Force a string cast */` |
|      ! 0 |  3725 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3726 | `			}` |
|       53 |  3727 | `			if( pKey == 0 ){` |
|        - |  3728 | `				/* Append string */` |
|        3 |  3729 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3730 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3731 | `				}` |
|        2 |  3732 | `			}else{` |
|        - |  3733 | `				sxu32 nOfft;` |
|       51 |  3734 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3735 | `					/* Force an int cast */` |
|       51 |  3736 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3737 | `				}` |
|       51 |  3738 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3739 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3740 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3741 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3742 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3743 | `				}else{` |
|      ! 0 |  3744 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3745 | `						/* Perform an append operation */` |
|      ! 0 |  3746 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3747 | `					}` |
|        - |  3748 | `				}` |
|        - |  3749 | `			}` |
|       53 |  3750 | `			if( pKey ){` |
|       51 |  3751 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3752 | `			}` |
|       53 |  3753 | `			break;` |
|      ! 0 |  3754 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3755 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3756 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3757 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3758 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3759 | `				goto Abort;` |
|        - |  3760 | `			}` |
|      ! 0 |  3761 | `		}` |
|        - |  3762 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3763 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3764 | `	}` |
|   167544 |  3765 | `	VmPopOperand(&pTos,1);` |
|        - |  3766 | `	/* Phase#2: Perform the insertion */` |
|   167544 |  3767 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3768 | `		/* Insertion by reference */` |
|       15 |  3769 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3770 | `	}else{` |
|   167530 |  3771 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3772 | `	}` |
|   167544 |  3773 | `	if( pKey ){` |
|    58020 |  3774 | `		PH7_MemObjRelease(pKey);` |
|    29009 |  3775 | `	}` |
|   167544 |  3776 | `	break;` |
|        - |  3777 | `					   }` |
|        - |  3778 | `/*` |
|        - |  3779 | ` * INCR: P1 * *` |
|        - |  3780 | ` *` |
|        - |  3781 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3782 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3783 | ` * the stack and increment after that.` |
|        - |  3784 | ` */` |
|   151436 |  3785 | `case PH7_OP_INCR:` |
|        - |  3786 | `#ifdef UNTRUST` |
|        - |  3787 | `	if( pTos < pStack ){` |
|        - |  3788 | `		goto Abort;` |
|        - |  3789 | `	}` |
|        - |  3790 | `#endif` |
|   302918 |  3791 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302918 |  3792 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3793 | `			ph7_value *pObj;` |
|   302918 |  3794 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3795 | `				/* Force a numeric cast */` |
|   302918 |  3796 | `				PH7_MemObjToNumeric(pObj);` |
|   302918 |  3797 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3798 | `					pObj->rVal++;` |
|        - |  3799 | `					/* Try to get an integer representation */` |
|      ! 0 |  3800 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3801 | `				}else{` |
|   302918 |  3802 | `					pObj->x.iVal++;` |
|   302918 |  3803 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3804 | `				}` |
|   302918 |  3805 | `				if( pInstr->iP1 ){` |
|        - |  3806 | `					/* Pre-icrement */` |
|       71 |  3807 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3808 | `				}` |
|   151480 |  3809 | `			}` |
|   151482 |  3810 | `		}else{` |
|      ! 0 |  3811 | `			if( pInstr->iP1 ){` |
|        - |  3812 | `				/* Force a numeric cast */` |
|      ! 0 |  3813 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3814 | `				/* Pre-increment */` |
|      ! 0 |  3815 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3816 | `					pTos->rVal++;` |
|        - |  3817 | `					/* Try to get an integer representation */` |
|      ! 0 |  3818 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3819 | `				}else{` |
|      ! 0 |  3820 | `					pTos->x.iVal++;` |
|      ! 0 |  3821 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3822 | `				}` |
|      ! 0 |  3823 | `			}` |
|        - |  3824 | `		}` |
|   151480 |  3825 | `	}` |
|   302918 |  3826 | `	break;` |
|        - |  3827 | `/*` |
|        - |  3828 | ` * DECR: P1 * *` |
|        - |  3829 | ` *` |
|        - |  3830 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3831 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3832 | ` * and decrement after that.` |
|        - |  3833 | ` */` |
|        2 |  3834 | `case PH7_OP_DECR:` |
|        - |  3835 | `#ifdef UNTRUST` |
|        - |  3836 | `	if( pTos < pStack ){` |
|        - |  3837 | `		goto Abort;` |
|        - |  3838 | `	}` |
|        - |  3839 | `#endif` |
|        5 |  3840 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3841 | `		/* Force a numeric cast */` |
|        5 |  3842 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3843 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3844 | `			ph7_value *pObj;` |
|        5 |  3845 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3846 | `				/* Force a numeric cast */` |
|        5 |  3847 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3848 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3849 | `					pObj->rVal--;` |
|        - |  3850 | `					/* Try to get an integer representation */` |
|      ! 0 |  3851 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3852 | `				}else{` |
|        5 |  3853 | `					pObj->x.iVal--;` |
|        5 |  3854 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3855 | `				}` |
|        5 |  3856 | `				if( pInstr->iP1 ){` |
|        - |  3857 | `					/* Pre-icrement */` |
|      ! 0 |  3858 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3859 | `				}` |
|        2 |  3860 | `			}` |
|        3 |  3861 | `		}else{` |
|      ! 0 |  3862 | `			if( pInstr->iP1 ){` |
|        - |  3863 | `				/* Pre-increment */` |
|      ! 0 |  3864 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3865 | `					pTos->rVal--;` |
|        - |  3866 | `					/* Try to get an integer representation */` |
|      ! 0 |  3867 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3868 | `				}else{` |
|      ! 0 |  3869 | `					pTos->x.iVal--;` |
|      ! 0 |  3870 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3871 | `				}` |
|      ! 0 |  3872 | `			}` |
|        - |  3873 | `		}` |
|        2 |  3874 | `	}` |
|        5 |  3875 | `	break;` |
|        - |  3876 | `/*` |
|        - |  3877 | ` * UMINUS: * * *` |
|        - |  3878 | ` *` |
|        - |  3879 | ` * Perform a unary minus operation.` |
|        - |  3880 | ` */` |
|    24374 |  3881 | `case PH7_OP_UMINUS:` |
|        - |  3882 | `#ifdef UNTRUST` |
|        - |  3883 | `	if( pTos < pStack ){` |
|        - |  3884 | `		goto Abort;` |
|        - |  3885 | `	}` |
|        - |  3886 | `#endif` |
|        - |  3887 | `	/* Force a numeric (integer,real or both) cast */` |
|    48750 |  3888 | `	PH7_MemObjToNumeric(pTos);` |
|    48750 |  3889 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3890 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3891 | `	}` |
|    48750 |  3892 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    48720 |  3893 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    24359 |  3894 | `	}` |
|    48750 |  3895 | `	break;` |
|        - |  3896 | `/*` |
|        - |  3897 | ` * UPLUS: * * *` |
|        - |  3898 | ` *` |
|        - |  3899 | ` * Perform a unary plus operation.` |
|        - |  3900 | ` */` |
|       17 |  3901 | `case PH7_OP_UPLUS:` |
|        - |  3902 | `#ifdef UNTRUST` |
|        - |  3903 | `	if( pTos < pStack ){` |
|        - |  3904 | `		goto Abort;` |
|        - |  3905 | `	}` |
|        - |  3906 | `#endif` |
|        - |  3907 | `	/* Force a numeric (integer,real or both) cast */` |
|       35 |  3908 | `	PH7_MemObjToNumeric(pTos);` |
|       35 |  3909 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3910 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3911 | `	}` |
|       35 |  3912 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       35 |  3913 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       17 |  3914 | `	}` |
|       35 |  3915 | `	break;` |
|        - |  3916 | `/*` |
|        - |  3917 | ` * OP_LNOT: * * *` |
|        - |  3918 | ` *` |
|        - |  3919 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3920 | ` * with its complement.` |
|        - |  3921 | ` */` |
|    40019 |  3922 | `case PH7_OP_LNOT:` |
|        - |  3923 | `#ifdef UNTRUST` |
|        - |  3924 | `	if( pTos < pStack ){` |
|        - |  3925 | `		goto Abort;` |
|        - |  3926 | `	}` |
|        - |  3927 | `#endif` |
|        - |  3928 | `	/* Force a boolean cast */` |
|    80084 |  3929 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3930 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3931 | `	}` |
|    80084 |  3932 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80084 |  3933 | `	break;` |
|        - |  3934 | `/*` |
|        - |  3935 | ` * OP_BITNOT: * * *` |
|        - |  3936 | ` *` |
|        - |  3937 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3938 | ` * with its ones-complement.` |
|        - |  3939 | ` */` |
|       13 |  3940 | `case PH7_OP_BITNOT:` |
|        - |  3941 | `#ifdef UNTRUST` |
|        - |  3942 | `	if( pTos < pStack ){` |
|        - |  3943 | `		goto Abort;` |
|        - |  3944 | `	}` |
|        - |  3945 | `#endif` |
|        - |  3946 | `	/* Force an integer cast */` |
|       28 |  3947 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3948 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3949 | `	}` |
|       28 |  3950 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  3951 | `	break;` |
|        - |  3952 | `/* OP_MUL * * *` |
|        - |  3953 | ` * OP_MUL_STORE * * *` |
|        - |  3954 | ` *` |
|        - |  3955 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3956 | ` * and push the result back onto the stack.` |
|        - |  3957 | ` */` |
|     1249 |  3958 | `case PH7_OP_MUL:` |
|        - |  3959 | `case PH7_OP_MUL_STORE: {` |
|     2500 |  3960 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3961 | `	/* Force the operand to be numeric */` |
|        - |  3962 | `#ifdef UNTRUST` |
|        - |  3963 | `	if( pNos < pStack ){` |
|        - |  3964 | `		goto Abort;` |
|        - |  3965 | `	}` |
|        - |  3966 | `#endif` |
|     2500 |  3967 | `	PH7_MemObjToNumeric(pTos);` |
|     2500 |  3968 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3969 | `	/* Perform the requested operation */` |
|     2500 |  3970 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3971 | `		/* Floating point arithemic */` |
|        - |  3972 | `		ph7_real a,b,r;` |
|       17 |  3973 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3974 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3975 | `		}` |
|       17 |  3976 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3977 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3978 | `		}` |
|       17 |  3979 | `		a = pNos->rVal;` |
|       17 |  3980 | `		b = pTos->rVal;` |
|       17 |  3981 | `		r = a * b;` |
|        - |  3982 | `		/* Push the result */` |
|       17 |  3983 | `		pNos->rVal = r;` |
|       17 |  3984 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3985 | `		/* Try to get an integer representation */` |
|       17 |  3986 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3987 | `	}else{` |
|        - |  3988 | `		/* Integer arithmetic */` |
|        - |  3989 | `		sxi64 a,b,r;` |
|     2484 |  3990 | `		a = pNos->x.iVal;` |
|     2484 |  3991 | `		b = pTos->x.iVal;` |
|     2484 |  3992 | `		r = a * b;` |
|        - |  3993 | `		/* Push the result */` |
|     2484 |  3994 | `		pNos->x.iVal = r;` |
|     2484 |  3995 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3996 | `	}` |
|     2500 |  3997 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3998 | `		ph7_value *pObj;` |
|       27 |  3999 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4000 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       27 |  4001 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  4002 | `			PH7_MemObjStore(pNos,pObj);` |
|       13 |  4003 | `		}` |
|       13 |  4004 | `	}` |
|     2500 |  4005 | `	VmPopOperand(&pTos,1);` |
|     2500 |  4006 | `	break;` |
|        - |  4007 | `				 }` |
|        - |  4008 | `/* OP_ADD * * *` |
|        - |  4009 | ` *` |
|        - |  4010 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4011 | ` * and push the result back onto the stack.` |
|        - |  4012 | ` */` |
|      452 |  4013 | `case PH7_OP_ADD:{` |
|      906 |  4014 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4015 | `#ifdef UNTRUST` |
|        - |  4016 | `	if( pNos < pStack ){` |
|        - |  4017 | `		goto Abort;` |
|        - |  4018 | `	}` |
|        - |  4019 | `#endif` |
|        - |  4020 | `	/* Perform the addition */` |
|      906 |  4021 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      906 |  4022 | `	VmPopOperand(&pTos,1);` |
|      906 |  4023 | `	break;` |
|        - |  4024 | `				}` |
|        - |  4025 | `/*` |
|        - |  4026 | ` * OP_ADD_STORE * * *` |
|        - |  4027 | ` *` |
|        - |  4028 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4029 | ` * and push the result back onto the stack.` |
|        - |  4030 | ` */` |
|      495 |  4031 | `case PH7_OP_ADD_STORE:{` |
|      992 |  4032 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4033 | `	ph7_value *pObj;` |
|        - |  4034 | `	sxu32 nIdx;` |
|        - |  4035 | `#ifdef UNTRUST` |
|        - |  4036 | `	if( pNos < pStack ){` |
|        - |  4037 | `		goto Abort;` |
|        - |  4038 | `	}` |
|        - |  4039 | `#endif` |
|        - |  4040 | `	/* Perform the addition */` |
|      992 |  4041 | `	nIdx = pTos->nIdx;` |
|      992 |  4042 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4043 | `	/* Peform the store operation */` |
|      992 |  4044 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4045 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      992 |  4046 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      992 |  4047 | `		PH7_MemObjStore(pTos,pObj);` |
|      495 |  4048 | `	}` |
|        - |  4049 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      992 |  4050 | `	PH7_MemObjStore(pTos,pNos);` |
|      992 |  4051 | `	VmPopOperand(&pTos,1);` |
|      992 |  4052 | `	break;` |
|        - |  4053 | `				}` |
|        - |  4054 | `/* OP_SUB * * *` |
|        - |  4055 | ` *` |
|        - |  4056 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4057 | ` * first (what was next on the stack) from the second (the` |
|        - |  4058 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4059 | ` */` |
|      300 |  4060 | `case PH7_OP_SUB: {` |
|      602 |  4061 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4062 | `#ifdef UNTRUST` |
|        - |  4063 | `	if( pNos < pStack ){` |
|        - |  4064 | `		goto Abort;` |
|        - |  4065 | `	}` |
|        - |  4066 | `#endif` |
|      602 |  4067 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4068 | `		/* Floating point arithemic */` |
|        - |  4069 | `		ph7_real a,b,r;` |
|       95 |  4070 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4071 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4072 | `		}` |
|       95 |  4073 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4074 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4075 | `		}` |
|       95 |  4076 | `		a = pNos->rVal;` |
|       95 |  4077 | `		b = pTos->rVal;` |
|       95 |  4078 | `		r = a - b;` |
|        - |  4079 | `		/* Push the result */` |
|       95 |  4080 | `		pNos->rVal = r;` |
|       95 |  4081 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4082 | `		/* Try to get an integer representation */` |
|       95 |  4083 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4084 | `	}else{` |
|        - |  4085 | `		/* Integer arithmetic */` |
|        - |  4086 | `		sxi64 a,b,r;` |
|      508 |  4087 | `		a = pNos->x.iVal;` |
|      508 |  4088 | `		b = pTos->x.iVal;` |
|      508 |  4089 | `		r = a - b;` |
|        - |  4090 | `		/* Push the result */` |
|      508 |  4091 | `		pNos->x.iVal = r;` |
|      508 |  4092 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4093 | `	}` |
|      602 |  4094 | `	VmPopOperand(&pTos,1);` |
|      602 |  4095 | `	break;` |
|        - |  4096 | `				 }` |
|        - |  4097 | `/* OP_SUB_STORE * * *` |
|        - |  4098 | ` *` |
|        - |  4099 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4100 | ` * first (what was next on the stack) from the second (the` |
|        - |  4101 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4102 | ` */` |
|        2 |  4103 | `case PH7_OP_SUB_STORE: {` |
|        5 |  4104 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4105 | `	ph7_value *pObj;` |
|        - |  4106 | `#ifdef UNTRUST` |
|        - |  4107 | `	if( pNos < pStack ){` |
|        - |  4108 | `		goto Abort;` |
|        - |  4109 | `	}` |
|        - |  4110 | `#endif` |
|        5 |  4111 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4112 | `		/* Floating point arithemic */` |
|        - |  4113 | `		ph7_real a,b,r;` |
|      ! 0 |  4114 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4115 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4116 | `		}` |
|      ! 0 |  4117 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4118 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4119 | `		}` |
|      ! 0 |  4120 | `		a = pTos->rVal;` |
|      ! 0 |  4121 | `		b = pNos->rVal;` |
|      ! 0 |  4122 | `		r = a - b;` |
|        - |  4123 | `		/* Push the result */` |
|      ! 0 |  4124 | `		pNos->rVal = r;` |
|      ! 0 |  4125 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4126 | `		/* Try to get an integer representation */` |
|      ! 0 |  4127 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4128 | `	}else{` |
|        - |  4129 | `		/* Integer arithmetic */` |
|        - |  4130 | `		sxi64 a,b,r;` |
|        5 |  4131 | `		a = pTos->x.iVal;` |
|        5 |  4132 | `		b = pNos->x.iVal;` |
|        5 |  4133 | `		r = a - b;` |
|        - |  4134 | `		/* Push the result */` |
|        5 |  4135 | `		pNos->x.iVal = r;` |
|        5 |  4136 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4137 | `	}` |
|        5 |  4138 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4139 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4140 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4141 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4142 | `	}` |
|        5 |  4143 | `	VmPopOperand(&pTos,1);` |
|        5 |  4144 | `	break;` |
|        - |  4145 | `				 }` |
|        - |  4146 |  |
|        - |  4147 | `/*` |
|        - |  4148 | ` * OP_MOD * * *` |
|        - |  4149 | ` *` |
|        - |  4150 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4151 | ` * first (what was next on the stack) from the second (the` |
|        - |  4152 | ` * top of the stack) and push the remainder after division` |
|        - |  4153 | ` * onto the stack.` |
|        - |  4154 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4155 | ` */` |
|      306 |  4156 | `case PH7_OP_MOD:{` |
|      614 |  4157 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4158 | `	sxi64 a,b,r;` |
|        - |  4159 | `#ifdef UNTRUST` |
|        - |  4160 | `	if( pNos < pStack ){` |
|        - |  4161 | `		goto Abort;` |
|        - |  4162 | `	}` |
|        - |  4163 | `#endif` |
|        - |  4164 | `	/* Force the operands to be integer */` |
|      614 |  4165 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4166 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4167 | `	}` |
|      614 |  4168 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4169 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4170 | `	}` |
|        - |  4171 | `	/* Perform the requested operation */` |
|      614 |  4172 | `	a = pNos->x.iVal;` |
|      614 |  4173 | `	b = pTos->x.iVal;` |
|      614 |  4174 | `	if( b == 0 ){` |
|        3 |  4175 | `		r = 0;` |
|        3 |  4176 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4177 | `		/* goto Abort; */` |
|        2 |  4178 | `	}else{` |
|      611 |  4179 | `		r = a%b;` |
|        - |  4180 | `	}` |
|        - |  4181 | `	/* Push the result */` |
|      614 |  4182 | `	pNos->x.iVal = r;` |
|      614 |  4183 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      614 |  4184 | `	VmPopOperand(&pTos,1);` |
|      614 |  4185 | `	break;` |
|        - |  4186 | `				}` |
|        - |  4187 | `/*` |
|        - |  4188 | ` * OP_MOD_STORE * * *` |
|        - |  4189 | ` *` |
|        - |  4190 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4191 | ` * first (what was next on the stack) from the second (the` |
|        - |  4192 | ` * top of the stack) and push the remainder after division` |
|        - |  4193 | ` * onto the stack.` |
|        - |  4194 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4195 | ` */` |
|        1 |  4196 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4197 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4198 | `	ph7_value *pObj;` |
|        - |  4199 | `	sxi64 a,b,r;` |
|        - |  4200 | `#ifdef UNTRUST` |
|        - |  4201 | `	if( pNos < pStack ){` |
|        - |  4202 | `		goto Abort;` |
|        - |  4203 | `	}` |
|        - |  4204 | `#endif` |
|        - |  4205 | `	/* Force the operands to be integer */` |
|        3 |  4206 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4207 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4208 | `	}` |
|        3 |  4209 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4210 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4211 | `	}` |
|        - |  4212 | `	/* Perform the requested operation */` |
|        3 |  4213 | `	a = pTos->x.iVal;` |
|        3 |  4214 | `	b = pNos->x.iVal;` |
|        3 |  4215 | `	if( b == 0 ){` |
|      ! 0 |  4216 | `		r = 0;` |
|      ! 0 |  4217 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4218 | `		/* goto Abort; */` |
|      ! 0 |  4219 | `	}else{` |
|        3 |  4220 | `		r = a%b;` |
|        - |  4221 | `	}` |
|        - |  4222 | `	/* Push the result */` |
|        3 |  4223 | `	pNos->x.iVal = r;` |
|        3 |  4224 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4225 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4226 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4227 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4228 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4229 | `	}` |
|        3 |  4230 | `	VmPopOperand(&pTos,1);` |
|        3 |  4231 | `	break;` |
|        - |  4232 | `				}` |
|        - |  4233 | `/*` |
|        - |  4234 | ` * OP_DIV * * *` |
|        - |  4235 | ` *` |
|        - |  4236 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4237 | ` * first (what was next on the stack) from the second (the` |
|        - |  4238 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4239 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4240 | ` */` |
|       29 |  4241 | `case PH7_OP_DIV:{` |
|       60 |  4242 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4243 | `	ph7_real a,b,r;` |
|        - |  4244 | `#ifdef UNTRUST` |
|        - |  4245 | `	if( pNos < pStack ){` |
|        - |  4246 | `		goto Abort;` |
|        - |  4247 | `	}` |
|        - |  4248 | `#endif` |
|        - |  4249 | `	/* Force the operands to be real */` |
|       60 |  4250 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 |  4251 | `		PH7_MemObjToReal(pTos);` |
|       27 |  4252 | `	}` |
|       60 |  4253 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       22 |  4254 | `		PH7_MemObjToReal(pNos);` |
|       10 |  4255 | `	}` |
|        - |  4256 | `	/* Perform the requested operation */` |
|       60 |  4257 | `	a = pNos->rVal;` |
|       60 |  4258 | `	b = pTos->rVal;` |
|       60 |  4259 | `	if( b == 0 ){` |
|        - |  4260 | `		/* Division by zero */` |
|        3 |  4261 | `		pNos->rVal = 0;` |
|        3 |  4262 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4263 | `		/* goto Abort; */` |
|        2 |  4264 | `	}else{` |
|       57 |  4265 | `		r = a/b;` |
|        - |  4266 | `		/* Push the result */` |
|       57 |  4267 | `		pNos->rVal = r;` |
|       57 |  4268 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4269 | `		/* Try to get an integer representation */` |
|       57 |  4270 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4271 | `	}` |
|       60 |  4272 | `	VmPopOperand(&pTos,1);` |
|       60 |  4273 | `	break;` |
|        - |  4274 | `				}` |
|        - |  4275 | `/*` |
|        - |  4276 | ` * OP_DIV_STORE * * *` |
|        - |  4277 | ` *` |
|        - |  4278 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4279 | ` * first (what was next on the stack) from the second (the` |
|        - |  4280 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4281 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4282 | ` */` |
|        1 |  4283 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4284 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4285 | `	ph7_value *pObj;` |
|        - |  4286 | `	ph7_real a,b,r;` |
|        - |  4287 | `#ifdef UNTRUST` |
|        - |  4288 | `	if( pNos < pStack ){` |
|        - |  4289 | `		goto Abort;` |
|        - |  4290 | `	}` |
|        - |  4291 | `#endif` |
|        - |  4292 | `	/* Force the operands to be real */` |
|        3 |  4293 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4294 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4295 | `	}` |
|        3 |  4296 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4297 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4298 | `	}` |
|        - |  4299 | `	/* Perform the requested operation */` |
|        3 |  4300 | `	a = pTos->rVal;` |
|        3 |  4301 | `	b = pNos->rVal;` |
|        3 |  4302 | `	if( b == 0 ){` |
|        - |  4303 | `		/* Division by zero */` |
|      ! 0 |  4304 | `		r = 0;` |
|      ! 0 |  4305 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4306 | `		/* goto Abort; */` |
|      ! 0 |  4307 | `	}else{` |
|        3 |  4308 | `		r = a/b;` |
|        - |  4309 | `		/* Push the result */` |
|        3 |  4310 | `		pNos->rVal = r;` |
|        3 |  4311 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4312 | `		/* Try to get an integer representation */` |
|        3 |  4313 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4314 | `	}` |
|        3 |  4315 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4316 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4317 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4318 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4319 | `	}` |
|        3 |  4320 | `	VmPopOperand(&pTos,1);` |
|        3 |  4321 | `	break;` |
|        - |  4322 | `				}` |
|        - |  4323 | `/* OP_BAND * * *` |
|        - |  4324 | ` *` |
|        - |  4325 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4326 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4327 | ` * two elements.` |
|        - |  4328 | `*/` |
|        - |  4329 | `/* OP_BOR * * *` |
|        - |  4330 | ` *` |
|        - |  4331 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4332 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4333 | ` * two elements.` |
|        - |  4334 | ` */` |
|        - |  4335 | `/* OP_BXOR * * *` |
|        - |  4336 | ` *` |
|        - |  4337 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4338 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4339 | ` * two elements.` |
|        - |  4340 | ` */` |
|       44 |  4341 | `case PH7_OP_BAND:` |
|        - |  4342 | `case PH7_OP_BOR:` |
|        - |  4343 | `case PH7_OP_BXOR:{` |
|       90 |  4344 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4345 | `	sxi64 a,b,r;` |
|        - |  4346 | `#ifdef UNTRUST` |
|        - |  4347 | `	if( pNos < pStack ){` |
|        - |  4348 | `		goto Abort;` |
|        - |  4349 | `	}` |
|        - |  4350 | `#endif` |
|        - |  4351 | `	/* Force the operands to be integer */` |
|       90 |  4352 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4353 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4354 | `	}` |
|       90 |  4355 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4356 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4357 | `	}` |
|        - |  4358 | `	/* Perform the requested operation */` |
|       90 |  4359 | `	a = pNos->x.iVal;` |
|       90 |  4360 | `	b = pTos->x.iVal;` |
|       90 |  4361 | `	switch(pInstr->iOp){` |
|        7 |  4362 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4363 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4364 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4365 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4366 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4367 | `	case PH7_OP_BAND:` |
|       62 |  4368 | `	default:          r = a&b; break;` |
|        - |  4369 | `	}` |
|        - |  4370 | `	/* Push the result */` |
|       90 |  4371 | `	pNos->x.iVal = r;` |
|       90 |  4372 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4373 | `	VmPopOperand(&pTos,1);` |
|       90 |  4374 | `	break;` |
|        - |  4375 | `				 }` |
|        - |  4376 | `/* OP_BAND_STORE * * *` |
|        - |  4377 | ` *` |
|        - |  4378 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4379 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4380 | ` * two elements.` |
|        - |  4381 | `*/` |
|        - |  4382 | `/* OP_BOR_STORE * * *` |
|        - |  4383 | ` *` |
|        - |  4384 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4385 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4386 | ` * two elements.` |
|        - |  4387 | ` */` |
|        - |  4388 | `/* OP_BXOR_STORE * * *` |
|        - |  4389 | ` *` |
|        - |  4390 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4391 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4392 | ` * two elements.` |
|        - |  4393 | ` */` |
|       10 |  4394 | `case PH7_OP_BAND_STORE:` |
|        - |  4395 | `case PH7_OP_BOR_STORE:` |
|        - |  4396 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4397 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4398 | `	ph7_value *pObj;` |
|        - |  4399 | `	sxi64 a,b,r;` |
|        - |  4400 | `#ifdef UNTRUST` |
|        - |  4401 | `	if( pNos < pStack ){` |
|        - |  4402 | `		goto Abort;` |
|        - |  4403 | `	}` |
|        - |  4404 | `#endif` |
|        - |  4405 | `	/* Force the operands to be integer */` |
|       21 |  4406 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4407 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4408 | `	}` |
|       21 |  4409 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4410 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4411 | `	}` |
|        - |  4412 | `	/* Perform the requested operation */` |
|       21 |  4413 | `	a = pTos->x.iVal;` |
|       21 |  4414 | `	b = pNos->x.iVal;` |
|       21 |  4415 | `	switch(pInstr->iOp){` |
|        3 |  4416 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4417 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4418 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4419 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4420 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4421 | `	case PH7_OP_BAND:` |
|        7 |  4422 | `	default:          r = a&b; break;` |
|        - |  4423 | `	}` |
|        - |  4424 | `	/* Push the result */` |
|       21 |  4425 | `	pNos->x.iVal = r;` |
|       21 |  4426 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4427 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4428 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4429 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4430 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4431 | `	}` |
|       21 |  4432 | `	VmPopOperand(&pTos,1);` |
|       21 |  4433 | `	break;` |
|        - |  4434 | `				 }` |
|        - |  4435 | `/* OP_SHL * * *` |
|        - |  4436 | ` *` |
|        - |  4437 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4438 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4439 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4440 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4441 | ` */` |
|        - |  4442 | `/* OP_SHR * * *` |
|        - |  4443 | ` *` |
|        - |  4444 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4445 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4446 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4447 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4448 | ` */` |
|       12 |  4449 | `case PH7_OP_SHL:` |
|        - |  4450 | `case PH7_OP_SHR: {` |
|       25 |  4451 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4452 | `	sxi64 a,r;` |
|        - |  4453 | `	sxi32 b;` |
|        - |  4454 | `#ifdef UNTRUST` |
|        - |  4455 | `	if( pNos < pStack ){` |
|        - |  4456 | `		goto Abort;` |
|        - |  4457 | `	}` |
|        - |  4458 | `#endif` |
|        - |  4459 | `	/* Force the operands to be integer */` |
|       25 |  4460 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4461 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4462 | `	}` |
|       25 |  4463 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4464 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4465 | `	}` |
|        - |  4466 | `	/* Perform the requested operation */` |
|       25 |  4467 | `	a = pNos->x.iVal;` |
|       25 |  4468 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4469 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4470 | `		r = a << b;` |
|        8 |  4471 | `	}else{` |
|       11 |  4472 | `		r = a >> b;` |
|        - |  4473 | `	}` |
|        - |  4474 | `	/* Push the result */` |
|       25 |  4475 | `	pNos->x.iVal = r;` |
|       25 |  4476 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4477 | `	VmPopOperand(&pTos,1);` |
|       25 |  4478 | `	break;` |
|        - |  4479 | `				 }` |
|        - |  4480 | `/*  OP_SHL_STORE * * *` |
|        - |  4481 | ` *` |
|        - |  4482 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4483 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4484 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4485 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4486 | ` */` |
|        - |  4487 | `/* OP_SHR_STORE * * *` |
|        - |  4488 | ` *` |
|        - |  4489 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4490 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4491 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4492 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4493 | ` */` |
|        9 |  4494 | `case PH7_OP_SHL_STORE:` |
|        - |  4495 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4496 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4497 | `	ph7_value *pObj;` |
|        - |  4498 | `	sxi64 a,r;` |
|        - |  4499 | `	sxi32 b;` |
|        - |  4500 | `#ifdef UNTRUST` |
|        - |  4501 | `	if( pNos < pStack ){` |
|        - |  4502 | `		goto Abort;` |
|        - |  4503 | `	}` |
|        - |  4504 | `#endif` |
|        - |  4505 | `	/* Force the operands to be integer */` |
|       19 |  4506 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4507 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4508 | `	}` |
|       19 |  4509 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4510 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4511 | `	}` |
|        - |  4512 | `	/* Perform the requested operation */` |
|       19 |  4513 | `	a = pTos->x.iVal;` |
|       19 |  4514 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4515 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4516 | `		r = a << b;` |
|        5 |  4517 | `	}else{` |
|       11 |  4518 | `		r = a >> b;` |
|        - |  4519 | `	}` |
|        - |  4520 | `	/* Push the result */` |
|       19 |  4521 | `	pNos->x.iVal = r;` |
|       19 |  4522 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4523 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4524 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4525 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  4526 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  4527 | `	}` |
|       19 |  4528 | `	VmPopOperand(&pTos,1);` |
|       19 |  4529 | `	break;` |
|        - |  4530 | `				 }` |
|        - |  4531 | `/* CAT:  P1 * *` |
|        - |  4532 | ` *` |
|        - |  4533 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4534 | ` * back.` |
|        - |  4535 | ` */` |
|    63579 |  4536 | `case PH7_OP_CAT:{` |
|        - |  4537 | `	ph7_value *pNos,*pCur;` |
|   127160 |  4538 | `	if( pInstr->iP1 < 1 ){` |
|   100108 |  4539 | `		pNos = &pTos[-1];` |
|    50055 |  4540 | `	}else{` |
|    27054 |  4541 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4542 | `	}` |
|        - |  4543 | `#ifdef UNTRUST` |
|        - |  4544 | `	if( pNos < pStack ){` |
|        - |  4545 | `		goto Abort;` |
|        - |  4546 | `	}` |
|        - |  4547 | `#endif` |
|        - |  4548 | `	/* Force a string cast */` |
|   127160 |  4549 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1362 |  4550 | `		PH7_MemObjToString(pNos);` |
|      680 |  4551 | `	}` |
|   127160 |  4552 | `	pCur = &pNos[1];` |
|   256370 |  4553 | `	while( pCur <= pTos ){` |
|   129212 |  4554 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50674 |  4555 | `			PH7_MemObjToString(pCur);` |
|    25336 |  4556 | `		}` |
|        - |  4557 | `		/* Perform the concatenation */` |
|   129212 |  4558 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   129174 |  4559 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64586 |  4560 | `		}` |
|   129212 |  4561 | `		SyBlobRelease(&pCur->sBlob);` |
|   129212 |  4562 | `		pCur++;` |
|        2 |  4563 | `	}` |
|   127160 |  4564 | `	pTos = pNos;` |
|   127160 |  4565 | `	break;` |
|        - |  4566 | `				}` |
|        - |  4567 | `/*  CAT_STORE: * * *` |
|        - |  4568 | ` *` |
|        - |  4569 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4570 | ` * back.` |
|        - |  4571 | ` */` |
|     3386 |  4572 | `case PH7_OP_CAT_STORE:{` |
|     6774 |  4573 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4574 | `	ph7_value *pObj;` |
|        - |  4575 | `#ifdef UNTRUST` |
|        - |  4576 | `	if( pNos < pStack ){` |
|        - |  4577 | `		goto Abort;` |
|        - |  4578 | `	}` |
|        - |  4579 | `#endif` |
|     6774 |  4580 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4581 | `		/* Force a string cast */` |
|      ! 0 |  4582 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4583 | `	}` |
|     6774 |  4584 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4585 | `		/* Force a string cast */` |
|      ! 0 |  4586 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4587 | `	}` |
|        - |  4588 | `	/* Perform the concatenation (Reverse order) */` |
|     6774 |  4589 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6774 |  4590 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3386 |  4591 | `	}` |
|        - |  4592 | `	/* Perform the store operation */` |
|     6774 |  4593 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4594 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6774 |  4595 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6774 |  4596 | `		PH7_MemObjStore(pTos,pObj);` |
|     3386 |  4597 | `	}` |
|     6774 |  4598 | `	PH7_MemObjStore(pTos,pNos);` |
|     6774 |  4599 | `	VmPopOperand(&pTos,1);` |
|     6774 |  4600 | `	break;` |
|        - |  4601 | `				}` |
|        - |  4602 | `/* OP_AND: * * *` |
|        - |  4603 | ` *` |
|        - |  4604 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4605 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4606 | ` * stack.` |
|        - |  4607 | ` */` |
|        - |  4608 | `/* OP_OR: * * *` |
|        - |  4609 | ` *` |
|        - |  4610 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4611 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4612 | ` * stack.` |
|        - |  4613 | ` */` |
|    94953 |  4614 | `case PH7_OP_LAND:` |
|        - |  4615 | `case PH7_OP_LOR: {` |
|   189952 |  4616 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4617 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4618 | `#ifdef UNTRUST` |
|        - |  4619 | `	if( pNos < pStack ){` |
|        - |  4620 | `		goto Abort;` |
|        - |  4621 | `	}` |
|        - |  4622 | `#endif` |
|        - |  4623 | `	/* Force a boolean cast */` |
|   189952 |  4624 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4625 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4626 | `	}` |
|   189952 |  4627 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4628 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4629 | `	}` |
|   189952 |  4630 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   189952 |  4631 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   189952 |  4632 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4633 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    87434 |  4634 | `		v1 = and_logic[v1*3+v2];` |
|    43740 |  4635 | `	}else{` |
|        - |  4636 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102520 |  4637 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4638 | `	}` |
|   189952 |  4639 | `	if( v1 == 2 ){` |
|      ! 0 |  4640 | `		v1 = 1;` |
|      ! 0 |  4641 | `	}` |
|   189952 |  4642 | `	VmPopOperand(&pTos,1);` |
|   189952 |  4643 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   189952 |  4644 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   189952 |  4645 | `	break;` |
|        - |  4646 | `				 }` |
|        - |  4647 | `/*` |
|        - |  4648 | ` * OP_NULLC: * * *` |
|        - |  4649 | ` * Null coalescing operator '??'.` |
|        - |  4650 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4651 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4652 | ` */` |
|        - |  4653 | `/*` |
|        - |  4654 | ` * OP_NULLC: * P2 *` |
|        - |  4655 | ` * Short-circuit null coalescing '??'.` |
|        - |  4656 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4657 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4658 | ` */` |
|       19 |  4659 | `case PH7_OP_NULLC: {` |
|        - |  4660 | `#ifdef UNTRUST` |
|        - |  4661 | `	if( pTos < pStack ){` |
|        - |  4662 | `		goto Abort;` |
|        - |  4663 | `	}` |
|        - |  4664 | `#endif` |
|       40 |  4665 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4666 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  4667 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  4668 | `	}else{` |
|        - |  4669 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  4670 | `		VmPopOperand(&pTos, 1);` |
|        - |  4671 | `	}` |
|       40 |  4672 | `	break;` |
|        - |  4673 |  |
|        - |  4674 | `/*` |
|        - |  4675 | ` * OP_SPREAD: * * *` |
|        - |  4676 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4677 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4678 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4679 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4680 | ` */` |
|        7 |  4681 | `case PH7_OP_SPREAD: {` |
|        - |  4682 | `#ifdef UNTRUST` |
|        - |  4683 | `	if( pTos < pStack ){` |
|        - |  4684 | `		goto Abort;` |
|        - |  4685 | `	}` |
|        - |  4686 | `#endif` |
|       15 |  4687 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4688 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4689 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4690 | `		if( nEntry == 0 ){` |
|        - |  4691 | `			/* Empty array — remove from stack */` |
|        3 |  4692 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4693 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4694 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4695 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4696 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4697 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4698 | `				VM_STACK_GUARD);` |
|      ! 0 |  4699 | `		}else{` |
|        - |  4700 | `			ph7_hashmap_node *pNode2;` |
|        - |  4701 | `			ph7_value *pElem;` |
|        - |  4702 | `			sxu32 i;` |
|        - |  4703 | `			/* Overwrite TOS with first element */` |
|       13 |  4704 | `			pNode2 = pMap->pFirst;` |
|       13 |  4705 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4706 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4707 | `			if( pElem ){` |
|       13 |  4708 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4709 | `			}` |
|       13 |  4710 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4711 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4712 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4713 | `			pNode2 = pNode2->pPrev;` |
|        - |  4714 | `			/* Push remaining elements */` |
|       33 |  4715 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4716 | `				pTos++;` |
|       21 |  4717 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4718 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4719 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4720 | `				if( pElem ){` |
|       21 |  4721 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4722 | `				}` |
|       21 |  4723 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4724 | `			}` |
|       13 |  4725 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4726 | `		}` |
|        7 |  4727 | `	}` |
|        - |  4728 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4729 | `	break;` |
|        - |  4730 |  |
|        - |  4731 | `/* OP_LXOR: * * *` |
|        - |  4732 | ` *` |
|        - |  4733 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4734 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4735 | ` * stack.` |
|        - |  4736 | ` * According to the PHP language reference manual:` |
|        - |  4737 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4738 | ` *  TRUE,but not both.` |
|        - |  4739 | ` */` |
|        5 |  4740 | `case PH7_OP_LXOR:{` |
|       11 |  4741 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4742 | `	sxi32 v = 0;` |
|        - |  4743 | `#ifdef UNTRUST` |
|        - |  4744 | `	if( pNos < pStack ){` |
|        - |  4745 | `		goto Abort;` |
|        - |  4746 | `	}` |
|        - |  4747 | `#endif` |
|        - |  4748 | `	/* Force a boolean cast */` |
|       11 |  4749 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4750 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4751 | `	}` |
|       11 |  4752 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4753 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4754 | `	}` |
|       11 |  4755 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4756 | `		v = 1;` |
|        3 |  4757 | `	}` |
|       11 |  4758 | `	VmPopOperand(&pTos,1);` |
|       11 |  4759 | `	pTos->x.iVal = v;` |
|       11 |  4760 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4761 | `	break;` |
|        - |  4762 | `				 }` |
|        - |  4763 | `/* OP_EQ P1 P2 P3` |
|        - |  4764 | ` *` |
|        - |  4765 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4766 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4767 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4768 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4769 | ` */` |
|        - |  4770 | `/* OP_NEQ P1 P2 P3` |
|        - |  4771 | ` *` |
|        - |  4772 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4773 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4774 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4775 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4776 | ` */` |
|     3996 |  4777 | `case PH7_OP_EQ:` |
|        - |  4778 | `case PH7_OP_NEQ: {` |
|     7994 |  4779 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4780 | `	/* Perform the comparison and act accordingly */` |
|        - |  4781 | `#ifdef UNTRUST` |
|        - |  4782 | `	if( pNos < pStack ){` |
|        - |  4783 | `		goto Abort;` |
|        - |  4784 | `	}` |
|        - |  4785 | `#endif` |
|     7994 |  4786 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7994 |  4787 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4788 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7985 |  4789 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7950 |  4790 | `		rc = rc == 0;` |
|     3976 |  4791 | `	}else{` |
|       28 |  4792 | `		rc = rc != 0;` |
|        - |  4793 | `	}` |
|     7994 |  4794 | `	VmPopOperand(&pTos,1);` |
|     7994 |  4795 | `	if( !pInstr->iP2 ){` |
|        - |  4796 | `		/* Push comparison result without taking the jump */` |
|     7994 |  4797 | `		PH7_MemObjRelease(pTos);` |
|     7994 |  4798 | `		pTos->x.iVal = rc;` |
|        - |  4799 | `		/* Invalidate any prior representation */` |
|     7994 |  4800 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3998 |  4801 | `	}else{` |
|      ! 0 |  4802 | `		if( rc ){` |
|        - |  4803 | `			/* Jump to the desired location */` |
|      ! 0 |  4804 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4805 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4806 | `		}` |
|        - |  4807 | `	}` |
|     7994 |  4808 | `	break;` |
|        - |  4809 | `				 }` |
|        - |  4810 | `/* OP_TEQ P1 P2 *` |
|        - |  4811 | ` *` |
|        - |  4812 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4813 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4814 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4815 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4816 | ` */` |
|   134077 |  4817 | `case PH7_OP_TEQ: {` |
|   268156 |  4818 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4819 | `	/* Perform the comparison and act accordingly */` |
|        - |  4820 | `#ifdef UNTRUST` |
|        - |  4821 | `	if( pNos < pStack ){` |
|        - |  4822 | `		goto Abort;` |
|        - |  4823 | `	}` |
|        - |  4824 | `#endif` |
|   268156 |  4825 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   268156 |  4826 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4827 | `		rc = 0;` |
|        2 |  4828 | `	}else{` |
|   268154 |  4829 | `		rc = rc == 0;` |
|        - |  4830 | `	}` |
|   268156 |  4831 | `	VmPopOperand(&pTos,1);` |
|   268156 |  4832 | `	if( !pInstr->iP2 ){` |
|        - |  4833 | `		/* Push comparison result without taking the jump */` |
|   268156 |  4834 | `		PH7_MemObjRelease(pTos);` |
|   268156 |  4835 | `		pTos->x.iVal = rc;` |
|        - |  4836 | `		/* Invalidate any prior representation */` |
|   268156 |  4837 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   134079 |  4838 | `	}else{` |
|      ! 0 |  4839 | `		if( rc ){` |
|        - |  4840 | `			/* Jump to the desired location */` |
|      ! 0 |  4841 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4842 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4843 | `		}` |
|        - |  4844 | `	}` |
|   268156 |  4845 | `	break;` |
|        - |  4846 | `				 }` |
|        - |  4847 | `/* OP_TNE P1 P2 *` |
|        - |  4848 | ` *` |
|        - |  4849 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4850 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4851 | ` * instruction.` |
|        - |  4852 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4853 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4854 | ` *` |
|        - |  4855 | ` */` |
|   104602 |  4856 | `case PH7_OP_TNE: {` |
|   209206 |  4857 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4858 | `	/* Perform the comparison and act accordingly */` |
|        - |  4859 | `#ifdef UNTRUST` |
|        - |  4860 | `	if( pNos < pStack ){` |
|        - |  4861 | `		goto Abort;` |
|        - |  4862 | `	}` |
|        - |  4863 | `#endif` |
|   209206 |  4864 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   209206 |  4865 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4866 | `		rc = 1;` |
|        2 |  4867 | `	}else{` |
|   209204 |  4868 | `		rc = rc != 0;` |
|        - |  4869 | `	}` |
|   209206 |  4870 | `	VmPopOperand(&pTos,1);` |
|   209206 |  4871 | `	if( !pInstr->iP2 ){` |
|        - |  4872 | `		/* Push comparison result without taking the jump */` |
|   209206 |  4873 | `		PH7_MemObjRelease(pTos);` |
|   209206 |  4874 | `		pTos->x.iVal = rc;` |
|        - |  4875 | `		/* Invalidate any prior representation */` |
|   209206 |  4876 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   104604 |  4877 | `	}else{` |
|      ! 0 |  4878 | `		if( rc ){` |
|        - |  4879 | `			/* Jump to the desired location */` |
|      ! 0 |  4880 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4881 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4882 | `		}` |
|        - |  4883 | `	}` |
|   209206 |  4884 | `	break;` |
|        - |  4885 | `				 }` |
|        - |  4886 | `/* OP_LT P1 P2 P3` |
|        - |  4887 | ` *` |
|        - |  4888 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4889 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4890 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4891 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4892 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4893 | ` *` |
|        - |  4894 | ` */` |
|        - |  4895 | `/* OP_LE P1 P2 P3` |
|        - |  4896 | ` *` |
|        - |  4897 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4898 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4899 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4900 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4901 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4902 | ` *` |
|        - |  4903 | ` */` |
|   102455 |  4904 | `case PH7_OP_LT:` |
|        - |  4905 | `case PH7_OP_LE: {` |
|   204956 |  4906 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4907 | `	/* Perform the comparison and act accordingly */` |
|        - |  4908 | `#ifdef UNTRUST` |
|        - |  4909 | `	if( pNos < pStack ){` |
|        - |  4910 | `		goto Abort;` |
|        - |  4911 | `	}` |
|        - |  4912 | `#endif` |
|   204956 |  4913 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204956 |  4914 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4915 | `		rc = 0;` |
|   204952 |  4916 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      432 |  4917 | `		rc = rc < 1;` |
|      217 |  4918 | `	}else{` |
|   204518 |  4919 | `		rc = rc < 0;` |
|        - |  4920 | `	}` |
|   204956 |  4921 | `	VmPopOperand(&pTos,1);` |
|   204956 |  4922 | `	if( !pInstr->iP2 ){` |
|        - |  4923 | `		/* Push comparison result without taking the jump */` |
|   204956 |  4924 | `		PH7_MemObjRelease(pTos);` |
|   204956 |  4925 | `		pTos->x.iVal = rc;` |
|        - |  4926 | `		/* Invalidate any prior representation */` |
|   204956 |  4927 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102501 |  4928 | `	}else{` |
|      ! 0 |  4929 | `		if( rc ){` |
|        - |  4930 | `			/* Jump to the desired location */` |
|      ! 0 |  4931 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4932 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4933 | `		}` |
|        - |  4934 | `	}` |
|   204956 |  4935 | `	break;` |
|        - |  4936 | `				}` |
|        - |  4937 | `/* OP_GT P1 P2 P3` |
|        - |  4938 | ` *` |
|        - |  4939 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4940 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4941 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4942 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4943 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4944 | ` *` |
|        - |  4945 | ` */` |
|        - |  4946 | `/* OP_GE P1 P2 P3` |
|        - |  4947 | ` *` |
|        - |  4948 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4949 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4950 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4951 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4952 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4953 | ` *` |
|        - |  4954 | ` */` |
|    48773 |  4955 | `case PH7_OP_GT:` |
|        - |  4956 | `case PH7_OP_GE: {` |
|    97548 |  4957 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4958 | `	/* Perform the comparison and act accordingly */` |
|        - |  4959 | `#ifdef UNTRUST` |
|        - |  4960 | `	if( pNos < pStack ){` |
|        - |  4961 | `		goto Abort;` |
|        - |  4962 | `	}` |
|        - |  4963 | `#endif` |
|    97548 |  4964 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97548 |  4965 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4966 | `		rc = 0;` |
|    97544 |  4967 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97390 |  4968 | `		rc = rc >= 0;` |
|    48696 |  4969 | `	}else{` |
|      152 |  4970 | `		rc = rc > 0;` |
|        - |  4971 | `	}` |
|    97548 |  4972 | `	VmPopOperand(&pTos,1);` |
|    97548 |  4973 | `	if( !pInstr->iP2 ){` |
|        - |  4974 | `		/* Push comparison result without taking the jump */` |
|    97548 |  4975 | `		PH7_MemObjRelease(pTos);` |
|    97548 |  4976 | `		pTos->x.iVal = rc;` |
|        - |  4977 | `		/* Invalidate any prior representation */` |
|    97548 |  4978 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48775 |  4979 | `	}else{` |
|      ! 0 |  4980 | `		if( rc ){` |
|        - |  4981 | `			/* Jump to the desired location */` |
|      ! 0 |  4982 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4983 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4984 | `		}` |
|        - |  4985 | `	}` |
|    97548 |  4986 | `	break;` |
|        - |  4987 | `				}` |
|        - |  4988 | `/* OP_SPACESHIP * * *` |
|        - |  4989 | ` *` |
|        - |  4990 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  4991 | ` *   -1 if left < right` |
|        - |  4992 | ` *    0 if left == right` |
|        - |  4993 | ` *    1 if left > right` |
|        - |  4994 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  4995 | ` */` |
|       25 |  4996 | `case PH7_OP_SPACESHIP: {` |
|       51 |  4997 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4998 | `#ifdef UNTRUST` |
|        - |  4999 | `	if( pNos < pStack ){` |
|        - |  5000 | `		goto Abort;` |
|        - |  5001 | `	}` |
|        - |  5002 | `#endif` |
|       51 |  5003 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5004 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5005 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5006 | `		rc = 1;` |
|        4 |  5007 | `	}else{` |
|        - |  5008 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5009 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5010 | `	}` |
|       51 |  5011 | `	VmPopOperand(&pTos,1);` |
|       51 |  5012 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5013 | `	pTos->x.iVal = rc;` |
|       51 |  5014 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5015 | `	break;` |
|        - |  5016 | `				}` |
|        - |  5017 | `/* OP_SEQ P1 P2 *` |
|        - |  5018 | ` * Strict string comparison.` |
|        - |  5019 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5020 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5021 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5022 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5023 | ` * use PH7_OP_EQ.` |
|        - |  5024 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5025 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5026 | ` */` |
|        - |  5027 | `/* OP_SNE P1 P2 *` |
|        - |  5028 | ` * Strict string comparison.` |
|        - |  5029 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5030 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5031 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5032 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5033 | ` * use PH7_OP_EQ.` |
|        - |  5034 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5035 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5036 | ` */` |
|       18 |  5037 | `case PH7_OP_SEQ:` |
|        - |  5038 | `case PH7_OP_SNE: {` |
|       38 |  5039 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5040 | `	SyString s1,s2;` |
|        - |  5041 | `	/* Perform the comparison and act accordingly */` |
|        - |  5042 | `#ifdef UNTRUST` |
|        - |  5043 | `	if( pNos < pStack ){` |
|        - |  5044 | `		goto Abort;` |
|        - |  5045 | `	}` |
|        - |  5046 | `#endif` |
|        - |  5047 | `	/* Force a string cast */` |
|       38 |  5048 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5049 | `		PH7_MemObjToString(pTos);` |
|        2 |  5050 | `	}` |
|       38 |  5051 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5052 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5053 | `	}` |
|       38 |  5054 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5055 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5056 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5057 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5058 | `		rc = rc != 0;` |
|      ! 0 |  5059 | `	}else{` |
|       38 |  5060 | `		rc = rc == 0;` |
|        - |  5061 | `	}` |
|       38 |  5062 | `	VmPopOperand(&pTos,1);` |
|       38 |  5063 | `	if( !pInstr->iP2 ){` |
|        - |  5064 | `		/* Push comparison result without taking the jump */` |
|       38 |  5065 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5066 | `		pTos->x.iVal = rc;` |
|        - |  5067 | `		/* Invalidate any prior representation */` |
|       38 |  5068 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5069 | `	}else{` |
|      ! 0 |  5070 | `		if( rc ){` |
|        - |  5071 | `			/* Jump to the desired location */` |
|      ! 0 |  5072 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5073 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5074 | `		}` |
|        - |  5075 | `	}` |
|       38 |  5076 | `	break;` |
|        - |  5077 | `				 }` |
|        - |  5078 | `/*` |
|        - |  5079 | ` * OP_LOAD_REF * * *` |
|        - |  5080 | ` * Push the index of a referenced object on the stack.` |
|        - |  5081 | ` */` |
|       57 |  5082 | `case PH7_OP_LOAD_REF: {` |
|        - |  5083 | `	sxu32 nIdx;` |
|        - |  5084 | `#ifdef UNTRUST` |
|        - |  5085 | `	if( pTos < pStack ){` |
|        - |  5086 | `		goto Abort;` |
|        - |  5087 | `	}` |
|        - |  5088 | `#endif` |
|        - |  5089 | `	/* Extract memory object index */` |
|      115 |  5090 | `	nIdx = pTos->nIdx;` |
|      115 |  5091 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5092 | `		/* Nullify the object */` |
|       95 |  5093 | `		PH7_MemObjRelease(pTos);` |
|        - |  5094 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5095 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5096 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5097 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5098 | `	}` |
|      115 |  5099 | `	break;` |
|        - |  5100 | `					  }` |
|        - |  5101 | `/*` |
|        - |  5102 | ` * OP_STORE_REF * * P3` |
|        - |  5103 | ` * Perform an assignment operation by reference.` |
|        - |  5104 | ` */` |
|       15 |  5105 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5106 | `	 SyString sName = { 0 , 0 };` |
|        - |  5107 | `	 VmFrame *pFrameLocal;` |
|        - |  5108 | `	SyHashEntry *pEntry;` |
|        - |  5109 | `	sxu32 nIdx;` |
|        - |  5110 | `#ifdef UNTRUST` |
|        - |  5111 | `	if( pTos < pStack ){` |
|        - |  5112 | `		goto Abort;` |
|        - |  5113 | `	}` |
|        - |  5114 | `#endif` |
|       32 |  5115 | `	if( pInstr->p3 == 0 ){` |
|        - |  5116 | `		char *zName;` |
|        - |  5117 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5118 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5119 | `			/* Force a string cast */` |
|      ! 0 |  5120 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5121 | `		}` |
|      ! 0 |  5122 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5123 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5124 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5125 | `			if( zName ){` |
|      ! 0 |  5126 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5127 | `			}` |
|      ! 0 |  5128 | `		}` |
|      ! 0 |  5129 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5130 | `		pTos--;` |
|      ! 0 |  5131 | `	}else{` |
|       32 |  5132 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5133 | `	}` |
|       32 |  5134 | `	nIdx = pTos->nIdx;` |
|       32 |  5135 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5136 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5137 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5138 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5139 | `		}else{` |
|        - |  5140 | `			ph7_value *pObj;` |
|        - |  5141 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5142 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5143 | `			if( pObj == 0 ){` |
|      ! 0 |  5144 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5145 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5146 | `				goto Abort;` |
|        - |  5147 | `			}` |
|        - |  5148 | `			/* Perform the store operation */` |
|      ! 0 |  5149 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5150 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5151 | `		}` |
|       32 |  5152 | `	}else if( sName.nByte > 0){` |
|       32 |  5153 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5154 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5155 | `		}else{` |
|       32 |  5156 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5157 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5158 | `			/* Query the local frame */` |
|       32 |  5159 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5160 | `			if( pEntry ){` |
|      ! 0 |  5161 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5162 | `			}else{` |
|       32 |  5163 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5164 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5165 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5166 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5167 | `				}` |
|       32 |  5168 | `				if( rc == SXRET_OK ){` |
|       32 |  5169 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5170 | `				}` |
|        - |  5171 | `			}` |
|        - |  5172 | `		}` |
|       15 |  5173 | `	}` |
|       32 |  5174 | `	break;` |
|        - |  5175 | `				 }` |
|        - |  5176 | `/*` |
|        - |  5177 | ` * OP_UPLINK P1 * *` |
|        - |  5178 | ` * Link a variable to the top active VM frame.` |
|        - |  5179 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5180 | ` */` |
|       25 |  5181 | `case PH7_OP_UPLINK: {` |
|       52 |  5182 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5183 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5184 | `		SyString sName;` |
|        - |  5185 | `		/* Perform the link */` |
|      104 |  5186 | `		while( pLink <= pTos ){` |
|       54 |  5187 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5188 | `				/* Force a string cast */` |
|      ! 0 |  5189 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5190 | `			}` |
|       54 |  5191 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5192 | `			if( sName.nByte > 0 ){` |
|       54 |  5193 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5194 | `			}` |
|       54 |  5195 | `			pLink++;` |
|        2 |  5196 | `		}` |
|       25 |  5197 | `	}` |
|       52 |  5198 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5199 | `	break;` |
|        - |  5200 | `					}` |
|        - |  5201 | `/*` |
|        - |  5202 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5203 | ` * Push an exception in the corresponding container so that` |
|        - |  5204 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5205 | ` */` |
|       32 |  5206 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5207 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5208 | `	VmFrame *pFrameLocal;` |
|        - |  5209 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5210 | `	pException->iFinallyDone = 0;` |
|       66 |  5211 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5212 | `	/* Create the exception frame */` |
|       66 |  5213 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5214 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5215 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5216 | `		goto Abort;` |
|        - |  5217 | `	}` |
|        - |  5218 | `	/* Mark the special frame */` |
|       66 |  5219 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5220 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5221 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5222 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5223 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5224 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5225 | `	break;` |
|        - |  5226 | `							}` |
|        - |  5227 | `/*` |
|        - |  5228 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5229 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5230 | ` */` |
|       31 |  5231 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5232 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5233 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5234 | `		ph7_exception **apException;` |
|        - |  5235 | `		/* Pop the loaded exception */` |
|       28 |  5236 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5237 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5238 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5239 | `		}` |
|       13 |  5240 | `	}` |
|       64 |  5241 | `	pException->pFrame = 0;` |
|        - |  5242 | `	/* Leave the exception frame */` |
|       64 |  5243 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5244 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5245 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5246 | `		sxi32 rcFinally;` |
|       20 |  5247 | `		pException->iFinallyDone = 1;` |
|       20 |  5248 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5249 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5250 | `			goto Abort;` |
|        - |  5251 | `		}` |
|        9 |  5252 | `	}` |
|       64 |  5253 | `	break;` |
|        - |  5254 | `							}` |
|        - |  5255 |  |
|        - |  5256 | `/*` |
|        - |  5257 | ` * OP_THROW * P2 *` |
|        - |  5258 | ` * Throw an user exception.` |
|        - |  5259 | ` */` |
|       18 |  5260 | `case PH7_OP_THROW: {` |
|       38 |  5261 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5262 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5263 | `#ifdef UNTRUST` |
|        - |  5264 | `	if( pTos < pStack ){` |
|        - |  5265 | `		goto Abort;` |
|        - |  5266 | `	}` |
|        - |  5267 | `#endif` |
|       38 |  5268 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5269 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5270 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5271 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5272 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5273 | `		ph7_class *pException;` |
|        - |  5274 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5275 | `		 */` |
|       38 |  5276 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5277 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5278 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5279 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5280 | `			if( rc == SXERR_ABORT ){` |
|        - |  5281 | `				/* Abort processing immediately */` |
|      ! 0 |  5282 | `				goto Abort;` |
|        - |  5283 | `			}` |
|      ! 0 |  5284 | `		}else{` |
|        - |  5285 | `			/* Throw the exception */` |
|       38 |  5286 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5287 | `			if( rc == SXERR_ABORT ){` |
|        - |  5288 | `				/* Abort processing immediately */` |
|        9 |  5289 | `				goto Abort;` |
|        - |  5290 | `			}` |
|        - |  5291 | `		}` |
|       16 |  5292 | `	}else{` |
|        - |  5293 | `		/* Expecting a class instance */` |
|      ! 0 |  5294 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5295 | `		if( rc == SXERR_ABORT ){` |
|        - |  5296 | `			/* Abort processing immediately */` |
|      ! 0 |  5297 | `			goto Abort;` |
|        - |  5298 | `		}` |
|        - |  5299 | `	}` |
|        - |  5300 | `	/* Pop the top entry */` |
|       30 |  5301 | `	VmPopOperand(&pTos,1);` |
|        - |  5302 | `	/* Perform an unconditional jump */` |
|       30 |  5303 | `	pc = nJump - 1;` |
|       30 |  5304 | `	break;` |
|        - |  5305 | `				   }` |
|        - |  5306 | `/*` |
|        - |  5307 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5308 | ` * Prepare a foreach step.` |
|        - |  5309 | ` */` |
|     5083 |  5310 | `case PH7_OP_FOREACH_INIT: {` |
|    10168 |  5311 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5312 | `	void *pName;` |
|        - |  5313 | `#ifdef UNTRUST` |
|        - |  5314 | `	if( pTos < pStack ){` |
|        - |  5315 | `		goto Abort;` |
|        - |  5316 | `	}` |
|        - |  5317 | `#endif` |
|    10168 |  5318 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5319 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5320 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5321 | `			/* Force a string cast */` |
|      ! 0 |  5322 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5323 | `		}` |
|        - |  5324 | `		/* Duplicate name */` |
|      ! 0 |  5325 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5326 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5327 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5328 | `		}` |
|      ! 0 |  5329 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5330 | `	}` |
|    10168 |  5331 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5332 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5333 | `			/* Force a string cast */` |
|      ! 0 |  5334 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5335 | `		}` |
|        - |  5336 | `		/* Duplicate name */` |
|      ! 0 |  5337 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5338 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5339 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5340 | `		}` |
|      ! 0 |  5341 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5342 | `	}` |
|        - |  5343 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10168 |  5344 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5345 | `		/* Jump out of the loop */` |
|      ! 0 |  5346 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5347 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5348 | `		}` |
|      ! 0 |  5349 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5350 | `	}else{` |
|        - |  5351 | `		ph7_foreach_step *pStep;` |
|    10168 |  5352 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10168 |  5353 | `		if( pStep == 0 ){` |
|      ! 0 |  5354 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5355 | `			/* Jump out of the loop */` |
|      ! 0 |  5356 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5357 | `		}else{` |
|        - |  5358 | `			/* Zero the structure */` |
|    10168 |  5359 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5360 | `			/* Prepare the step */` |
|    10168 |  5361 | `			pStep->iFlags = pInfo->iFlags;` |
|    10168 |  5362 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5363 | `				ph7_hashmap *pMap;` |
|        - |  5364 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5365 | `				 * source array so mutations don't affect other sharers. */` |
|    10140 |  5366 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5367 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5368 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5369 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5370 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5371 | `						 * variable still points at the same hashmap as` |
|        - |  5372 | `						 * the stack value. */` |
|        9 |  5373 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5374 | `							pCur->iRef--;` |
|        9 |  5375 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5376 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5377 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5378 | `						}` |
|        4 |  5379 | `					}` |
|        4 |  5380 | `				}` |
|    10140 |  5381 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5382 | `				/* Reset the internal loop cursor */` |
|    10140 |  5383 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5384 | `				/* Mark the step */` |
|    10140 |  5385 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10140 |  5386 | `				pStep->xIter.pMap = pMap;` |
|    10140 |  5387 | `				pMap->iRef++;` |
|     5071 |  5388 | `			}else{` |
|       30 |  5389 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5390 | `				ph7_class *pIteratorClass;` |
|        - |  5391 | `				/* Check if the object implements Iterator */` |
|       30 |  5392 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5393 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5394 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5395 | `					ph7_class_method *pRewind;` |
|       20 |  5396 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5397 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5398 | `					pThis->iRef++;` |
|       20 |  5399 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5400 | `					if( pRewind ){` |
|       20 |  5401 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5402 | `					}` |
|       11 |  5403 | `				}else{` |
|        - |  5404 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5405 | `					ph7_class *pIterAggClass;` |
|       12 |  5406 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5407 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5408 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5409 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5410 | `						ph7_class_method *pGetIter;` |
|        3 |  5411 | `						int iterAggOk = 0;` |
|        3 |  5412 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5413 | `						if( pGetIter ){` |
|        - |  5414 | `							ph7_value sResult;` |
|        3 |  5415 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5416 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5417 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5418 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5419 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5420 | `									ph7_class_method *pRewind;` |
|        3 |  5421 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5422 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5423 | `									pIterObj->iRef++;` |
|        - |  5424 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5425 | `									pStep->pOwner = pThis;` |
|        3 |  5426 | `									pThis->iRef++;` |
|        3 |  5427 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5428 | `									if( pRewind ){` |
|        3 |  5429 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5430 | `									}` |
|        3 |  5431 | `									iterAggOk = 1;` |
|        1 |  5432 | `								}` |
|        1 |  5433 | `							}` |
|        3 |  5434 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5435 | `						}` |
|        3 |  5436 | `						if( !iterAggOk ){` |
|        - |  5437 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5438 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5439 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5440 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5441 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5442 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5443 | `						}` |
|        2 |  5444 | `					}else{` |
|        - |  5445 | `						/* Plain object iteration via hAttr */` |
|        9 |  5446 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5447 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5448 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5449 | `						pThis->iRef++;` |
|        - |  5450 | `					}` |
|        - |  5451 | `				}` |
|        - |  5452 | `			}` |
|        - |  5453 | `		}` |
|    10168 |  5454 | `		if( pStep ){` |
|    10168 |  5455 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5456 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5457 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5458 | `				/* Jump out of the loop */` |
|      ! 0 |  5459 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5460 | `			}` |
|     5083 |  5461 | `		}` |
|        - |  5462 | `	}` |
|    10168 |  5463 | `	VmPopOperand(&pTos,1);` |
|    10168 |  5464 | `	break;` |
|        - |  5465 | `						  }` |
|        - |  5466 | `/*` |
|        - |  5467 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5468 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5469 | ` */` |
|    82378 |  5470 | `case PH7_OP_FOREACH_STEP: {` |
|   164758 |  5471 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5472 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5473 | `	ph7_value *pValue;` |
|        - |  5474 | `	VmFrame *pFrameLocal;` |
|        - |  5475 | `	/* Peek the last step */` |
|   164758 |  5476 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   164758 |  5477 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   164758 |  5478 | `	pFrameLocal = pVm->pFrame;` |
|   164758 |  5479 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   164758 |  5480 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   164646 |  5481 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5482 | `		ph7_hashmap_node *pNode;` |
|        - |  5483 | `		/* Extract the current node value */` |
|   164646 |  5484 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   164646 |  5485 | `		if( pNode == 0 ){` |
|        - |  5486 | `			/* No more entry to process */` |
|    10138 |  5487 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10138 |  5488 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5489 | `				/* Break the reference with the last element */` |
|        7 |  5490 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5491 | `			}` |
|        - |  5492 | `			/* Automatically reset the loop cursor */` |
|    10138 |  5493 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5494 | `			/* Cleanup the mess left behind */` |
|    10138 |  5495 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10138 |  5496 | `			SySetPop(&pInfo->aStep);` |
|    10138 |  5497 | `			PH7_HashmapUnref(pMap);` |
|     5070 |  5498 | `		}else{` |
|   154510 |  5499 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5500 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5501 | `				if( pKey ){` |
|      416 |  5502 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5503 | `				}` |
|      207 |  5504 | `			}` |
|   154510 |  5505 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5506 | `				SyHashEntry *pEntry;` |
|        - |  5507 | `				/* Pass by reference */` |
|       23 |  5508 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5509 | `				if( pEntry ){` |
|       23 |  5510 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5511 | `				}else{` |
|      ! 0 |  5512 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5513 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5514 | `				}` |
|       12 |  5515 | `			}else{` |
|        - |  5516 | `				/* Make a copy of the entry value */` |
|   154488 |  5517 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   154488 |  5518 | `				if( pValue ){` |
|   154488 |  5519 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    77243 |  5520 | `				}` |
|        - |  5521 | `			}` |
|        2 |  5522 | `		}` |
|    82436 |  5523 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5524 | `		/* Iterator-based iteration.` |
|        - |  5525 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5526 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5527 | `		 */` |
|       90 |  5528 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5529 | `		ph7_class_method *pMethod;` |
|        - |  5530 | `		ph7_value sResult;` |
|       90 |  5531 | `		int isValid = 0;` |
|        - |  5532 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5533 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5534 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5535 | `		}else{` |
|       70 |  5536 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5537 | `			if( pMethod ){` |
|       70 |  5538 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5539 | `			}` |
|        - |  5540 | `		}` |
|        - |  5541 | `		/* Call valid() */` |
|       90 |  5542 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5543 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5544 | `		if( pMethod ){` |
|       90 |  5545 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5546 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5547 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5548 | `		}` |
|       90 |  5549 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5550 | `		if( !isValid ){` |
|        - |  5551 | `			/* Iterator exhausted */` |
|       20 |  5552 | `			pc = pInstr->iP2 - 1;` |
|        - |  5553 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5554 | `			if( pStep->pOwner ){` |
|        3 |  5555 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5556 | `			}` |
|       20 |  5557 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5558 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5559 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5560 | `		}else{` |
|        - |  5561 | `			/* Call current() to get value */` |
|       72 |  5562 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5563 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5564 | `			if( pMethod ){` |
|       72 |  5565 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5566 | `			}` |
|       72 |  5567 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5568 | `			if( pValue ){` |
|       72 |  5569 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5570 | `			}` |
|       72 |  5571 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5572 | `			/* Call key() if needed */` |
|       72 |  5573 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5574 | `				ph7_value sKey;` |
|       35 |  5575 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5576 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5577 | `				if( pMethod ){` |
|       35 |  5578 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5579 | `				}` |
|       35 |  5580 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5581 | `				if( pValue ){` |
|       35 |  5582 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5583 | `				}` |
|       35 |  5584 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5585 | `			}` |
|        - |  5586 | `		}` |
|       46 |  5587 | `	}else{` |
|       25 |  5588 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5589 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5590 | `		SyHashEntry *pEntry;` |
|        - |  5591 | `		/* Point to the next attribute */` |
|       29 |  5592 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5593 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5594 | `			/* Check access permission */` |
|       31 |  5595 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5596 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5597 | `					break; /* Access is granted */` |
|        - |  5598 | `			}` |
|        1 |  5599 | `		}` |
|       25 |  5600 | `		if( pEntry == 0 ){` |
|        - |  5601 | `			/* Clean up the mess left behind */` |
|        9 |  5602 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5603 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5604 | `				/* Break the reference with the last element */` |
|        3 |  5605 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5606 | `			}` |
|        9 |  5607 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5608 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5609 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5610 | `		}else{` |
|       17 |  5611 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5612 | `			ph7_value *pAttrValue;` |
|       17 |  5613 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5614 | `				/* Fill with the current attribute name */` |
|       17 |  5615 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5616 | `				if( pKey ){` |
|       17 |  5617 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5618 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5619 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5620 | `				}` |
|        8 |  5621 | `			}` |
|        - |  5622 | `			/* Extract attribute value */` |
|       17 |  5623 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5624 | `			if( pAttrValue ){` |
|       17 |  5625 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5626 | `					/* Pass by reference */` |
|        3 |  5627 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5628 | `					if( pEntry ){` |
|        3 |  5629 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5630 | `					}else{` |
|      ! 0 |  5631 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5632 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5633 | `					}` |
|        2 |  5634 | `				}else{` |
|        - |  5635 | `					/* Make a copy of the attribute value */` |
|       15 |  5636 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5637 | `					if( pValue ){` |
|       15 |  5638 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5639 | `					}` |
|        - |  5640 | `				}` |
|        8 |  5641 | `			}` |
|        - |  5642 | `		}` |
|        - |  5643 | `	}` |
|   164758 |  5644 | `	break;` |
|        - |  5645 | `						  }` |
|        - |  5646 | `/*` |
|        - |  5647 | ` * OP_MEMBER P1 P2` |
|        - |  5648 | ` * Load class attribute/method on the stack.` |
|        - |  5649 | ` */` |
|     2210 |  5650 | `case PH7_OP_MEMBER: {` |
|        - |  5651 | `	ph7_class_instance *pThis;` |
|        - |  5652 | `	ph7_value *pNos;` |
|        - |  5653 | `	SyString sName;` |
|     4422 |  5654 | `	if( !pInstr->iP1 ){` |
|     4280 |  5655 | `		pNos = &pTos[-1];` |
|        - |  5656 | `#ifdef UNTRUST` |
|        - |  5657 | `		if( pNos < pStack ){` |
|        - |  5658 | `			goto Abort;` |
|        - |  5659 | `		}` |
|        - |  5660 | `#endif` |
|     4280 |  5661 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5662 | `			ph7_class *pClass;` |
|        - |  5663 | `			/* Class already instantiated */` |
|     4280 |  5664 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5665 | `			/* Point to the instantiated class */` |
|     4280 |  5666 | `			pClass = pThis->pClass;` |
|        - |  5667 | `			/* Extract attribute name first */` |
|     4280 |  5668 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4280 |  5669 | `			if( pInstr->iP2 ){` |
|        - |  5670 | `				/* Method call */` |
|      436 |  5671 | `				ph7_class_method *pMeth = 0;` |
|      436 |  5672 | `				if( sName.nByte > 0 ){` |
|        - |  5673 | `					/* Extract the target method */` |
|      436 |  5674 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      217 |  5675 | `				}` |
|      436 |  5676 | `				if( pMeth == 0 ){` |
|      ! 0 |  5677 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5678 | `						&pClass->sName,&sName` |
|        - |  5679 | `						);` |
|        - |  5680 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5681 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5682 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5683 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5684 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5685 | `				}else{` |
|        - |  5686 | `					/* Push method name on the stack */` |
|      436 |  5687 | `					PH7_MemObjRelease(pTos);` |
|      436 |  5688 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      436 |  5689 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5690 | `				}` |
|      436 |  5691 | `				pTos->nIdx = SXU32_HIGH;` |
|      219 |  5692 | `			}else{` |
|        - |  5693 | `				/* Attribute access */` |
|     3846 |  5694 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5695 | `				SyHashEntry *pEntry;` |
|        - |  5696 | `				/* Extract the target attribute */` |
|     3846 |  5697 | `				if( sName.nByte > 0 ){` |
|     3846 |  5698 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3846 |  5699 | `					if( pEntry ){` |
|        - |  5700 | `						/* Point to the attribute value */` |
|     3844 |  5701 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1921 |  5702 | `					}` |
|     1922 |  5703 | `				}` |
|     3846 |  5704 | `				if( pObjAttr == 0 ){` |
|        - |  5705 | `					/* No such attribute,load null */` |
|        4 |  5706 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5707 | `						&pClass->sName,&sName);` |
|        - |  5708 | `					/* Call the __get magic method if available */` |
|        3 |  5709 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5710 | `				}` |
|     3846 |  5711 | `				VmPopOperand(&pTos,1);` |
|        - |  5712 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5713 | `				 * This is due to the following case:` |
|        - |  5714 | `				 *     (new TestClass())->foo;` |
|        - |  5715 | `				 */` |
|     3846 |  5716 | `				pThis->iRef++;` |
|     3846 |  5717 | `				PH7_MemObjRelease(pTos);` |
|     3846 |  5718 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3846 |  5719 | `				if( pObjAttr ){` |
|     3844 |  5720 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5721 | `					/* Check attribute access */` |
|     3844 |  5722 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5723 | `						/* Load attribute */` |
|     3844 |  5724 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3844 |  5725 | `						if( pValue ){` |
|     3844 |  5726 | `							if( pThis->iRef < 2 ){` |
|        - |  5727 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5728 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5729 | `								 */` |
|        3 |  5730 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5731 | `							}else{` |
|        - |  5732 | `								/* Simple load */` |
|     3842 |  5733 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5734 | `							}` |
|     3844 |  5735 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3842 |  5736 | `								if( pThis->iRef > 1 ){` |
|        - |  5737 | `									/* Load attribute index */` |
|     3840 |  5738 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1919 |  5739 | `								}` |
|     1920 |  5740 | `							}` |
|     1921 |  5741 | `						}` |
|     1921 |  5742 | `					}` |
|     1921 |  5743 | `				}` |
|        - |  5744 | `				/* Safely unreference the object */` |
|     3846 |  5745 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5746 | `			}` |
|     2141 |  5747 | `		}else{` |
|      ! 0 |  5748 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5749 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5750 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5751 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5752 | `		}` |
|     2141 |  5753 | `	}else{` |
|        - |  5754 | `		/* Static member access using class name */` |
|      144 |  5755 | `		pNos = pTos;` |
|      144 |  5756 | `		pThis = 0;` |
|      144 |  5757 | `		if( !pInstr->p3 ){` |
|      132 |  5758 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      132 |  5759 | `			pNos--;` |
|        - |  5760 | `#ifdef UNTRUST` |
|        - |  5761 | `			if( pNos < pStack ){` |
|        - |  5762 | `				goto Abort;` |
|        - |  5763 | `			}` |
|        - |  5764 | `#endif` |
|       67 |  5765 | `		}else{` |
|        - |  5766 | `			/* Attribute name already computed */` |
|       14 |  5767 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5768 | `		}` |
|      144 |  5769 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      144 |  5770 | `			ph7_class *pClass = 0;` |
|      144 |  5771 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5772 | `				/* Class already instantiated */` |
|        5 |  5773 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5774 | `				pClass = pThis->pClass;` |
|        5 |  5775 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5776 | `			}else{` |
|        - |  5777 | `				/* Try to extract the target class */` |
|      140 |  5778 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      140 |  5779 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      140 |  5780 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5781 | `					/* Handle self/static/parent keywords */` |
|      140 |  5782 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       36 |  5783 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       36 |  5784 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5785 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5786 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5787 | `						}` |
|      123 |  5788 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       22 |  5789 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      103 |  5790 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       16 |  5791 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       16 |  5792 | `						if( pSelf && pSelf->pBase ){` |
|       16 |  5793 | `							pClass = pSelf->pBase;` |
|        7 |  5794 | `						}` |
|        9 |  5795 | `					}else{` |
|       72 |  5796 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5797 | `					}` |
|       69 |  5798 | `				}` |
|        - |  5799 | `			}` |
|      144 |  5800 | `			if( pClass == 0 ){` |
|        - |  5801 | `				/* Undefined class */` |
|      ! 0 |  5802 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5803 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5804 | `					);` |
|      ! 0 |  5805 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5806 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5807 | `				}` |
|      ! 0 |  5808 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5809 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5810 | `			}else{` |
|      144 |  5811 | `				if( pInstr->iP2 ){` |
|        - |  5812 | `					/* Method call */` |
|       68 |  5813 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5814 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5815 | `						/* Extract the target method */` |
|       68 |  5816 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5817 | `					}` |
|       68 |  5818 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5819 | `						if( pMeth ){` |
|      ! 0 |  5820 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5821 | `								&pClass->sName,&sName` |
|        - |  5822 | `								);` |
|      ! 0 |  5823 | `						}else{` |
|      ! 0 |  5824 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5825 | `								&pClass->sName,&sName` |
|        - |  5826 | `								);` |
|        - |  5827 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5828 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5829 | `						}` |
|        - |  5830 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5831 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5832 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5833 | `						}` |
|      ! 0 |  5834 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5835 | `					}else{` |
|        - |  5836 | `						/* Push method name on the stack */` |
|       68 |  5837 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5838 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5839 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5840 | `					}` |
|       68 |  5841 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5842 | `				}else{` |
|        - |  5843 | `					/* Attribute access */` |
|       78 |  5844 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5845 | `					/* Check for special ::class pseudo-constant */` |
|      113 |  5846 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5847 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5848 | `						/* ::class returns the fully qualified class name */` |
|        - |  5849 | `						/* Pop the attribute name from the stack */` |
|       60 |  5850 | `						if( !pInstr->p3 ){` |
|       60 |  5851 | `							VmPopOperand(&pTos,1);` |
|       29 |  5852 | `						}` |
|       60 |  5853 | `						PH7_MemObjRelease(pTos);` |
|        - |  5854 | `						/* Load the class name */` |
|       60 |  5855 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5856 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5857 | `					}else{` |
|        - |  5858 | `						/* Extract the target attribute */` |
|       20 |  5859 | `						if( sName.nByte > 0 ){` |
|       20 |  5860 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5861 | `						}` |
|       20 |  5862 | `						if( pAttr == 0 ){` |
|        - |  5863 | `							/* No such attribute,load null */` |
|      ! 0 |  5864 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5865 | `								&pClass->sName,&sName);` |
|        - |  5866 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5867 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5868 | `						}` |
|        - |  5869 | `						/* Pop the attribute name from the stack */` |
|       20 |  5870 | `						if( !pInstr->p3 ){` |
|        7 |  5871 | `							VmPopOperand(&pTos,1);` |
|        3 |  5872 | `						}` |
|       20 |  5873 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5874 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5875 | `						if( pAttr ){` |
|       20 |  5876 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5877 | `								/* Access to a non static attribute */` |
|      ! 0 |  5878 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5879 | `									&pClass->sName,&pAttr->sName` |
|        - |  5880 | `									);` |
|      ! 0 |  5881 | `							}else{` |
|        - |  5882 | `								ph7_value *pValue;` |
|        - |  5883 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5884 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5885 | `									/* Load the desired attribute */` |
|       20 |  5886 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5887 | `									if( pValue ){` |
|       20 |  5888 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5889 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5890 | `											/* Load index number */` |
|       14 |  5891 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5892 | `										}` |
|        9 |  5893 | `									}` |
|        9 |  5894 | `								}` |
|        - |  5895 | `							}` |
|        9 |  5896 | `						}` |
|        - |  5897 | `					}` |
|        - |  5898 | `				}` |
|      144 |  5899 | `				if( pThis ){` |
|        - |  5900 | `					/* Safely unreference the object */` |
|        5 |  5901 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  5902 | `				}` |
|        - |  5903 | `			}` |
|       73 |  5904 | `		}else{` |
|        - |  5905 | `			/* Pop operands */` |
|      ! 0 |  5906 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5907 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5908 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5909 | `			}` |
|      ! 0 |  5910 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5911 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5912 | `		}` |
|        - |  5913 | `	}` |
|     4422 |  5914 | `	break;` |
|        - |  5915 | `					}` |
|        - |  5916 | `/*` |
|        - |  5917 | ` * OP_NEW P1 * * *` |
|        - |  5918 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5919 | ` */` |
|      329 |  5920 | `case PH7_OP_NEW: {` |
|      660 |  5921 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      660 |  5922 | `	ph7_class *pClass = 0;` |
|        - |  5923 | `	ph7_class_instance *pNew;` |
|      660 |  5924 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5925 | `		/* Try to extract the desired class */` |
|      989 |  5926 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      658 |  5927 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      329 |  5928 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5929 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5930 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5931 | `	}` |
|      660 |  5932 | `	if( pClass == 0 ){` |
|        - |  5933 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5934 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5935 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5936 | `			);` |
|        - |  5937 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5938 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5939 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5940 | `			/* Pop given arguments */` |
|      ! 0 |  5941 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5942 | `		}` |
|      ! 0 |  5943 | `		goto Abort;` |
|      ! 0 |  5944 | `	}else{` |
|        - |  5945 | `		ph7_class_method *pCons;` |
|        - |  5946 | `		/* Create a new class instance */` |
|      660 |  5947 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      660 |  5948 | `		if( pNew == 0 ){` |
|      ! 0 |  5949 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5950 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5951 | `				&pClass->sName` |
|        - |  5952 | `			);` |
|      ! 0 |  5953 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5954 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5955 | `				/* Pop given arguments */` |
|      ! 0 |  5956 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5957 | `			}` |
|      ! 0 |  5958 | `			break;` |
|        - |  5959 | `		}` |
|        - |  5960 | `		/* Check if a constructor is available */` |
|      660 |  5961 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      660 |  5962 | `		if( pCons == 0 ){` |
|      546 |  5963 | `			SyString *pName = &pClass->sName;` |
|        - |  5964 | `			/* Check for a constructor with the same base class name */` |
|      546 |  5965 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      272 |  5966 | `		}` |
|      660 |  5967 | `		if( pCons ){` |
|        - |  5968 | `			/* Call the class constructor */` |
|      116 |  5969 | `			SySetReset(&aArg);` |
|      220 |  5970 | `			while( pArg < pTos ){` |
|      106 |  5971 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      106 |  5972 | `				pArg++;` |
|        2 |  5973 | `			}` |
|      116 |  5974 | `			if( pVm->bErrReport ){` |
|        - |  5975 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5976 | `				sxu32 n;` |
|       57 |  5977 | `				n = SySetUsed(&aArg);` |
|        - |  5978 | `				/* Emit a notice for missing arguments */` |
|      101 |  5979 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  5980 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  5981 | `					if( pFuncArg ){` |
|       45 |  5982 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5983 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5984 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5985 | `						}` |
|       22 |  5986 | `					}` |
|       45 |  5987 | `					n++;` |
|        1 |  5988 | `				}` |
|       28 |  5989 | `			}` |
|      116 |  5990 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5991 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      116 |  5992 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5993 | `				pNew->iRef = 1;` |
|      ! 0 |  5994 | `			}` |
|       57 |  5995 | `		}` |
|      660 |  5996 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5997 | `			/* Pop given arguments */` |
|       98 |  5998 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       48 |  5999 | `		}` |
|      660 |  6000 | `		PH7_MemObjRelease(pTos);` |
|      660 |  6001 | `		pTos->x.pOther = pNew;` |
|      660 |  6002 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6003 | `	}` |
|      660 |  6004 | `	break;` |
|        - |  6005 | `				 }` |
|        - |  6006 | `/*` |
|        - |  6007 | ` * OP_CLONE * * *` |
|        - |  6008 | ` * Perfome a clone operation.` |
|        - |  6009 | ` */` |
|       23 |  6010 | `case PH7_OP_CLONE: {` |
|        - |  6011 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6012 | `#ifdef UNTRUST` |
|        - |  6013 | `	if( pTos < pStack ){` |
|        - |  6014 | `		goto Abort;` |
|        - |  6015 | `	}` |
|        - |  6016 | `#endif` |
|        - |  6017 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6018 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6019 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6020 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6021 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6022 | `		break;` |
|        - |  6023 | `	}` |
|        - |  6024 | `	/* Point to the source */` |
|       44 |  6025 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6026 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6027 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6028 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6029 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6030 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6031 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6032 | `		break;` |
|        - |  6033 | `	}` |
|        - |  6034 | `	/* Perform the clone operation */` |
|       44 |  6035 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6036 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6037 | `	if( pClone == 0 ){` |
|      ! 0 |  6038 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6039 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6040 | `	}else{` |
|        - |  6041 | `		/* Load the cloned object */` |
|       44 |  6042 | `		pTos->x.pOther = pClone;` |
|       44 |  6043 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6044 | `	}` |
|       44 |  6045 | `	break;` |
|        - |  6046 | `				   }` |
|        - |  6047 | `/*` |
|        - |  6048 | ` * OP_SWITCH * * P3` |
|        - |  6049 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6050 | ` */` |
|       21 |  6051 | `case PH7_OP_SWITCH: {` |
|       44 |  6052 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6053 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6054 | `	ph7_value sValue,sCaseValue;` |
|        - |  6055 | `	sxu32 n,nEntry;` |
|        - |  6056 | `#ifdef UNTRUST` |
|        - |  6057 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6058 | `		goto Abort;` |
|        - |  6059 | `	}` |
|        - |  6060 | `#endif` |
|        - |  6061 | `	/* Point to the case table  */` |
|       44 |  6062 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       44 |  6063 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6064 | `	/* Select the appropriate case block to execute */` |
|       44 |  6065 | `	PH7_MemObjInit(pVm,&sValue);` |
|       44 |  6066 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      102 |  6067 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      102 |  6068 | `		pCase = &aCase[n];` |
|      102 |  6069 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6070 | `		/* Execute the case expression first */` |
|      102 |  6071 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6072 | `		/* Compare the two expression */` |
|      102 |  6073 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      102 |  6074 | `		PH7_MemObjRelease(&sValue);` |
|      102 |  6075 | `		PH7_MemObjRelease(&sCaseValue);` |
|      102 |  6076 | `		if( rc == 0 ){` |
|        - |  6077 | `			/* Value match,jump to this block */` |
|       44 |  6078 | `			pc = pCase->nStart - 1;` |
|       44 |  6079 | `			break;` |
|        - |  6080 | `		}` |
|       31 |  6081 | `	}` |
|       44 |  6082 | `	VmPopOperand(&pTos,1);` |
|       44 |  6083 | `	if( n >= nEntry ){` |
|        - |  6084 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  6085 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  6086 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  6087 | `		}else{` |
|        - |  6088 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6089 | `			pc = pSwitch->nOut - 1;` |
|        - |  6090 | `		}` |
|      ! 0 |  6091 | `	}` |
|       44 |  6092 | `	break;` |
|        - |  6093 | `					}` |
|        - |  6094 | `/*` |
|        - |  6095 | ` * OP_YIELD P1 P2 *` |
|        - |  6096 | ` *  Yield a value from a generator function.` |
|        - |  6097 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6098 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6099 | ` */` |
|       28 |  6100 | `case PH7_OP_YIELD: {` |
|        - |  6101 | `	ph7_generator *pGen;` |
|       58 |  6102 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6103 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6104 | `		goto Abort;` |
|        - |  6105 | `	}` |
|       58 |  6106 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6107 | `	if( pInstr->iP2 ){` |
|        - |  6108 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6109 | `#ifdef UNTRUST` |
|        - |  6110 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6111 | `#endif` |
|        7 |  6112 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6113 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6114 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6115 | `		VmPopOperand(&pTos, 1);` |
|        - |  6116 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6117 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6118 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6119 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6120 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6121 | `			}` |
|        1 |  6122 | `		}` |
|       55 |  6123 | `	}else if( pInstr->iP1 ){` |
|        - |  6124 | `		/* yield $value */` |
|        - |  6125 | `#ifdef UNTRUST` |
|        - |  6126 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6127 | `#endif` |
|       52 |  6128 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6129 | `		VmPopOperand(&pTos, 1);` |
|        - |  6130 | `		/* Auto-increment key */` |
|       52 |  6131 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6132 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6133 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6134 | `	}else{` |
|        - |  6135 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6136 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6137 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6138 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6139 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6140 | `	}` |
|        - |  6141 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6142 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6143 | `	goto Suspend;` |
|        - |  6144 |  |
|        - |  6145 | `/*` |
|        - |  6146 | ` * OP_CALL P1 * *` |
|        - |  6147 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6148 | ` *  function on the stack.` |
|        - |  6149 | ` */` |
|   296661 |  6150 | `case PH7_OP_CALL: {` |
|   593368 |  6151 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6152 | `	ph7_value *pArg;` |
|   593368 |  6153 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   593368 |  6154 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6155 | `	SyHashEntry *pEntry;` |
|        - |  6156 | `	SyString sName;` |
|        - |  6157 | `	/* Extract function name */` |
|   593368 |  6158 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6159 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6160 | `			ph7_value sResult;` |
|      ! 0 |  6161 | `			SySetReset(&aArg);` |
|      ! 0 |  6162 | `			while( pArg < pTos ){` |
|      ! 0 |  6163 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6164 | `				pArg++;` |
|      ! 0 |  6165 | `			}` |
|      ! 0 |  6166 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6167 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6168 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6169 | `			SySetReset(&aArg);` |
|        - |  6170 | `			/* Pop given arguments */` |
|      ! 0 |  6171 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6172 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6173 | `			}` |
|        - |  6174 | `			/* Copy result */` |
|      ! 0 |  6175 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6176 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6177 | `		}else{` |
|        3 |  6178 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6179 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6180 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6181 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6182 | `			}else{` |
|        - |  6183 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6184 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6185 | `			}` |
|        - |  6186 | `			/* Pop given arguments */` |
|        3 |  6187 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6188 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6189 | `			}` |
|        - |  6190 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6191 | `			PH7_MemObjRelease(pTos);` |
|        - |  6192 | `		}` |
|   296388 |  6193 | `		break;` |
|        - |  6194 | `	}` |
|   593366 |  6195 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6196 | `	/* Check for a compiled function first.` |
|        - |  6197 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6198 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   593366 |  6199 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6200 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6201 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6202 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6203 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6204 | `	 * function calls inside namespaces. */` |
|   593366 |  6205 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6206 | `		const char *zFunc;` |
|        - |  6207 | `		const char *zEnd;` |
|        - |  6208 | `		const char *z;` |
|        - |  6209 | `		SyString sGlobal;` |
|       18 |  6210 | `		zFunc = sName.zString;` |
|       18 |  6211 | `		zEnd  = zFunc + sName.nByte;` |
|       18 |  6212 | `		z = zEnd;` |
|        - |  6213 | `		/* Find last namespace separator */` |
|      154 |  6214 | `		while( z > zFunc ){` |
|      154 |  6215 | `			if( z[-1] == '\\' ){` |
|       18 |  6216 | `				break;` |
|        - |  6217 | `			}` |
|      138 |  6218 | `			z--;` |
|        2 |  6219 | `		}` |
|       18 |  6220 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6221 | `			/* Retry lookup using the unqualified/global function name */` |
|       18 |  6222 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       18 |  6223 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        8 |  6224 | `		}` |
|        8 |  6225 | `	}` |
|   593366 |  6226 | `	if( pEntry ){` |
|        - |  6227 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6228 | `		ph7_class_instance *pThis;` |
|        - |  6229 | `		ph7_value *pFrameStack;` |
|        - |  6230 | `		ph7_vm_func *pVmFunc;` |
|        - |  6231 | `		ph7_class *pSelf;` |
|        - |  6232 | `		VmFrame *pFrame;` |
|        - |  6233 | `		ph7_value *pObj;` |
|        - |  6234 | `		VmSlot sArg;` |
|        - |  6235 | `		sxu32 n;` |
|        - |  6236 | `		/* initialize fields */` |
|    13328 |  6237 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13328 |  6238 | `		pThis = 0;` |
|    13328 |  6239 | `		pSelf = 0;` |
|    13328 |  6240 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6241 | `			ph7_class_method *pMeth;` |
|        - |  6242 | `			/* Class method call */` |
|     1994 |  6243 | `			ph7_value *pTarget = &pTos[-1];` |
|     1994 |  6244 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6245 | `				/* Extract the 'this' pointer */` |
|     1994 |  6246 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6247 | `					/* Instance already loaded */` |
|     1922 |  6248 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1922 |  6249 | `					pThis->iRef++;` |
|     1922 |  6250 | `					pSelf = pThis->pClass;` |
|      960 |  6251 | `				}` |
|     1994 |  6252 | `				if( pSelf == 0 ){` |
|       74 |  6253 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6254 | `						/* "Late Static Binding" class name */` |
|      101 |  6255 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6256 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6257 | `					}` |
|       74 |  6258 | `					if( pSelf == 0 ){` |
|       13 |  6259 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6260 | `					}` |
|       36 |  6261 | `				}` |
|     1994 |  6262 | `				if( pThis == 0  ){` |
|       74 |  6263 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6264 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6265 | `					if( pFrameLocal->pParent ){` |
|        - |  6266 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6267 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6268 | `						if( pThis ){` |
|       13 |  6269 | `							pThis->iRef++;` |
|        6 |  6270 | `						}` |
|       28 |  6271 | `					}` |
|       36 |  6272 | `				}` |
|     1994 |  6273 | `				VmPopOperand(&pTos,1);` |
|     1994 |  6274 | `				PH7_MemObjRelease(pTos);` |
|        - |  6275 | `				/* Synchronize pointers */` |
|     1994 |  6276 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6277 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6278 | `				 * user have already computed the random generated unique class method name` |
|        - |  6279 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6280 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6281 | `				 */` |
|     1994 |  6282 | `				while( pArg < pStack ){` |
|      ! 0 |  6283 | `					pArg++;` |
|      ! 0 |  6284 | `				}` |
|     1994 |  6285 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6286 | `					/* Check if the call is allowed */` |
|     1994 |  6287 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1994 |  6288 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6289 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6290 | `							/* Pop given arguments */` |
|      ! 0 |  6291 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6292 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6293 | `							}` |
|        - |  6294 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6295 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6296 | `							break;` |
|        - |  6297 | `						}` |
|        3 |  6298 | `					}` |
|      996 |  6299 | `				}` |
|      996 |  6300 | `			}` |
|      996 |  6301 | `		}` |
|        - |  6302 | `		/* Check The recursion limit */` |
|    13328 |  6303 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6304 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6305 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6306 | `				&pVmFunc->sName);` |
|        - |  6307 | `			/* Pop given arguments */` |
|        3 |  6308 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6309 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6310 | `			}` |
|        - |  6311 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6312 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6313 | `			break;` |
|        - |  6314 | `		}` |
|    13326 |  6315 | `		if( pVmFunc->pNextName ){` |
|        - |  6316 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6317 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6318 | `		}` |
|    13326 |  6319 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6320 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6321 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6322 | `			ph7_generator *pGenerator;` |
|        - |  6323 | `			ph7_class_instance *pGenObj;` |
|        - |  6324 | `			ph7_value *pCtxAttr;` |
|        - |  6325 | `			SyString sAttrName;` |
|        - |  6326 | `			ph7_value **apCallArgs;` |
|        - |  6327 | `			int nGenArgs, iArg;` |
|        - |  6328 | `			/* Collect arguments from the operand stack */` |
|       20 |  6329 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6330 | `			apCallArgs = 0;` |
|       20 |  6331 | `			if( nGenArgs > 0 ){` |
|        8 |  6332 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6333 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6334 | `				if( apCallArgs == 0 ){` |
|        - |  6335 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6336 | `					nGenArgs = 0;` |
|      ! 0 |  6337 | `				}else{` |
|       12 |  6338 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6339 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6340 | `					}` |
|        - |  6341 | `				}` |
|        2 |  6342 | `			}` |
|        - |  6343 | `			/* Create execution context and generator wrapper */` |
|       20 |  6344 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6345 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6346 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6347 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6348 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6349 | `				break;` |
|        - |  6350 | `			}` |
|       20 |  6351 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6352 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6353 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6354 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6355 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6356 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6357 | `				break;` |
|        - |  6358 | `			}` |
|        - |  6359 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6360 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6361 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6362 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6363 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6364 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6365 | `			if( apCallArgs ){` |
|        6 |  6366 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6367 | `			}` |
|       20 |  6368 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6369 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6370 | `				if( pThis ){` |
|      ! 0 |  6371 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6372 | `				}` |
|      ! 0 |  6373 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6374 | `					goto Abort;` |
|        - |  6375 | `				}` |
|      ! 0 |  6376 | `				break;` |
|        - |  6377 | `			}` |
|        - |  6378 | `			/* Create Generator class instance */` |
|       20 |  6379 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6380 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6381 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6382 | `				break;` |
|        - |  6383 | `			}` |
|        - |  6384 | `			/* Store generator in __ctx attribute */` |
|       20 |  6385 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6386 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6387 | `			if( pCtxAttr ){` |
|       20 |  6388 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6389 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6390 | `			}` |
|        - |  6391 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6392 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6393 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6394 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6395 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6396 | `			pGenObj->iRef++;` |
|       20 |  6397 | `			if( pThis ){` |
|      ! 0 |  6398 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6399 | `			}` |
|       20 |  6400 | `			break;` |
|        - |  6401 | `		}` |
|        - |  6402 | `		/* Extract the formal argument set */` |
|    13308 |  6403 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6404 | `		/* Create a new VM frame  */` |
|    13308 |  6405 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13308 |  6406 | `		if( rc != SXRET_OK ){` |
|        - |  6407 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6408 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6409 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6410 | `				&pVmFunc->sName);` |
|        - |  6411 | `			/* Pop given arguments */` |
|      ! 0 |  6412 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6413 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6414 | `			}` |
|        - |  6415 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6416 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6417 | `			break;` |
|        - |  6418 | `		}` |
|    13308 |  6419 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6420 | `			/* Install the '$this' variable */` |
|        - |  6421 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1932 |  6422 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1932 |  6423 | `			if( pObj ){` |
|        - |  6424 | `				/* Reflect the change */` |
|     1932 |  6425 | `				pObj->x.pOther = pThis;` |
|     1932 |  6426 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      965 |  6427 | `			}` |
|      965 |  6428 | `		}` |
|    13308 |  6429 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6430 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6431 | `			/* Install static variables */` |
|      ! 0 |  6432 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6433 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6434 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6435 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6436 | `					/* Initialize the static variables */` |
|      ! 0 |  6437 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6438 | `					if( pObj ){` |
|        - |  6439 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6440 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6441 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6442 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6443 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6444 | `						}` |
|      ! 0 |  6445 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6446 | `					}else{` |
|      ! 0 |  6447 | `						continue;` |
|        - |  6448 | `					}` |
|      ! 0 |  6449 | `				}` |
|        - |  6450 | `				/* Install in the current frame */` |
|      ! 0 |  6451 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6452 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6453 | `			}` |
|      ! 0 |  6454 | `		}` |
|        - |  6455 | `		/* Push arguments in the local frame */` |
|    13308 |  6456 | `		n = 0;` |
|    36054 |  6457 | `		while( pArg < pTos ){` |
|    22768 |  6458 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6459 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6460 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6461 | `				if( pObj ){` |
|        - |  6462 | `					/* Initialize as empty array */` |
|       21 |  6463 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6464 | `					{` |
|       21 |  6465 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6466 | `						while( pArg < pTos ){` |
|        - |  6467 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6468 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6469 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6470 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6471 | `								if( xCast ){` |
|       13 |  6472 | `									xCast(pArg);` |
|        6 |  6473 | `								}` |
|        6 |  6474 | `							}` |
|       63 |  6475 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6476 | `							pArg++;` |
|        1 |  6477 | `						}` |
|        - |  6478 | `					}` |
|       21 |  6479 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6480 | `					sArg.pUserData = 0;` |
|       21 |  6481 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6482 | `				}` |
|       21 |  6483 | `				break; /* All remaining args consumed */` |
|        - |  6484 | `			}` |
|    22748 |  6485 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22592 |  6486 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6487 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6488 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6489 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6490 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6491 | `						goto Abort;` |
|        - |  6492 | `					}` |
|      ! 0 |  6493 | `				}` |
|        - |  6494 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6495 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22604 |  6496 | `				if( aFormalArg[n].nType > 0` |
|    11873 |  6497 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1140 |  6498 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6499 | `						/* Argument must be a class instance [i.e: object] */` |
|        5 |  6500 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6501 | `						ph7_class *pClass;` |
|        - |  6502 | `						/* Try to extract the desired class */` |
|        5 |  6503 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        5 |  6504 | `						if( pClass ){` |
|        5 |  6505 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6506 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6507 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6508 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6509 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6510 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6511 | `								}` |
|      ! 0 |  6512 | `							}else{` |
|        - |  6513 | `								/* reuse pThis declared in outer scope */` |
|        5 |  6514 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6515 | `								/* Make sure the object is an instance of the given class */` |
|        5 |  6516 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6517 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6518 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6519 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6520 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6521 | `								}` |
|        - |  6522 | `							}` |
|        3 |  6523 | `						}` |
|     1138 |  6524 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6525 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6526 | `						/* Cast to the desired type */` |
|      ! 0 |  6527 | `						xCast(pArg);` |
|      ! 0 |  6528 | `					}` |
|      569 |  6529 | `				}` |
|    22594 |  6530 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6531 | `					/* Pass by reference */` |
|       54 |  6532 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6533 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6534 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6535 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6536 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6537 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6538 | `						}` |
|        - |  6539 | `						/* Switch to pass by value */` |
|      ! 0 |  6540 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6541 | `					}else{` |
|        - |  6542 | `						SyHashEntry *pRefEntry;` |
|        - |  6543 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6544 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6545 | `						if( pRefEntry == 0 ){` |
|       80 |  6546 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6547 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6548 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6549 | `							sArg.pUserData = 0;` |
|       54 |  6550 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6551 | `						}` |
|       54 |  6552 | `						pObj = 0;` |
|        - |  6553 | `					}` |
|       28 |  6554 | `				}else{` |
|        - |  6555 | `					/* Pass by value,make a copy of the given argument */` |
|    22542 |  6556 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6557 | `				}` |
|    11298 |  6558 | `			}else{` |
|        - |  6559 | `				char zName[32];` |
|        - |  6560 | `				SyString sArgName;` |
|        - |  6561 | `				/* Set a dummy name */` |
|      156 |  6562 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6563 | `				sArgName.zString = zName;` |
|        - |  6564 | `				/* Annonymous argument */` |
|      156 |  6565 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6566 | `			}` |
|    22748 |  6567 | `			if( pObj ){` |
|    22696 |  6568 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6569 | `				/* Insert argument index  */` |
|    22696 |  6570 | `				sArg.nIdx = pObj->nIdx;` |
|    22696 |  6571 | `				sArg.pUserData = 0;` |
|    22696 |  6572 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11347 |  6573 | `			}` |
|    22748 |  6574 | `			PH7_MemObjRelease(pArg);` |
|    22748 |  6575 | `			pArg++;` |
|    22748 |  6576 | `			++n;` |
|        2 |  6577 | `		}` |
|        - |  6578 | `		/* Set up closure environment */` |
|    13308 |  6579 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6580 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6581 | `			ph7_value *pValue;` |
|        - |  6582 | `			sxu32 iEnv;` |
|       11 |  6583 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6584 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6585 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6586 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6587 | `					/* Do not install null value */` |
|       11 |  6588 | `					continue;` |
|        - |  6589 | `				}` |
|       11 |  6590 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6591 | `				if( pValue == 0 ){` |
|      ! 0 |  6592 | `					continue;` |
|        - |  6593 | `				}` |
|        - |  6594 | `				/* Invalidate any prior representation */` |
|       11 |  6595 | `				PH7_MemObjRelease(pValue);` |
|        - |  6596 | `				/* Duplicate bound variable value */` |
|       11 |  6597 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6598 | `			}` |
|        5 |  6599 | `		}` |
|        - |  6600 | `		/* Process default values for remaining formal parameters */` |
|    15258 |  6601 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1978 |  6602 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6603 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6604 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6605 | `				if( pObj ){` |
|       27 |  6606 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6607 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6608 | `					sArg.pUserData = 0;` |
|       27 |  6609 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6610 | `				}` |
|       27 |  6611 | `				n++;` |
|       27 |  6612 | `				break; /* Variadic is always last */` |
|        - |  6613 | `			}` |
|     1952 |  6614 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1946 |  6615 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1946 |  6616 | `				if( pObj ){` |
|        - |  6617 | `					/* Evaluate the default value and extract it's result */` |
|     1946 |  6618 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1946 |  6619 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6620 | `						goto Abort;` |
|        - |  6621 | `					}` |
|        - |  6622 | `					/* Insert argument index */` |
|     1946 |  6623 | `					sArg.nIdx = pObj->nIdx;` |
|     1946 |  6624 | `					sArg.pUserData = 0;` |
|     1946 |  6625 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6626 | `					/* Make sure the default argument is of the correct type */` |
|     1946 |  6627 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6628 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6629 | `						/* Cast to the desired type */` |
|      ! 0 |  6630 | `						xCast(pObj);` |
|      ! 0 |  6631 | `					}` |
|      972 |  6632 | `				}` |
|      972 |  6633 | `			}` |
|     1952 |  6634 | `			++n;` |
|        2 |  6635 | `		}` |
|        - |  6636 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6637 | `		 * does not return anything.` |
|        - |  6638 | `		 */` |
|    13308 |  6639 | `		PH7_MemObjRelease(pTos);` |
|    13308 |  6640 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6641 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13308 |  6642 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13308 |  6643 | `		if( pFrameStack == 0 ){` |
|        - |  6644 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6645 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6646 | `				&pVmFunc->sName);` |
|      ! 0 |  6647 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6648 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6649 | `			}` |
|      ! 0 |  6650 | `			break;` |
|        - |  6651 | `		}` |
|    13308 |  6652 | `		if( pSelf ){` |
|        - |  6653 | `			/* Push class name */` |
|     1992 |  6654 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      995 |  6655 | `		}` |
|        - |  6656 | `		/* Increment nesting level */` |
|    13308 |  6657 | `		pVm->nRecursionDepth++;` |
|        - |  6658 | `		/* Execute function body */` |
|    13308 |  6659 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6660 | `		/* Decrement nesting level */` |
|    13308 |  6661 | `		pVm->nRecursionDepth--;` |
|    13308 |  6662 | `		if( pSelf ){` |
|        - |  6663 | `			/* Pop class name */` |
|     1992 |  6664 | `			(void)SySetPop(&pVm->aSelf);` |
|      995 |  6665 | `		}` |
|        - |  6666 | `		/* Cleanup the mess left behind */` |
|    13308 |  6667 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6668 | `			/* Return by reference,reflect that */` |
|        9 |  6669 | `			if( n != SXU32_HIGH ){` |
|        9 |  6670 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6671 | `				sxu32 i;` |
|        - |  6672 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6673 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6674 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6675 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6676 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6677 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6678 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6679 | `								&pVmFunc->sName);` |
|      ! 0 |  6680 | `						}` |
|      ! 0 |  6681 | `						n = SXU32_HIGH;` |
|      ! 0 |  6682 | `						break;` |
|        - |  6683 | `					}` |
|        3 |  6684 | `				}` |
|        5 |  6685 | `			}else{` |
|      ! 0 |  6686 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6687 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6688 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6689 | `						&pVmFunc->sName);` |
|      ! 0 |  6690 | `				}` |
|        - |  6691 | `			}` |
|        9 |  6692 | `			pTos->nIdx = n;` |
|        4 |  6693 | `		}` |
|        - |  6694 | `		/* Cleanup the mess left behind */` |
|    13308 |  6695 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6696 | `			/* An exception was throw in this frame */` |
|       12 |  6697 | `			pFrame = pFrame->pParent;` |
|       12 |  6698 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6699 | `				/* Pop the resutlt */` |
|       10 |  6700 | `				VmPopOperand(&pTos,1);` |
|        - |  6701 | `				/* Jump to this destination */` |
|       10 |  6702 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6703 | `				rc = PH7_OK;` |
|        6 |  6704 | `			}else{` |
|        3 |  6705 | `				if( pFrame->pParent ){` |
|        3 |  6706 | `					rc = PH7_EXCEPTION;` |
|        2 |  6707 | `				}else{` |
|        - |  6708 | `					/* Continue normal execution */` |
|      ! 0 |  6709 | `					rc = PH7_OK;` |
|        - |  6710 | `				}` |
|        - |  6711 | `			}` |
|        5 |  6712 | `		}` |
|        - |  6713 | `		/* Free the operand stack */` |
|    13308 |  6714 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6715 | `		/* Leave the frame */` |
|    13308 |  6716 | `		VmLeaveFrame(&(*pVm));` |
|    13308 |  6717 | `		if( rc == PH7_ABORT ){` |
|        - |  6718 | `			/* Abort processing immeditaley */` |
|        7 |  6719 | `			goto Abort;` |
|    13302 |  6720 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6721 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6722 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6723 | `			 * overwriting the state saved by the inner level.` |
|        - |  6724 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6725 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6726 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6727 | `			goto Suspend;` |
|    13264 |  6728 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6729 | `			goto Exception;` |
|        - |  6730 | `		}` |
|     6632 |  6731 | `	}else{` |
|        - |  6732 | `		ph7_user_func *pFunc;` |
|        - |  6733 | `		ph7_context sCtx;` |
|        - |  6734 | `		ph7_value sRet;` |
|        - |  6735 | `		/* Look for an installed foreign function.` |
|        - |  6736 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6737 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6738 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6739 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6740 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6741 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   580040 |  6742 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   580040 |  6743 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6744 | `			/* Compiler-qualified: try short name as global fallback */` |
|       18 |  6745 | `			const char *zShort = sName.zString;` |
|        - |  6746 | `			sxu32 i;` |
|      262 |  6747 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      246 |  6748 | `				if( sName.zString[i] == '\\' ){` |
|       22 |  6749 | `					zShort = &sName.zString[i + 1];` |
|       10 |  6750 | `				}` |
|      124 |  6751 | `			}` |
|       18 |  6752 | `			if( zShort != sName.zString ){` |
|       18 |  6753 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       18 |  6754 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        8 |  6755 | `			}` |
|        8 |  6756 | `		}` |
|   580040 |  6757 | `		if( pEntry == 0 ){` |
|        - |  6758 | `			/* Call to undefined function */` |
|        5 |  6759 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6760 | `			/* Pop given arguments */` |
|        5 |  6761 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6762 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6763 | `			}` |
|        - |  6764 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6765 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6766 | `			break;` |
|        - |  6767 | `		}` |
|   580036 |  6768 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6769 | `		/* Start collecting function arguments */` |
|   580036 |  6770 | `		SySetReset(&aArg);` |
|  1557728 |  6771 | `		while( pArg < pTos ){` |
|   977694 |  6772 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   977694 |  6773 | `			pArg++;` |
|        2 |  6774 | `		}` |
|        - |  6775 | `		/* Assume a null return value */` |
|   580036 |  6776 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6777 | `		/* Init the call context */` |
|   580036 |  6778 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6779 | `		/* Call the foreign function */` |
|   580036 |  6780 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6781 | `		/* Release the call context */` |
|   580036 |  6782 | `		VmReleaseCallContext(&sCtx);` |
|   580036 |  6783 | `		if( rc == PH7_ABORT ){` |
|      463 |  6784 | `			goto Abort;` |
|   579574 |  6785 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6786 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6787 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6788 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6789 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6790 | `				goto Exception;` |
|        - |  6791 | `			}` |
|        - |  6792 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6793 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6794 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6795 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6796 | `			}` |
|        - |  6797 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6798 | `			VmPopOperand(&pTos,1);` |
|        - |  6799 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6800 | `			pFrm = pVm->pFrame;` |
|        7 |  6801 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6802 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6803 | `			}` |
|        7 |  6804 | `			break;` |
|        - |  6805 | `		}` |
|   579564 |  6806 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6807 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6808 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6809 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6810 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6811 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6812 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6813 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6814 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6815 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6816 | `			}` |
|        - |  6817 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6818 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6819 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6820 | `			goto Suspend;` |
|        - |  6821 | `		}` |
|   579526 |  6822 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6823 | `			/* Pop function name and arguments */` |
|   561014 |  6824 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   280528 |  6825 | `		}` |
|        - |  6826 | `		/* Save foreign function return value */` |
|   579526 |  6827 | `		PH7_MemObjStore(&sRet,pTos);` |
|   579526 |  6828 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6829 | `	}` |
|   592786 |  6830 | `	break;` |
|        - |  6831 | `				  }` |
|        - |  6832 | `/*` |
|        - |  6833 | ` * OP_CONSUME: P1 * *` |
|        - |  6834 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6835 | ` */` |
|    11782 |  6836 | `case PH7_OP_CONSUME: {` |
|    23566 |  6837 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    23566 |  6838 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6839 |  |
|    23566 |  6840 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    23566 |  6841 | `	pCur = pOut;` |
|        - |  6842 | `	/* Start the consume process  */` |
|    47130 |  6843 | `	while( pOut <= pTos ){` |
|        - |  6844 | `		/* Force a string cast */` |
|    23566 |  6845 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6846 | `			PH7_MemObjToString(pOut);` |
|      149 |  6847 | `		}` |
|    23566 |  6848 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6849 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6850 | `			/* Invoke the output consumer callback */` |
|    13188 |  6851 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13188 |  6852 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13188 |  6853 | `			SyBlobRelease(&pOut->sBlob);` |
|    13188 |  6854 | `			if( rc == SXERR_ABORT ){` |
|        - |  6855 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6856 | `				goto Abort;` |
|        - |  6857 | `			}` |
|     6593 |  6858 | `		}` |
|    23566 |  6859 | `		pOut++;` |
|        2 |  6860 | `	}` |
|    23566 |  6861 | `	pTos = &pCur[-1];` |
|    23564 |  6862 | `	break;` |
|        - |  6863 | `					 }` |
|        - |  6864 |  |
|        - |  6865 | `		} /* Switch() */` |
| 10001614 |  6866 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6867 | `	} /* For(;;) */` |
|    16177 |  6868 | `Done:` |
|    32356 |  6869 | `	SySetRelease(&aArg);` |
|    32356 |  6870 | `	return SXRET_OK;` |
|       66 |  6871 | `Suspend:` |
|      134 |  6872 | `	SySetRelease(&aArg);` |
|      134 |  6873 | `	return PH7_SUSPEND;` |
|      238 |  6874 | `Abort:` |
|      477 |  6875 | `	SySetRelease(&aArg);` |
|     1661 |  6876 | `	while( pTos >= pStack ){` |
|     1185 |  6877 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6878 | `		pTos--;` |
|        1 |  6879 | `	}` |
|      477 |  6880 | `	return PH7_ABORT;` |
|        3 |  6881 | `Exception:` |
|        8 |  6882 | `	SySetRelease(&aArg);` |
|       22 |  6883 | `	while( pTos >= pStack ){` |
|       16 |  6884 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6885 | `		pTos--;` |
|        2 |  6886 | `	}` |
|        8 |  6887 | `	return PH7_EXCEPTION;` |
|    16486 |  6888 |  |
|        - |  6889 | `/*` |
|        - |  6890 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6891 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6892 | ` * See block-comment on that function for additional information.` |
|        - |  6893 | ` */` |
|    15262 |  6894 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6895 |  |
|        - |  6896 | `	ph7_value *pStack;` |
|        - |  6897 | `	sxi32 rc;` |
|        - |  6898 | `	/* Allocate a new operand stack */` |
|    15264 |  6899 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15264 |  6900 | `	if( pStack == 0 ){` |
|      ! 0 |  6901 | `		return SXERR_MEM;` |
|        - |  6902 | `	}` |
|        - |  6903 | `	/* Execute the program */` |
|    15264 |  6904 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6905 | `	/* Free the operand stack */` |
|    15264 |  6906 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6907 | `	/* Execution result */` |
|    15264 |  6908 | `	return rc;` |
|     7633 |  6909 |  |
|        - |  6910 | `/*` |
|        - |  6911 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6912 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6913 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6914 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6915 | ` * execution ends.` |
|        - |  6916 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6917 | ` * additional information.` |
|        - |  6918 | ` */` |
|     2308 |  6919 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6920 |  |
|        - |  6921 | `	VmShutdownCB *pEntry;` |
|        - |  6922 | `	ph7_value *apArg[10];` |
|        - |  6923 | `	sxu32 n,nEntry;` |
|        - |  6924 | `	int i;` |
|        - |  6925 | `	/* Point to the stack of registered callbacks */` |
|     2310 |  6926 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25390 |  6927 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23082 |  6928 | `		apArg[i] = 0;` |
|    11542 |  6929 | `	}` |
|     2312 |  6930 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6931 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6932 | `		if( pEntry ){` |
|        - |  6933 | `			/* Prepare callback arguments if any */` |
|        3 |  6934 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6935 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6936 | `					break;` |
|        - |  6937 | `				}` |
|      ! 0 |  6938 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6939 | `			}` |
|        - |  6940 | `			/* Invoke the callback */` |
|        3 |  6941 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6942 | `			/*` |
|        - |  6943 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6944 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6945 | `			 */` |
|        3 |  6946 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6947 | `			if( pEntry ){` |
|        3 |  6948 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6949 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6950 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6951 | `				}` |
|        1 |  6952 | `			}` |
|        1 |  6953 | `		}` |
|        2 |  6954 | `	}` |
|     2310 |  6955 | `	SySetReset(&pVm->aShutdown);` |
|     2310 |  6956 |  |
|        - |  6957 | `/*` |
|        - |  6958 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6959 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6960 | ` * See block-comment on that function for additional information.` |
|        - |  6961 | ` */` |
|     2316 |  6962 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6963 |  |
|        - |  6964 | `	/* Make sure we are ready to execute this program */` |
|     2318 |  6965 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6966 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6967 | `	}` |
|        - |  6968 | `	/* Set the execution magic number  */` |
|     2318 |  6969 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6970 | `	/* Execute the program */` |
|     2318 |  6971 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6972 | `	/* Invoke any shutdown callbacks */` |
|     2314 |  6973 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6974 | `	/*` |
|        - |  6975 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6976 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6977 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6978 | `	 */` |
|     2314 |  6979 | `	return SXRET_OK;` |
|     1160 |  6980 |  |
|        - |  6981 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6982 | `/*` |
|        - |  6983 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6984 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6985 | ` */` |
|       42 |  6986 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  6987 |  |
|        - |  6988 | `	ph7_exec_ctx *pCtx;` |
|        - |  6989 | `	ph7_value *pStack;` |
|        - |  6990 | `	VmFrame *pFrame;` |
|       44 |  6991 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  6992 | `	if( pCtx == 0 ){` |
|      ! 0 |  6993 | `		return 0;` |
|        - |  6994 | `	}` |
|       44 |  6995 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  6996 | `	pCtx->pVm = pVm;` |
|       44 |  6997 | `	pCtx->pFunc = pFunc;` |
|       44 |  6998 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  6999 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7000 | `	pCtx->pc = 0;` |
|       44 |  7001 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7002 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7003 | `	/* Allocate a private operand stack */` |
|       44 |  7004 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7005 | `	if( pStack == 0 ){` |
|      ! 0 |  7006 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7007 | `		return 0;` |
|        - |  7008 | `	}` |
|       44 |  7009 | `	pCtx->pStack = pStack;` |
|        - |  7010 | `	/* Create a detached frame for the fiber */` |
|       44 |  7011 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7012 | `	if( pFrame == 0 ){` |
|      ! 0 |  7013 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7014 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7015 | `		return 0;` |
|        - |  7016 | `	}` |
|       44 |  7017 | `	pCtx->pFrame = pFrame;` |
|       44 |  7018 | `	return pCtx;` |
|       23 |  7019 |  |
|        - |  7020 | `/*` |
|        - |  7021 | ` * Start executing a fiber context for the first time.` |
|        - |  7022 | ` */` |
|       42 |  7023 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7024 |  |
|        - |  7025 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7026 | `	sxi32 rc;` |
|       44 |  7027 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7028 | `		return SXERR_INVALID;` |
|        - |  7029 | `	}` |
|        - |  7030 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7031 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7032 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7033 | `	/* Save and set the active context */` |
|       44 |  7034 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7035 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7036 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7037 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7038 | `	pVm->nRecursionDepth++;` |
|        - |  7039 | `	/* Execute from the beginning */` |
|       65 |  7040 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7041 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7042 | `	pVm->nRecursionDepth--;` |
|        - |  7043 | `	/* Restore the previous context */` |
|       44 |  7044 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7045 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7046 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7047 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7048 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7049 | `		if( pResult ){` |
|       24 |  7050 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7051 | `		}` |
|       42 |  7052 | `		return SXRET_OK;` |
|        - |  7053 | `	}` |
|        - |  7054 | `	/* Detach frame */` |
|        3 |  7055 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7056 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7057 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7058 | `	}` |
|        3 |  7059 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7060 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7061 | `		return PH7_ABORT;` |
|        - |  7062 | `	}` |
|        3 |  7063 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7064 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7065 | `		return PH7_EXCEPTION;` |
|        - |  7066 | `	}` |
|        - |  7067 | `	/* Normal completion */` |
|        3 |  7068 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7069 | `	if( pResult ){` |
|        3 |  7070 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7071 | `	}` |
|        3 |  7072 | `	return SXRET_OK;` |
|       23 |  7073 |  |
|        - |  7074 | `/*` |
|        - |  7075 | ` * Resume a suspended fiber context.` |
|        - |  7076 | ` */` |
|       86 |  7077 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7078 |  |
|        - |  7079 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7080 | `	sxi32 rc;` |
|       88 |  7081 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7082 | `		return SXERR_INVALID;` |
|        - |  7083 | `	}` |
|        - |  7084 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7085 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7086 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7087 | `	if( pResumeValue ){` |
|       40 |  7088 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7089 | `	}else{` |
|       50 |  7090 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7091 | `	}` |
|       88 |  7092 | `	pCtx->nTos++;` |
|        - |  7093 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7094 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7095 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7096 | `	/* Save and set the active context */` |
|       88 |  7097 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7098 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7099 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7100 | `	pVm->nRecursionDepth++;` |
|        - |  7101 | `	/* Resume execution from saved PC */` |
|      131 |  7102 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7103 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7104 | `	pVm->nRecursionDepth--;` |
|        - |  7105 | `	/* Restore the previous context */` |
|       88 |  7106 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7107 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7108 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7109 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7110 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7111 | `		if( pResult ){` |
|       18 |  7112 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7113 | `		}` |
|       56 |  7114 | `		return SXRET_OK;` |
|        - |  7115 | `	}` |
|        - |  7116 | `	/* Detach frame */` |
|       34 |  7117 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7118 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7119 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7120 | `	}` |
|       34 |  7121 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7122 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7123 | `		return PH7_ABORT;` |
|        - |  7124 | `	}` |
|       34 |  7125 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7126 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7127 | `		return PH7_EXCEPTION;` |
|        - |  7128 | `	}` |
|        - |  7129 | `	/* Normal completion */` |
|       34 |  7130 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7131 | `	if( pResult ){` |
|       20 |  7132 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7133 | `	}` |
|       34 |  7134 | `	return SXRET_OK;` |
|       45 |  7135 |  |
|        - |  7136 | `/*` |
|        - |  7137 | ` * Release an execution context and all its resources.` |
|        - |  7138 | ` */` |
|        4 |  7139 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7140 |  |
|        5 |  7141 | `	if( pCtx == 0 ){` |
|      ! 0 |  7142 | `		return;` |
|        - |  7143 | `	}` |
|        5 |  7144 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7145 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7146 | `		return;` |
|        - |  7147 | `	}` |
|        5 |  7148 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7149 | `	/* Release values */` |
|        5 |  7150 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7151 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7152 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7153 | `	if( pCtx->pFrame ){` |
|        - |  7154 | `		VmSlot *aSlot;` |
|        - |  7155 | `		sxu32 n;` |
|        - |  7156 | `		/* Free local variables */` |
|        5 |  7157 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7158 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7159 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7160 | `		}` |
|        - |  7161 | `		/* Remove local references */` |
|        5 |  7162 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7163 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7164 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7165 | `		}` |
|        5 |  7166 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7167 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7168 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7169 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7170 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7171 | `		pCtx->pFrame = 0;` |
|        2 |  7172 | `	}` |
|        - |  7173 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7174 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7175 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7176 | `	if( pCtx->pStack ){` |
|        5 |  7177 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7178 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7179 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7180 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7181 | `				pTos--;` |
|        1 |  7182 | `			}` |
|        2 |  7183 | `		}` |
|        5 |  7184 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7185 | `		pCtx->pStack = 0;` |
|        2 |  7186 | `	}` |
|        - |  7187 | `	/* Free the context itself */` |
|        5 |  7188 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7189 |  |
|        - |  7190 | `/*` |
|        - |  7191 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7192 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7193 | ` */` |
|       90 |  7194 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7195 |  |
|        - |  7196 | `	ph7_class_instance *pThis;` |
|        - |  7197 | `	SyString sAttr;` |
|        - |  7198 | `	ph7_value *pAttr;` |
|       92 |  7199 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7200 | `		return 0;` |
|        - |  7201 | `	}` |
|       92 |  7202 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7203 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7204 | `		return 0;` |
|        - |  7205 | `	}` |
|       92 |  7206 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7207 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7208 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7209 | `		return 0;` |
|        - |  7210 | `	}` |
|       62 |  7211 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7212 |  |
|        - |  7213 | `/*` |
|        - |  7214 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7215 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7216 | ` */` |
|       38 |  7217 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7218 |  |
|       40 |  7219 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7220 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7221 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7222 | `			"Cannot suspend outside of a fiber");` |
|        - |  7223 | `	}` |
|       40 |  7224 | `	if( nArg > 0 ){` |
|       40 |  7225 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7226 | `	}else{` |
|      ! 0 |  7227 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7228 | `	}` |
|       40 |  7229 | `	return PH7_SUSPEND;` |
|       21 |  7230 |  |
|        - |  7231 | `/*` |
|        - |  7232 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7233 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7234 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7235 | ` */` |
|       24 |  7236 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7237 |  |
|        - |  7238 | `	ph7_class_instance *pThis;` |
|        - |  7239 | `	ph7_value *pAttr;` |
|        - |  7240 | `	SyString sAttrName;` |
|       26 |  7241 | `	if( nArg < 2 ){` |
|      ! 0 |  7242 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7243 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7244 | `	}` |
|       26 |  7245 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7246 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7247 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7248 | `	}` |
|       26 |  7249 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7250 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7251 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7252 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7253 | `	}` |
|        - |  7254 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7255 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7256 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7257 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7258 | `	}` |
|        - |  7259 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7260 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7261 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7262 | `	if( pAttr ){` |
|       26 |  7263 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7264 | `	}` |
|       26 |  7265 | `	return PH7_OK;` |
|       14 |  7266 |  |
|        - |  7267 | `/*` |
|        - |  7268 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7269 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7270 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7271 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7272 | ` */` |
|       24 |  7273 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7274 | `	ph7_class_instance **ppThis)` |
|        2 |  7275 |  |
|       26 |  7276 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7277 | `	ph7_value *pCallable;` |
|        - |  7278 | `	SyString sAttrName;` |
|       26 |  7279 | `	*ppThis = 0;` |
|       26 |  7280 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7281 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7282 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7283 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7284 | `		return 0;` |
|        - |  7285 | `	}` |
|       26 |  7286 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7287 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7288 | `		SyString sName;` |
|        - |  7289 | `		SyHashEntry *pEntry;` |
|        - |  7290 | `		ph7_vm_func *pFunc;` |
|       26 |  7291 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7292 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7293 | `		if( pEntry == 0 ){` |
|      ! 0 |  7294 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7295 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7296 | `			return 0;` |
|        - |  7297 | `		}` |
|       26 |  7298 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7299 | `		return pFunc;` |
|      ! 0 |  7300 | `	}else{` |
|        - |  7301 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7302 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7303 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7304 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7305 | `		if( pMethod == 0 ){` |
|      ! 0 |  7306 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7307 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7308 | `			return 0;` |
|        - |  7309 | `		}` |
|      ! 0 |  7310 | `		*ppThis = pClosure;` |
|      ! 0 |  7311 | `		return &pMethod->sFunc;` |
|        - |  7312 | `	}` |
|       14 |  7313 |  |
|        - |  7314 | `/*` |
|        - |  7315 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7316 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7317 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7318 | ` */` |
|       42 |  7319 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7320 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7321 |  |
|       44 |  7322 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7323 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7324 | `	sxu32 nFormal, n;` |
|        - |  7325 | `	VmSlot sSlot;` |
|        - |  7326 | `	sxi32 rc;` |
|        - |  7327 | `	/* Install $this for closure/method callables */` |
|       44 |  7328 | `	if( pClosureThis ){` |
|        - |  7329 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7330 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7331 | `		if( pObj ){` |
|      ! 0 |  7332 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7333 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7334 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7335 | `		}` |
|      ! 0 |  7336 | `	}` |
|        - |  7337 | `	/* Install static variables */` |
|       44 |  7338 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7339 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7340 | `		ph7_value *pVal;` |
|      ! 0 |  7341 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7342 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7343 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7344 | `			if( pVal ){` |
|      ! 0 |  7345 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7346 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7347 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7348 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7349 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7350 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7351 | `				}` |
|      ! 0 |  7352 | `			}` |
|      ! 0 |  7353 | `		}` |
|      ! 0 |  7354 | `	}` |
|        - |  7355 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7356 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7357 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7358 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7359 | `		ph7_value *pObj;` |
|       12 |  7360 | `		if( n < (sxu32)nArg ){` |
|        - |  7361 | `			/* Argument provided — install with type casting */` |
|       12 |  7362 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7363 | `			if( pObj ){` |
|       12 |  7364 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7365 | `				/* Type casting */` |
|       12 |  7366 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7367 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7368 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7369 | `						if( xCast ){` |
|      ! 0 |  7370 | `							xCast(pObj);` |
|      ! 0 |  7371 | `						}` |
|      ! 0 |  7372 | `					}` |
|      ! 0 |  7373 | `				}` |
|       12 |  7374 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7375 | `				sSlot.pUserData = 0;` |
|       12 |  7376 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7377 | `			}` |
|        5 |  7378 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7379 | `			/* Default value */` |
|      ! 0 |  7380 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7381 | `			if( pObj ){` |
|      ! 0 |  7382 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7383 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7384 | `					return rc;` |
|        - |  7385 | `				}` |
|      ! 0 |  7386 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7387 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7388 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7389 | `						if( xCast ){` |
|      ! 0 |  7390 | `							xCast(pObj);` |
|      ! 0 |  7391 | `						}` |
|      ! 0 |  7392 | `					}` |
|      ! 0 |  7393 | `				}` |
|      ! 0 |  7394 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7395 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7396 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7397 | `			}` |
|      ! 0 |  7398 | `		}` |
|        7 |  7399 | `	}` |
|        - |  7400 | `	/* Install closure environment (captured variables) */` |
|       44 |  7401 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7402 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7403 | `		ph7_value *pValue;` |
|        - |  7404 | `		sxu32 iEnv;` |
|        3 |  7405 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7406 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7407 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7408 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7409 | `				continue;` |
|        - |  7410 | `			}` |
|        5 |  7411 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7412 | `			if( pValue == 0 ){` |
|      ! 0 |  7413 | `				continue;` |
|        - |  7414 | `			}` |
|        5 |  7415 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7416 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7417 | `		}` |
|        1 |  7418 | `	}` |
|       44 |  7419 | `	return SXRET_OK;` |
|       23 |  7420 |  |
|        - |  7421 | `/*` |
|        - |  7422 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7423 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7424 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7425 | ` */` |
|       26 |  7426 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7427 |  |
|       28 |  7428 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7429 | `	ph7_class_instance *pThis;` |
|        - |  7430 | `	ph7_class_instance *pClosureThis;` |
|        - |  7431 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7432 | `	ph7_vm_func *pFunc;` |
|        - |  7433 | `	ph7_value sResult;` |
|        - |  7434 | `	ph7_value *pCtxAttr;` |
|        - |  7435 | `	SyString sAttrName;` |
|        - |  7436 | `	sxi32 rc;` |
|       28 |  7437 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7438 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7439 | `	}` |
|       28 |  7440 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7441 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7442 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7443 | `	if( pExecCtx != 0 ){` |
|        3 |  7444 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7445 | `			"Cannot start a fiber that has already been started");` |
|        - |  7446 | `	}` |
|        - |  7447 | `	/* Resolve callable */` |
|       26 |  7448 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7449 | `	if( pFunc == 0 ){` |
|      ! 0 |  7450 | `		return PH7_EXCEPTION;` |
|        - |  7451 | `	}` |
|        - |  7452 | `	/* Create execution context now that we know the function */` |
|       26 |  7453 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7454 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7455 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7456 | `			"Fiber::start(): out of memory");` |
|        - |  7457 | `	}` |
|        - |  7458 | `	/* Store context in $this->__ctx */` |
|       26 |  7459 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7460 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7461 | `	if( pCtxAttr ){` |
|       26 |  7462 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7463 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7464 | `	}` |
|        - |  7465 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7466 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7467 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7468 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7469 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7470 | `	/* Unpack the args array and install into the frame */` |
|        - |  7471 | `	{` |
|       26 |  7472 | `		ph7_value **apValues = 0;` |
|       26 |  7473 | `		int nActual = 0;` |
|       26 |  7474 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7475 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7476 | `			ph7_hashmap_node *pNode;` |
|       26 |  7477 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7478 | `			if( nCount > 0 ){` |
|        3 |  7479 | `				sxu32 idx = 0;` |
|        4 |  7480 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7481 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7482 | `				if( apValues ){` |
|        3 |  7483 | `					pNode = pMap->pFirst;` |
|        7 |  7484 | `					while( pNode && idx < nCount ){` |
|        5 |  7485 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7486 | `						idx++;` |
|        5 |  7487 | `						pNode = pNode->pPrev;` |
|        1 |  7488 | `					}` |
|        3 |  7489 | `					nActual = (int)idx;` |
|        1 |  7490 | `				}` |
|        1 |  7491 | `			}` |
|       12 |  7492 | `		}` |
|       26 |  7493 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7494 | `		if( apValues ){` |
|        3 |  7495 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7496 | `		}` |
|        - |  7497 | `	}` |
|        - |  7498 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7499 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7500 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7501 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7502 | `		return PH7_ABORT;` |
|        - |  7503 | `	}` |
|       26 |  7504 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7505 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7506 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7507 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7508 | `		return PH7_ABORT;` |
|        - |  7509 | `	}` |
|       26 |  7510 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7511 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7512 | `		return PH7_EXCEPTION;` |
|        - |  7513 | `	}` |
|       26 |  7514 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7515 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7516 | `	return PH7_OK;` |
|       15 |  7517 |  |
|        - |  7518 | `/*` |
|        - |  7519 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7520 | ` */` |
|       36 |  7521 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7522 |  |
|       38 |  7523 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7524 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7525 | `	ph7_value sResult;` |
|        - |  7526 | `	ph7_value *pResumeVal;` |
|        - |  7527 | `	sxi32 rc;` |
|       38 |  7528 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7529 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7530 | `		return PH7_OK;` |
|        - |  7531 | `	}` |
|       38 |  7532 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7533 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7534 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7535 | `		return PH7_OK;` |
|        - |  7536 | `	}` |
|       38 |  7537 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7538 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7539 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7540 | `	}` |
|       36 |  7541 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7542 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7543 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7544 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7545 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7546 | `		return PH7_ABORT;` |
|        - |  7547 | `	}` |
|       36 |  7548 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7549 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7550 | `		return PH7_EXCEPTION;` |
|        - |  7551 | `	}` |
|       36 |  7552 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7553 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7554 | `	return PH7_OK;` |
|       20 |  7555 |  |
|        - |  7556 | `/*` |
|        - |  7557 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7558 | ` */` |
|        6 |  7559 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7560 |  |
|        8 |  7561 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7562 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7563 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7564 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7565 | `		return PH7_OK;` |
|        - |  7566 | `	}` |
|        8 |  7567 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7568 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7569 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7570 | `		return PH7_OK;` |
|        - |  7571 | `	}` |
|        8 |  7572 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7573 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7574 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7575 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7576 | `		}` |
|      ! 0 |  7577 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7578 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7579 | `	}` |
|        8 |  7580 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7581 | `	return PH7_OK;` |
|        5 |  7582 |  |
|        - |  7583 | `/*` |
|        - |  7584 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7585 | ` */` |
|        6 |  7586 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7587 |  |
|        - |  7588 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7589 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7590 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7591 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7592 | `	return PH7_OK;` |
|        4 |  7593 |  |
|      ! 0 |  7594 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7595 |  |
|        - |  7596 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7597 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7598 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7599 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7600 | `	return PH7_OK;` |
|      ! 0 |  7601 |  |
|        6 |  7602 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7603 |  |
|        - |  7604 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7605 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7606 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7607 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7608 | `	return PH7_OK;` |
|        4 |  7609 |  |
|        6 |  7610 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7611 |  |
|        - |  7612 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7613 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7614 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7615 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7616 | `	return PH7_OK;` |
|        4 |  7617 |  |
|        - |  7618 | `/*` |
|        - |  7619 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7620 | ` */` |
|        4 |  7621 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7622 |  |
|        5 |  7623 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7624 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7625 | `	if( nArg < 1 ){` |
|      ! 0 |  7626 | `		return PH7_OK;` |
|        - |  7627 | `	}` |
|        5 |  7628 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7629 | `	if( pExecCtx ){` |
|        5 |  7630 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7631 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7632 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7633 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7634 | `			SyString sAttrName;` |
|        - |  7635 | `			ph7_value *pAttr;` |
|        5 |  7636 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7637 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7638 | `			if( pAttr ){` |
|        5 |  7639 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7640 | `			}` |
|        2 |  7641 | `		}` |
|        2 |  7642 | `	}` |
|        5 |  7643 | `	return PH7_OK;` |
|        3 |  7644 |  |
|        - |  7645 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7646 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7647 |  |
|        - |  7648 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7649 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7650 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7651 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7652 |  |
|      ! 0 |  7653 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7654 |  |
|        - |  7655 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7656 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7657 | `	ph7_exec_ctx *pCtx;` |
|        - |  7658 | `	ph7_vm_func *pFunc;` |
|        - |  7659 | `	ph7_value *pCallable;` |
|        - |  7660 | `	ph7_value *pCtxAttr;` |
|        - |  7661 | `	SyString sAttrName;` |
|        - |  7662 | `	/* Must not already be started */` |
|      ! 0 |  7663 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7664 | `	if( pCtx != 0 ){` |
|      ! 0 |  7665 | `		return SXERR_INVALID;` |
|        - |  7666 | `	}` |
|      ! 0 |  7667 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7668 | `		return SXERR_INVALID;` |
|        - |  7669 | `	}` |
|      ! 0 |  7670 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7671 | `	/* Get the callable */` |
|      ! 0 |  7672 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7673 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7674 | `	if( pCallable == 0 ){` |
|      ! 0 |  7675 | `		return SXERR_INVALID;` |
|        - |  7676 | `	}` |
|        - |  7677 | `	/* Resolve callable */` |
|      ! 0 |  7678 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7679 | `		SyString sName;` |
|        - |  7680 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7681 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7682 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7683 | `		if( pEntry == 0 ){` |
|      ! 0 |  7684 | `			return SXERR_NOTFOUND;` |
|        - |  7685 | `		}` |
|      ! 0 |  7686 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7687 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7688 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7689 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7690 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7691 | `		if( pMethod == 0 ){` |
|      ! 0 |  7692 | `			return SXERR_INVALID;` |
|        - |  7693 | `		}` |
|      ! 0 |  7694 | `		pClosureThis = pClosure;` |
|      ! 0 |  7695 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7696 | `	}else{` |
|      ! 0 |  7697 | `		return SXERR_INVALID;` |
|        - |  7698 | `	}` |
|        - |  7699 | `	/* Create context */` |
|      ! 0 |  7700 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7701 | `	if( pCtx == 0 ){` |
|      ! 0 |  7702 | `		return SXERR_MEM;` |
|        - |  7703 | `	}` |
|        - |  7704 | `	/* Store in __ctx */` |
|      ! 0 |  7705 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7706 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7707 | `	if( pCtxAttr ){` |
|      ! 0 |  7708 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7709 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7710 | `	}` |
|        - |  7711 | `	/* Set up frame with args */` |
|      ! 0 |  7712 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7713 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7714 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7715 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7716 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7717 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7718 |  |
|      ! 0 |  7719 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7720 |  |
|      ! 0 |  7721 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7722 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7723 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7724 |  |
|      ! 0 |  7725 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7726 |  |
|      ! 0 |  7727 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7728 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7729 |  |
|      ! 0 |  7730 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7731 |  |
|      ! 0 |  7732 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7733 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7734 |  |
|      ! 0 |  7735 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7736 |  |
|      ! 0 |  7737 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7738 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7739 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7740 |  |
|        - |  7741 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7742 | `/*` |
|        - |  7743 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7744 | ` */` |
|       18 |  7745 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7746 |  |
|        - |  7747 | `	ph7_generator *pGen;` |
|       20 |  7748 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7749 | `	if( pGen == 0 ){` |
|      ! 0 |  7750 | `		return 0;` |
|        - |  7751 | `	}` |
|       20 |  7752 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7753 | `	pGen->pCtx = pCtx;` |
|       20 |  7754 | `	pGen->iImplicitKey = 0;` |
|       20 |  7755 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7756 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7757 | `	/* Link the generator back to the exec context */` |
|       20 |  7758 | `	pCtx->pPrivate = pGen;` |
|       20 |  7759 | `	return pGen;` |
|       11 |  7760 |  |
|        - |  7761 | `/*` |
|        - |  7762 | ` * Release a generator and its execution context.` |
|        - |  7763 | ` */` |
|      ! 0 |  7764 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7765 |  |
|      ! 0 |  7766 | `	if( pGen == 0 ){` |
|      ! 0 |  7767 | `		return;` |
|        - |  7768 | `	}` |
|      ! 0 |  7769 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7770 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7771 | `	if( pGen->pCtx ){` |
|      ! 0 |  7772 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7773 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7774 | `		pGen->pCtx = 0;` |
|      ! 0 |  7775 | `	}` |
|      ! 0 |  7776 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7777 |  |
|        - |  7778 | `/*` |
|        - |  7779 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7780 | ` */` |
|      192 |  7781 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7782 |  |
|        - |  7783 | `	ph7_class_instance *pThis;` |
|        - |  7784 | `	SyString sAttr;` |
|        - |  7785 | `	ph7_value *pAttr;` |
|      194 |  7786 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7787 | `		return 0;` |
|        - |  7788 | `	}` |
|      194 |  7789 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7790 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7791 | `		return 0;` |
|        - |  7792 | `	}` |
|      194 |  7793 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7794 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7795 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7796 | `		return 0;` |
|        - |  7797 | `	}` |
|      194 |  7798 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7799 |  |
|        - |  7800 | `/*` |
|        - |  7801 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7802 | ` */` |
|       18 |  7803 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7804 |  |
|        - |  7805 | `	ph7_generator *pGen;` |
|        - |  7806 | `	sxi32 rc;` |
|       20 |  7807 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7808 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7809 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7810 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7811 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7812 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7813 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7814 | `	}` |
|       20 |  7815 | `	return PH7_OK;` |
|       11 |  7816 |  |
|        - |  7817 | `/*` |
|        - |  7818 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7819 | ` */` |
|       52 |  7820 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7821 |  |
|        - |  7822 | `	ph7_generator *pGen;` |
|       54 |  7823 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7824 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7825 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7826 | `	return PH7_OK;` |
|       28 |  7827 |  |
|        - |  7828 | `/*` |
|        - |  7829 | ` * Generator::current() — return the last yielded value.` |
|        - |  7830 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7831 | ` */` |
|       56 |  7832 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7833 |  |
|        - |  7834 | `	ph7_generator *pGen;` |
|        - |  7835 | `	sxi32 rc;` |
|       58 |  7836 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7837 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  7838 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7839 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7840 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7841 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7842 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7843 | `	}` |
|       58 |  7844 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  7845 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  7846 | `	}else{` |
|      ! 0 |  7847 | `		ph7_result_null(pCtx);` |
|        - |  7848 | `	}` |
|       58 |  7849 | `	return PH7_OK;` |
|       30 |  7850 |  |
|        - |  7851 | `/*` |
|        - |  7852 | ` * Generator::key() — return the last yielded key.` |
|        - |  7853 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7854 | ` */` |
|       12 |  7855 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7856 |  |
|        - |  7857 | `	ph7_generator *pGen;` |
|        - |  7858 | `	sxi32 rc;` |
|       13 |  7859 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7860 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7861 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7862 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7863 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7864 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7865 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7866 | `	}` |
|       13 |  7867 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7868 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7869 | `	}else{` |
|      ! 0 |  7870 | `		ph7_result_null(pCtx);` |
|        - |  7871 | `	}` |
|       13 |  7872 | `	return PH7_OK;` |
|        7 |  7873 |  |
|        - |  7874 | `/*` |
|        - |  7875 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7876 | ` */` |
|       48 |  7877 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7878 |  |
|        - |  7879 | `	ph7_generator *pGen;` |
|        - |  7880 | `	sxi32 rc;` |
|       50 |  7881 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  7882 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  7883 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  7884 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7885 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  7886 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  7887 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  7888 | `	}else{` |
|      ! 0 |  7889 | `		return PH7_OK;` |
|        - |  7890 | `	}` |
|       50 |  7891 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  7892 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  7893 | `	return PH7_OK;` |
|       26 |  7894 |  |
|        - |  7895 | `/*` |
|        - |  7896 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7897 | ` */` |
|        4 |  7898 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7899 |  |
|        - |  7900 | `	ph7_generator *pGen;` |
|        - |  7901 | `	ph7_value *pSendVal;` |
|        - |  7902 | `	sxi32 rc;` |
|        5 |  7903 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7904 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7905 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7906 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7907 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7908 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7909 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7910 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7911 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7912 | `	}else{` |
|      ! 0 |  7913 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7914 | `		return PH7_OK;` |
|        - |  7915 | `	}` |
|        5 |  7916 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7917 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7918 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7919 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7920 | `	}else{` |
|        3 |  7921 | `		ph7_result_null(pCtx);` |
|        - |  7922 | `	}` |
|        5 |  7923 | `	return PH7_OK;` |
|        3 |  7924 |  |
|        - |  7925 | `/*` |
|        - |  7926 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7927 | ` *` |
|        - |  7928 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7929 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7930 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7931 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7932 | ` * the exception to the caller.` |
|        - |  7933 | ` */` |
|      ! 0 |  7934 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7935 |  |
|        - |  7936 | `	ph7_generator *pGen;` |
|        - |  7937 | `	const char *zMsg;` |
|        - |  7938 | `	int nLen;` |
|      ! 0 |  7939 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7940 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7941 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7942 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7943 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7944 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7945 | `			"Cannot throw into a closed generator");` |
|        - |  7946 | `	}` |
|        - |  7947 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7948 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7949 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7950 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7951 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7952 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7953 | `	nLen = 0;` |
|      ! 0 |  7954 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7955 | `		/* Try to get the exception's message */` |
|        - |  7956 | `		SyString sAttr;` |
|        - |  7957 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7958 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7959 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7960 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7961 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7962 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7963 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7964 | `		}` |
|      ! 0 |  7965 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7966 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7967 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7968 | `	}` |
|      ! 0 |  7969 | `	(void)nLen;` |
|      ! 0 |  7970 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7971 |  |
|        - |  7972 | `/*` |
|        - |  7973 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7974 | ` */` |
|        2 |  7975 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7976 |  |
|        - |  7977 | `	ph7_generator *pGen;` |
|        3 |  7978 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7979 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7980 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7981 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7982 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7983 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7984 | `	}` |
|        3 |  7985 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7986 | `	return PH7_OK;` |
|        2 |  7987 |  |
|        - |  7988 | `/*` |
|        - |  7989 | ` * Generator::__destruct() — clean up.` |
|        - |  7990 | ` */` |
|      ! 0 |  7991 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7992 |  |
|        - |  7993 | `	ph7_generator *pGen;` |
|      ! 0 |  7994 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7995 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7996 | `	if( pGen ){` |
|      ! 0 |  7997 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7998 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7999 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8000 | `			SyString sAttrName;` |
|        - |  8001 | `			ph7_value *pAttr;` |
|      ! 0 |  8002 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8003 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8004 | `			if( pAttr ){` |
|      ! 0 |  8005 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8006 | `			}` |
|      ! 0 |  8007 | `		}` |
|      ! 0 |  8008 | `	}` |
|      ! 0 |  8009 | `	return PH7_OK;` |
|      ! 0 |  8010 |  |
|        - |  8011 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8012 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8013 | `/*` |
|        - |  8014 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8015 | ` * the desired message.` |
|        - |  8016 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8017 | ` * in 'api.c' for additional information.` |
|        - |  8018 | ` */` |
|      370 |  8019 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8020 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8021 | `	SyString *pString /* Message to output */` |
|        - |  8022 | `	)` |
|        2 |  8023 |  |
|      372 |  8024 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8025 | `	sxi32 rc = SXRET_OK;` |
|        - |  8026 | `	/* Call the output consumer */` |
|      372 |  8027 | `	if( pString->nByte > 0 ){` |
|      372 |  8028 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8029 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8030 | `	}` |
|      372 |  8031 | `	return rc;` |
|        2 |  8032 |  |
|        - |  8033 | `/*` |
|        - |  8034 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8035 | ` * callback to consume the formatted message.` |
|        - |  8036 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8037 | ` * in 'api.c' for additional information.` |
|        - |  8038 | ` */` |
|        2 |  8039 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8040 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8041 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8042 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8043 | `	)` |
|        1 |  8044 |  |
|        3 |  8045 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8046 | `	sxi32 rc = SXRET_OK;` |
|        - |  8047 | `	SyBlob sWorker;` |
|        - |  8048 | `	/* Format the message and call the output consumer */` |
|        3 |  8049 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8050 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8051 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8052 | `		/* Consume the formatted message */` |
|        3 |  8053 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8054 | `	}` |
|        3 |  8055 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8056 | `	/* Release the working buffer */` |
|        3 |  8057 | `	SyBlobRelease(&sWorker);` |
|        3 |  8058 | `	return rc;` |
|        1 |  8059 |  |
|        - |  8060 | `/*` |
|        - |  8061 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8062 | ` * This function never fail and always return a pointer` |
|        - |  8063 | ` * to a null terminated string.` |
|        - |  8064 | ` */` |
|       12 |  8065 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8066 |  |
|       13 |  8067 | `	const char *zOp = "Unknown     ";` |
|       13 |  8068 | `	switch(nOp){` |
|        3 |  8069 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8070 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8071 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8072 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8073 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8074 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8075 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8076 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8077 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8078 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8079 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8080 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8081 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8082 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8083 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8084 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8085 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8086 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8087 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8088 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8089 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8090 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8091 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8092 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8093 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8094 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8095 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8096 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8097 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8098 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8099 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8100 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8101 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8102 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8103 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8104 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8105 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8106 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8107 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8108 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8109 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8110 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8111 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8112 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8113 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8114 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8115 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8116 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8117 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8118 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8119 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8120 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8121 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8122 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8123 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8124 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8125 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8126 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8127 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8128 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8129 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8130 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8131 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8132 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8133 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8134 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8135 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8136 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8137 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8138 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8139 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8140 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8141 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8142 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8143 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8144 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8145 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8146 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8147 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8148 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8149 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8150 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8151 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8152 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8153 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8154 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8155 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8156 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8157 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8158 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8159 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8160 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8161 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8162 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8163 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8164 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8165 | `	default:` |
|      ! 0 |  8166 | `		break;` |
|        - |  8167 | `	}` |
|       13 |  8168 | `	return zOp;` |
|        1 |  8169 |  |
|        - |  8170 | `/*` |
|        - |  8171 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8172 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8173 | ` * is responsible of consuming the generated dump.` |
|        - |  8174 | ` */` |
|        2 |  8175 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8176 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8177 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8178 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8179 | `	)` |
|        1 |  8180 |  |
|        - |  8181 | `	sxi32 rc;` |
|        3 |  8182 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8183 | `	return rc;` |
|        1 |  8184 |  |
|        - |  8185 | `/*` |
|        - |  8186 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8187 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8188 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8189 | ` * in 'compile.c' for additional information.` |
|        - |  8190 | ` */` |
|       14 |  8191 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8192 |  |
|       15 |  8193 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8194 | `	/* Evaluate and expand constant value */` |
|       15 |  8195 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8196 |  |
|        - |  8197 | `/*` |
|        - |  8198 | ` * Section:` |
|        - |  8199 | ` *  Function handling functions.` |
|        - |  8200 | ` * Status:` |
|        - |  8201 | ` *    Stable.` |
|        - |  8202 | ` */` |
|        - |  8203 | `/*` |
|        - |  8204 | ` * int func_num_args(void)` |
|        - |  8205 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8206 | ` * Parameters` |
|        - |  8207 | ` *   None.` |
|        - |  8208 | ` * Return` |
|        - |  8209 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8210 | ` *  or -1 if called from the globe scope.` |
|        - |  8211 | ` */` |
|      936 |  8212 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8213 |  |
|        - |  8214 | `	VmFrame *pFrame;` |
|        - |  8215 | `	ph7_vm *pVm;` |
|        - |  8216 | `	/* Point to the target VM */` |
|      938 |  8217 | `	pVm = pCtx->pVm;` |
|        - |  8218 | `	/* Current frame */` |
|      938 |  8219 | `	pFrame = pVm->pFrame;` |
|      938 |  8220 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      938 |  8221 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8222 | `		SXUNUSED(nArg);` |
|      ! 0 |  8223 | `		SXUNUSED(apArg);` |
|        - |  8224 | `		/* Global frame,return -1 */` |
|      ! 0 |  8225 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8226 | `		return SXRET_OK;` |
|        - |  8227 | `	}` |
|        - |  8228 | `	/* Total number of arguments passed to the enclosing function */` |
|      938 |  8229 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      938 |  8230 | `	ph7_result_int(pCtx,nArg);` |
|      938 |  8231 | `	return SXRET_OK;` |
|      470 |  8232 |  |
|        - |  8233 | `/*` |
|        - |  8234 | ` * value func_get_arg(int $arg_num)` |
|        - |  8235 | ` *   Return an item from the argument list.` |
|        - |  8236 | ` * Parameters` |
|        - |  8237 | ` *  Argument number(index start from zero).` |
|        - |  8238 | ` * Return` |
|        - |  8239 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8240 | ` */` |
|       22 |  8241 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8242 |  |
|       24 |  8243 | `	ph7_value *pObj = 0;` |
|       24 |  8244 | `	VmSlot *pSlot = 0;` |
|        - |  8245 | `	VmFrame *pFrame;` |
|        - |  8246 | `	ph7_vm *pVm;` |
|        - |  8247 | `	/* Point to the target VM */` |
|       24 |  8248 | `	pVm = pCtx->pVm;` |
|        - |  8249 | `	/* Current frame */` |
|       24 |  8250 | `	pFrame = pVm->pFrame;` |
|       24 |  8251 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8252 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8253 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8254 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8255 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8256 | `		return SXRET_OK;` |
|        - |  8257 | `	}` |
|        - |  8258 | `	/* Extract the desired index */` |
|       21 |  8259 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8260 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8261 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8262 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8263 | `		return SXRET_OK;` |
|        - |  8264 | `	}` |
|        - |  8265 | `	/* Extract the desired argument */` |
|       21 |  8266 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8267 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8268 | `			/* Return the desired argument */` |
|       21 |  8269 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8270 | `		}else{` |
|        - |  8271 | `			/* No such argument,return false */` |
|      ! 0 |  8272 | `			ph7_result_bool(pCtx,0);` |
|        - |  8273 | `		}` |
|       11 |  8274 | `	}else{` |
|        - |  8275 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8276 | `		ph7_result_bool(pCtx,0);` |
|        - |  8277 | `	}` |
|       21 |  8278 | `	return SXRET_OK;` |
|       13 |  8279 |  |
|        - |  8280 | `/*` |
|        - |  8281 | ` * array func_get_args_byref(void)` |
|        - |  8282 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8283 | ` * Parameters` |
|        - |  8284 | ` *  None.` |
|        - |  8285 | ` * Return` |
|        - |  8286 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8287 | ` *  member of the current user-defined function's argument list.` |
|        - |  8288 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8289 | ` * NOTE:` |
|        - |  8290 | ` *  Arguments are returned to the array by reference.` |
|        - |  8291 | ` */` |
|        2 |  8292 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8293 |  |
|        - |  8294 | `	ph7_value *pArray;` |
|        - |  8295 | `	VmFrame *pFrame;` |
|        - |  8296 | `	VmSlot *aSlot;` |
|        - |  8297 | `	sxu32 n;` |
|        - |  8298 | `	/* Point to the current frame */` |
|        3 |  8299 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8300 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8301 | `	if( pFrame->pParent == 0 ){` |
|        - |  8302 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8303 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8304 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8305 | `		return SXRET_OK;` |
|        - |  8306 | `	}` |
|        - |  8307 | `	/* Create a new array */` |
|        3 |  8308 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8309 | `	if( pArray == 0 ){` |
|      ! 0 |  8310 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8311 | `		SXUNUSED(apArg);` |
|      ! 0 |  8312 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8313 | `		return SXRET_OK;` |
|        - |  8314 | `	}` |
|        - |  8315 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8316 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8317 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8318 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8319 | `	}` |
|        - |  8320 | `	/* Return the freshly created array */` |
|        3 |  8321 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8322 | `	return SXRET_OK;` |
|        2 |  8323 |  |
|        - |  8324 | `/*` |
|        - |  8325 | ` * array func_get_args(void)` |
|        - |  8326 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8327 | ` * Parameters` |
|        - |  8328 | ` *  None.` |
|        - |  8329 | ` * Return` |
|        - |  8330 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8331 | ` *  member of the current user-defined function's argument list.` |
|        - |  8332 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8333 | ` */` |
|       88 |  8334 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8335 |  |
|       90 |  8336 | `	ph7_value *pObj = 0;` |
|        - |  8337 | `	ph7_value *pArray;` |
|        - |  8338 | `	VmFrame *pFrame;` |
|        - |  8339 | `	VmSlot *aSlot;` |
|        - |  8340 | `	sxu32 n;` |
|        - |  8341 | `	/* Point to the current frame */` |
|       90 |  8342 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8343 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8344 | `	if( pFrame->pParent == 0 ){` |
|        - |  8345 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8346 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8347 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8348 | `		return SXRET_OK;` |
|        - |  8349 | `	}` |
|        - |  8350 | `	/* Create a new array */` |
|       90 |  8351 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8352 | `	if( pArray == 0 ){` |
|      ! 0 |  8353 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8354 | `		SXUNUSED(apArg);` |
|      ! 0 |  8355 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8356 | `		return SXRET_OK;` |
|        - |  8357 | `	}` |
|        - |  8358 | `	/* Start filling the array with the given arguments */` |
|       90 |  8359 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8360 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8361 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8362 | `		if( pObj ){` |
|      134 |  8363 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8364 | `		}` |
|       68 |  8365 | `	}` |
|        - |  8366 | `	/* Return the freshly created array */` |
|       90 |  8367 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8368 | `	return SXRET_OK;` |
|       46 |  8369 |  |
|        - |  8370 | `/*` |
|        - |  8371 | ` * bool function_exists(string $name)` |
|        - |  8372 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8373 | ` * Parameters` |
|        - |  8374 | ` *  The name of the desired function.` |
|        - |  8375 | ` * Return` |
|        - |  8376 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8377 | ` */` |
|     1682 |  8378 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8379 |  |
|        - |  8380 | `	const char *zName;` |
|        - |  8381 | `	ph7_vm *pVm;` |
|        - |  8382 | `	int nLen;` |
|        - |  8383 | `	int res;` |
|     1684 |  8384 | `	if( nArg < 1 ){` |
|        - |  8385 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8386 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8387 | `		return SXRET_OK;` |
|        - |  8388 | `	}` |
|        - |  8389 | `	/* Point to the target VM */` |
|     1684 |  8390 | `	pVm = pCtx->pVm;` |
|        - |  8391 | `	/* Extract the function name */` |
|     1684 |  8392 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8393 | `	/* Assume the function is not defined */` |
|     1684 |  8394 | `	res = 0;` |
|        - |  8395 | `	/* Perform the lookup */` |
|     2523 |  8396 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8397 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8398 | `			/* Function is defined */` |
|      206 |  8399 | `			res = 1;` |
|      102 |  8400 | `	}` |
|     1684 |  8401 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8402 | `	return SXRET_OK;` |
|      843 |  8403 |  |
|        - |  8404 | `/*` |
|        - |  8405 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8406 | ` * [i.e: Whether it is callable or not].` |
|        - |  8407 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8408 | ` */` |
|    17628 |  8409 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8410 |  |
|    17630 |  8411 | `	int res = 0;` |
|    17630 |  8412 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8413 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8414 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8415 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8416 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8417 | `		if( pMethod && CallInvoke ){` |
|        - |  8418 | `			ph7_value sResult;` |
|        - |  8419 | `			sxi32 rc;` |
|        - |  8420 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8421 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8422 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8423 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8424 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8425 | `			}` |
|      ! 0 |  8426 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8427 | `		}` |
|    17630 |  8428 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8429 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8430 | `		if( pMap->nEntry == 2 ){` |
|        - |  8431 | `			ph7_class *pClass;` |
|        - |  8432 | `			ph7_value *pV;` |
|        - |  8433 | `			/* Extract the target class */` |
|       12 |  8434 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8435 | `			if( pV ){` |
|       12 |  8436 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8437 | `				if( pClass ){` |
|        - |  8438 | `					ph7_class_method *pMethod;` |
|        - |  8439 | `					/* Extract the target method */` |
|       10 |  8440 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8441 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8442 | `						/* Perform the lookup */` |
|       10 |  8443 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8444 | `						if( pMethod ){` |
|        - |  8445 | `							/* Method is callable */` |
|        5 |  8446 | `							res = 1;` |
|        2 |  8447 | `						}` |
|        4 |  8448 | `					}` |
|        4 |  8449 | `				}` |
|        5 |  8450 | `			}` |
|        7 |  8451 | `		}` |
|    17617 |  8452 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8453 | `		const char *zName;` |
|        - |  8454 | `		int nLen;` |
|        - |  8455 | `		/* Extract the name */` |
|     4992 |  8456 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8457 | `		/* Perform the lookup */` |
|     5007 |  8458 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8459 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8460 | `				/* Function is callable */` |
|     4974 |  8461 | `				res = 1;` |
|     2486 |  8462 | `		}` |
|     2495 |  8463 | `	}` |
|    17630 |  8464 | `	return res;` |
|        2 |  8465 |  |
|        - |  8466 | `/*` |
|        - |  8467 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8468 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8469 | ` * Parameters` |
|        - |  8470 | ` * $name` |
|        - |  8471 | ` *    The callback function to check` |
|        - |  8472 | ` * $syntax_only` |
|        - |  8473 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8474 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8475 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8476 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8477 | ` *    a string.` |
|        - |  8478 | ` * Return` |
|        - |  8479 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8480 | ` */` |
|       14 |  8481 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8482 |  |
|        - |  8483 | `	ph7_vm *pVm;` |
|        - |  8484 | `	int res;` |
|       15 |  8485 | `	if( nArg < 1 ){` |
|        - |  8486 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8487 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8488 | `		return SXRET_OK;` |
|        - |  8489 | `	}` |
|        - |  8490 | `	/* Point to the target VM */` |
|       15 |  8491 | `	pVm = pCtx->pVm;` |
|        - |  8492 | `	/* Perform the requested operation */` |
|       15 |  8493 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8494 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8495 | `	return SXRET_OK;` |
|        8 |  8496 |  |
|        - |  8497 | `/*` |
|        - |  8498 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8499 | ` * defined below.` |
|        - |  8500 | ` */` |
|     1196 |  8501 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8502 |  |
|     1197 |  8503 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8504 | `	ph7_value sName;` |
|        - |  8505 | `	sxi32 rc;` |
|        - |  8506 | `	/* Prepare the function name for insertion */` |
|     1197 |  8507 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1197 |  8508 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8509 | `	/* Perform the insertion */` |
|     1197 |  8510 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1197 |  8511 | `	PH7_MemObjRelease(&sName);` |
|     1197 |  8512 | `	return rc;` |
|        1 |  8513 |  |
|        - |  8514 | `/*` |
|        - |  8515 | ` * array get_defined_functions(void)` |
|        - |  8516 | ` *  Returns an array of all defined functions.` |
|        - |  8517 | ` * Parameter` |
|        - |  8518 | ` *  None.` |
|        - |  8519 | ` * Return` |
|        - |  8520 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8521 | ` *  both built-in (internal) and user-defined.` |
|        - |  8522 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8523 | ` *  defined ones using $arr["user"].` |
|        - |  8524 | ` * Note:` |
|        - |  8525 | ` *  NULL is returned on failure.` |
|        - |  8526 | ` */` |
|        2 |  8527 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8528 |  |
|        - |  8529 | `	ph7_value *pArray,*pEntry;` |
|        - |  8530 | `	/* NOTE:` |
|        - |  8531 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8532 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8533 | `	 */` |
|        3 |  8534 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8535 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8536 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8537 | `		SXUNUSED(apArg);` |
|        - |  8538 | `		/* Return NULL */` |
|      ! 0 |  8539 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8540 | `		return SXRET_OK;` |
|        - |  8541 | `	}` |
|        3 |  8542 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8543 | `	if( pEntry == 0 ){` |
|        - |  8544 | `		/* Return NULL */` |
|      ! 0 |  8545 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8546 | `		return SXRET_OK;` |
|        - |  8547 | `	}` |
|        - |  8548 | `	/* Fill with the appropriate information */` |
|        3 |  8549 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8550 | `	/* Create the 'internal' index */` |
|        3 |  8551 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8552 | `	/* Create the user-func array */` |
|        3 |  8553 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8554 | `	if( pEntry == 0 ){` |
|        - |  8555 | `		/* Return NULL */` |
|      ! 0 |  8556 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8557 | `		return SXRET_OK;` |
|        - |  8558 | `	}` |
|        - |  8559 | `	/* Fill with the appropriate information */` |
|        3 |  8560 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8561 | `	/* Create the 'user' index */` |
|        3 |  8562 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8563 | `	/* Return the multi-dimensional array */` |
|        3 |  8564 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8565 | `	return SXRET_OK;` |
|        2 |  8566 |  |
|        - |  8567 | `/*` |
|        - |  8568 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8569 | ` *  Register a function for execution on shutdown.` |
|        - |  8570 | ` * Note` |
|        - |  8571 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8572 | ` *  be called in the same order as they were registered.` |
|        - |  8573 | ` * Parameters` |
|        - |  8574 | ` *  $callback` |
|        - |  8575 | ` *   The shutdown callback to register.` |
|        - |  8576 | ` * $param` |
|        - |  8577 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8578 | ` * Return` |
|        - |  8579 | ` *  Nothing.` |
|        - |  8580 | ` */` |
|        2 |  8581 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8582 |  |
|        - |  8583 | `	VmShutdownCB sEntry;` |
|        - |  8584 | `	int i,j;` |
|        3 |  8585 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8586 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8587 | `		return PH7_OK;` |
|        - |  8588 | `	}` |
|        - |  8589 | `	/* Zero the Entry */` |
|        3 |  8590 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8591 | `	/* Initialize fields */` |
|        3 |  8592 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8593 | `	/* Save the callback name for later invocation name */` |
|        3 |  8594 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8595 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8596 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8597 | `	}` |
|        - |  8598 | `	/* Copy arguments */` |
|        3 |  8599 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8600 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8601 | `			/* Limit reached */` |
|      ! 0 |  8602 | `			break;` |
|        - |  8603 | `		}` |
|      ! 0 |  8604 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8605 | `	}` |
|        3 |  8606 | `	sEntry.nArg = j;` |
|        - |  8607 | `	/* Install the callback */` |
|        3 |  8608 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8609 | `	return PH7_OK;` |
|        2 |  8610 |  |
|        - |  8611 | `/*` |
|        - |  8612 | ` * Section:` |
|        - |  8613 | ` *  Class handling functions.` |
|        - |  8614 | ` * Status:` |
|        - |  8615 | ` *    Stable.` |
|        - |  8616 | ` */` |
|        - |  8617 | `/*` |
|        - |  8618 | ` * Extract the top active class. NULL is returned` |
|        - |  8619 | ` * if the class stack is empty.` |
|        - |  8620 | ` */` |
|      566 |  8621 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8622 |  |
|      568 |  8623 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8624 | `	ph7_class **apClass;` |
|      568 |  8625 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8626 | `		/* Empty stack,return NULL */` |
|       15 |  8627 | `		return 0;` |
|        - |  8628 | `	}` |
|        - |  8629 | `	/* Peek the last entry */` |
|      554 |  8630 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      554 |  8631 | `	return apClass[pSet->nUsed - 1];` |
|      285 |  8632 |  |
|        - |  8633 | `/*` |
|        - |  8634 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8635 | ` *   Get the class that declared the currently executing method.` |
|        - |  8636 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8637 | ` *` |
|        - |  8638 | ` * Parameters` |
|        - |  8639 | ` *   pVm: Target VM` |
|        - |  8640 | ` *` |
|        - |  8641 | ` * Return` |
|        - |  8642 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8643 | ` *   - Not executing within a class method` |
|        - |  8644 | ` *` |
|        - |  8645 | ` * Note` |
|        - |  8646 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8647 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8648 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8649 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8650 | ` *   declaring class.` |
|        - |  8651 | ` */` |
|       60 |  8652 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8653 |  |
|       62 |  8654 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8655 | `	ph7_vm_func *pVmFunc;` |
|        - |  8656 |  |
|        - |  8657 | `	/* Skip exception frames to find the actual method frame */` |
|       62 |  8658 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8659 |  |
|        - |  8660 | `	/* Check if we're in a method context */` |
|       62 |  8661 | `	if( pFrame->pParent ){` |
|       58 |  8662 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       58 |  8663 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8664 | `			/* Return the declaring class */` |
|       58 |  8665 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8666 | `		}` |
|      ! 0 |  8667 | `	}` |
|        - |  8668 |  |
|        5 |  8669 | `	return 0;` |
|       32 |  8670 |  |
|        - |  8671 |  |
|        - |  8672 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8673 | `/*` |
|        - |  8674 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8675 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8676 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8677 | ` * return value indicates failure.` |
|        - |  8678 | ` */` |
|     1492 |  8679 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8680 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8681 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8682 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8683 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8684 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8685 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8686 | `	)` |
|        2 |  8687 |  |
|        - |  8688 | `	ph7_value *aStack;` |
|        - |  8689 | `	VmInstr aInstr[2];` |
|        - |  8690 | `	int iCursor;` |
|        - |  8691 | `	int i;` |
|        - |  8692 | `	/* Create a new operand stack */` |
|     1494 |  8693 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1494 |  8694 | `	if( aStack == 0 ){` |
|      ! 0 |  8695 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8696 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8697 | `		return SXERR_MEM;` |
|        - |  8698 | `	}` |
|        - |  8699 | `	/* Fill the operand stack with the given arguments */` |
|     2100 |  8700 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      608 |  8701 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8702 | `		/*` |
|        - |  8703 | `		 * Symisc eXtension:` |
|        - |  8704 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8705 | `		 */` |
|      608 |  8706 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      305 |  8707 | `	}` |
|     1494 |  8708 | `	iCursor = nArg + 1;` |
|     1494 |  8709 | `	if( pThis ){` |
|        - |  8710 | `		/*` |
|        - |  8711 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8712 | `		 */` |
|     1488 |  8713 | `		pThis->iRef++; /* Increment reference count */` |
|     1488 |  8714 | `		aStack[i].x.pOther = pThis;` |
|     1488 |  8715 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      743 |  8716 | `	}` |
|     1494 |  8717 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1494 |  8718 | `	i++;` |
|        - |  8719 | `	/* Push method name */` |
|     1494 |  8720 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1494 |  8721 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1494 |  8722 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1494 |  8723 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8724 | `	/* Emit the CALL istruction */` |
|     1494 |  8725 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1494 |  8726 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1494 |  8727 | `	aInstr[0].iP2 = 0;` |
|     1494 |  8728 | `	aInstr[0].p3  = 0;` |
|        - |  8729 | `	/* Emit the DONE instruction */` |
|     1494 |  8730 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1494 |  8731 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1494 |  8732 | `	aInstr[1].iP2 = 0;` |
|     1494 |  8733 | `	aInstr[1].p3  = 0;` |
|        - |  8734 | `	/* Execute the method body (if available) */` |
|     1494 |  8735 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8736 | `	/* Clean up the mess left behind */` |
|     1494 |  8737 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1494 |  8738 | `	return PH7_OK;` |
|      748 |  8739 |  |
|        - |  8740 | `/*` |
|        - |  8741 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8742 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8743 | ` * in the apArg[] array.` |
|        - |  8744 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8745 | ` * return value indicates failure.` |
|        - |  8746 | ` */` |
|      952 |  8747 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8748 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8749 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8750 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8751 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8752 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8753 | `	)` |
|        2 |  8754 |  |
|        - |  8755 | `	ph7_value *aStack;` |
|        - |  8756 | `	VmInstr aInstr[2];` |
|        - |  8757 | `	int i;` |
|      954 |  8758 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8759 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8760 | `		if( pResult ){` |
|        - |  8761 | `			/* Assume a null return value */` |
|      ! 0 |  8762 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8763 | `		}` |
|      471 |  8764 | `		return SXERR_INVALID;` |
|        - |  8765 | `	}` |
|      484 |  8766 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8767 | `		/* Class method */` |
|       11 |  8768 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8769 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8770 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8771 | `		ph7_class *pClass = 0;` |
|        - |  8772 | `		ph7_value *pValue;` |
|        - |  8773 | `		sxi32 rc;` |
|       11 |  8774 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8775 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8776 | `			if( pResult ){` |
|        - |  8777 | `				/* Assume a null return value */` |
|      ! 0 |  8778 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8779 | `			}` |
|      ! 0 |  8780 | `			return SXRET_OK;` |
|        - |  8781 | `		}` |
|        - |  8782 | `		/* Extract the class name or an instance of it */` |
|       11 |  8783 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8784 | `		if( pValue ){` |
|       11 |  8785 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8786 | `		}` |
|       11 |  8787 | `		if( pClass == 0 ){` |
|        - |  8788 | `			/* No such class,return NULL */` |
|      ! 0 |  8789 | `			if( pResult ){` |
|      ! 0 |  8790 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8791 | `			}` |
|      ! 0 |  8792 | `			return SXRET_OK;` |
|        - |  8793 | `		}` |
|       11 |  8794 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8795 | `			/* Point to the class instance */` |
|        5 |  8796 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8797 | `		}` |
|        - |  8798 | `		/* Try to extract the method */` |
|       11 |  8799 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8800 | `		if( pValue ){` |
|       11 |  8801 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8802 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8803 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8804 | `			}` |
|        5 |  8805 | `		}` |
|       11 |  8806 | `		if( pMethod == 0 ){` |
|        - |  8807 | `			/* No such method,return NULL */` |
|      ! 0 |  8808 | `			if( pResult ){` |
|      ! 0 |  8809 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8810 | `			}` |
|      ! 0 |  8811 | `			return SXRET_OK;` |
|        - |  8812 | `		}` |
|        - |  8813 | `		/* Call the class method */` |
|       11 |  8814 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8815 | `		return rc;` |
|        - |  8816 | `	}` |
|        - |  8817 | `	/* Create a new operand stack */` |
|      474 |  8818 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8819 | `	if( aStack == 0 ){` |
|      ! 0 |  8820 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8821 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8822 | `		if( pResult ){` |
|        - |  8823 | `			/* Assume a null return value */` |
|      ! 0 |  8824 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8825 | `		}` |
|      ! 0 |  8826 | `		return SXERR_MEM;` |
|        - |  8827 | `	}` |
|        - |  8828 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8829 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8830 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8831 | `		/*` |
|        - |  8832 | `		 * Symisc eXtension:` |
|        - |  8833 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8834 | `		 */` |
|     1050 |  8835 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8836 | `	}` |
|        - |  8837 | `	/* Push the function name */` |
|      474 |  8838 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  8839 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8840 | `	/* Emit the CALL istruction */` |
|      474 |  8841 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  8842 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  8843 | `	aInstr[0].iP2 = 0;` |
|      474 |  8844 | `	aInstr[0].p3  = 0;` |
|        - |  8845 | `	/* Emit the DONE instruction */` |
|      474 |  8846 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  8847 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  8848 | `	aInstr[1].iP2 = 0;` |
|      474 |  8849 | `	aInstr[1].p3  = 0;` |
|        - |  8850 | `	/* Execute the function body (if available) */` |
|      474 |  8851 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8852 | `	/* Clean up the mess left behind */` |
|      474 |  8853 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  8854 | `	return PH7_OK;` |
|      478 |  8855 |  |
|        - |  8856 | `/*` |
|        - |  8857 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8858 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8859 | ` * parameter.` |
|        - |  8860 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8861 | ` * return value indicates failure.` |
|        - |  8862 | ` */` |
|      236 |  8863 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8864 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8865 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8866 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8867 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8868 | `	)` |
|        1 |  8869 |  |
|        - |  8870 | `	ph7_value *pArg;` |
|        - |  8871 | `	SySet aArg;` |
|        - |  8872 | `	va_list ap;` |
|        - |  8873 | `	sxi32 rc;` |
|      237 |  8874 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8875 | `	/* Copy arguments one after one */` |
|      237 |  8876 | `	va_start(ap,pResult);` |
|      393 |  8877 | `	for(;;){` |
|      787 |  8878 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8879 | `		if( pArg == 0 ){` |
|      237 |  8880 | `			break;` |
|        - |  8881 | `		}` |
|      551 |  8882 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8883 | `	}` |
|        - |  8884 | `	/* Call the core routine */` |
|      237 |  8885 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8886 | `	/* Cleanup */` |
|      237 |  8887 | `	SySetRelease(&aArg);` |
|      237 |  8888 | `	return rc;` |
|        1 |  8889 |  |
|        - |  8890 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8891 | `/*` |
|        - |  8892 | ` * bool defined(string $name)` |
|        - |  8893 | ` *  Checks whether a given named constant exists.` |
|        - |  8894 | ` * Parameter:` |
|        - |  8895 | ` *  Name of the desired constant.` |
|        - |  8896 | ` * Return` |
|        - |  8897 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8898 | ` */` |
|       14 |  8899 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8900 |  |
|        - |  8901 | `	const char *zName;` |
|       16 |  8902 | `	int nLen = 0;` |
|       16 |  8903 | `	int res = 0;` |
|       16 |  8904 | `	if( nArg < 1 ){` |
|        - |  8905 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8906 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8907 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8908 | `		return SXRET_OK;` |
|        - |  8909 | `	}` |
|        - |  8910 | `	/* Extract constant name */` |
|       16 |  8911 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8912 | `	/* Perform the lookup */` |
|       16 |  8913 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8914 | `		/* Already defined */` |
|       10 |  8915 | `		res = 1;` |
|        4 |  8916 | `	}` |
|       16 |  8917 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8918 | `	return SXRET_OK;` |
|        9 |  8919 |  |
|        - |  8920 | `/*` |
|        - |  8921 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8922 | ` * below.` |
|        - |  8923 | ` */` |
|       10 |  8924 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8925 |  |
|       12 |  8926 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8927 | `	/* Expand constant value */` |
|       12 |  8928 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  8929 |  |
|        - |  8930 | `/*` |
|        - |  8931 | ` * bool define(string $constant_name,expression value)` |
|        - |  8932 | ` *  Defines a named constant at runtime.` |
|        - |  8933 | ` * Parameter:` |
|        - |  8934 | ` *  $constant_name` |
|        - |  8935 | ` *   The name of the constant` |
|        - |  8936 | ` *  $value` |
|        - |  8937 | ` *   Constant value` |
|        - |  8938 | ` * Return:` |
|        - |  8939 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8940 | ` */` |
|       12 |  8941 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8942 |  |
|        - |  8943 | `	const char *zName;  /* Constant name */` |
|        - |  8944 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  8945 | `	int nLen = 0;       /* Name length */` |
|        - |  8946 | `	sxi32 rc;` |
|       14 |  8947 | `	if( nArg < 2 ){` |
|        - |  8948 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8949 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8950 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8951 | `		return SXRET_OK;` |
|        - |  8952 | `	}` |
|       14 |  8953 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8954 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8955 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8956 | `		return SXRET_OK;` |
|        - |  8957 | `	}` |
|        - |  8958 | `	/* Extract constant name */` |
|       14 |  8959 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  8960 | `	if( nLen < 1 ){` |
|      ! 0 |  8961 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8962 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8963 | `		return SXRET_OK;` |
|        - |  8964 | `	}` |
|        - |  8965 | `	/* Duplicate constant value */` |
|       14 |  8966 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  8967 | `	if( pValue == 0 ){` |
|      ! 0 |  8968 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8969 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8970 | `		return SXRET_OK;` |
|        - |  8971 | `	}` |
|        - |  8972 | `	/* Initialize the memory object */` |
|       14 |  8973 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8974 | `	/* Register the constant */` |
|       14 |  8975 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  8976 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8977 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8978 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8979 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8980 | `		return SXRET_OK;` |
|        - |  8981 | `	}` |
|        - |  8982 | `	/* Duplicate constant value */` |
|       14 |  8983 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  8984 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8985 | `		/* Lower case the constant name */` |
|      ! 0 |  8986 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8987 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8988 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8989 | `				/* UTF-8 stream */` |
|      ! 0 |  8990 | `				zCur++;` |
|      ! 0 |  8991 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8992 | `					zCur++;` |
|      ! 0 |  8993 | `				}` |
|      ! 0 |  8994 | `				continue;` |
|        - |  8995 | `			}` |
|      ! 0 |  8996 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8997 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8998 | `				zCur[0] = (char)c;` |
|      ! 0 |  8999 | `			}` |
|      ! 0 |  9000 | `			zCur++;` |
|      ! 0 |  9001 | `		}` |
|        - |  9002 | `		/* Finally,register the constant */` |
|      ! 0 |  9003 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9004 | `	}` |
|        - |  9005 | `	/* All done,return TRUE */` |
|       14 |  9006 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9007 | `	return SXRET_OK;` |
|        8 |  9008 |  |
|        - |  9009 | `/*` |
|        - |  9010 | ` * value constant(string $name)` |
|        - |  9011 | ` *  Returns the value of a constant` |
|        - |  9012 | ` * Parameter` |
|        - |  9013 | ` *  $name` |
|        - |  9014 | ` *    Name of the constant.` |
|        - |  9015 | ` * Return` |
|        - |  9016 | ` *  Constant value or NULL if not defined.` |
|        - |  9017 | ` */` |
|        8 |  9018 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9019 |  |
|        - |  9020 | `	SyHashEntry *pEntry;` |
|        - |  9021 | `	ph7_constant *pCons;` |
|        - |  9022 | `	const char *zName; /* Constant name */` |
|        - |  9023 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9024 | `	int nLen;` |
|       10 |  9025 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9026 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9027 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9028 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9029 | `		return SXRET_OK;` |
|        - |  9030 | `	}` |
|        - |  9031 | `	/* Extract the constant name */` |
|       10 |  9032 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9033 | `	/* Perform the query */` |
|       10 |  9034 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9035 | `	if( pEntry == 0 ){` |
|        3 |  9036 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9037 | `		ph7_result_null(pCtx);` |
|        3 |  9038 | `		return SXRET_OK;` |
|        - |  9039 | `	}` |
|        8 |  9040 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9041 | `	/* Point to the structure that describe the constant */` |
|        8 |  9042 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9043 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9044 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9045 | `	/* Return that value */` |
|        8 |  9046 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9047 | `	/* Cleanup */` |
|        8 |  9048 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9049 | `	return SXRET_OK;` |
|        6 |  9050 |  |
|        - |  9051 | `/*` |
|        - |  9052 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9053 | ` * defined below.` |
|        - |  9054 | ` */` |
|      452 |  9055 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9056 |  |
|      453 |  9057 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9058 | `	ph7_value sName;` |
|        - |  9059 | `	sxi32 rc;` |
|        - |  9060 | `	/* Prepare the constant name for insertion */` |
|      453 |  9061 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9062 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9063 | `	/* Perform the insertion */` |
|      453 |  9064 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9065 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9066 | `	return rc;` |
|        1 |  9067 |  |
|        - |  9068 | `/*` |
|        - |  9069 | ` * array get_defined_constants(void)` |
|        - |  9070 | ` *  Returns an associative array with the names of all defined` |
|        - |  9071 | ` *  constants.` |
|        - |  9072 | ` * Parameters` |
|        - |  9073 | ` *  NONE.` |
|        - |  9074 | ` * Returns` |
|        - |  9075 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9076 | ` */` |
|        2 |  9077 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9078 |  |
|        - |  9079 | `	ph7_value *pArray;` |
|        - |  9080 | `	/* Create the array first*/` |
|        3 |  9081 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9082 | `	if( pArray == 0 ){` |
|      ! 0 |  9083 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9084 | `		SXUNUSED(apArg);` |
|        - |  9085 | `		/* Return NULL */` |
|      ! 0 |  9086 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9087 | `		return SXRET_OK;` |
|        - |  9088 | `	}` |
|        - |  9089 | `	/* Fill the array with the defined constants */` |
|        3 |  9090 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9091 | `	/* Return the created array */` |
|        3 |  9092 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9093 | `	return SXRET_OK;` |
|        2 |  9094 |  |
|        - |  9095 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9096 | `/*` |
|        - |  9097 | ` * Section:` |
|        - |  9098 | ` *  Random numbers/string generators.` |
|        - |  9099 | ` * Status:` |
|        - |  9100 | ` *    Stable.` |
|        - |  9101 | ` */` |
|        - |  9102 | `/*` |
|        - |  9103 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9104 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9105 | ` * used by te SQLite3 library.` |
|        - |  9106 | ` */` |
|     2388 |  9107 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9108 |  |
|        - |  9109 | `	sxu32 iNum;` |
|     2390 |  9110 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2390 |  9111 | `	return iNum;` |
|        2 |  9112 |  |
|        - |  9113 | `/*` |
|        - |  9114 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9115 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9116 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9117 | ` * by te SQLite3 library.` |
|        - |  9118 | ` */` |
|   124246 |  9119 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9120 |  |
|        - |  9121 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9122 | `	int i;` |
|        - |  9123 | `	/* Generate a binary string first */` |
|   124248 |  9124 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9125 | `	/* Turn the binary string into english based alphabet */` |
|  1366876 |  9126 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1242630 |  9127 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   621316 |  9128 | `	 }` |
|   124248 |  9129 |  |
|        - |  9130 | `/*` |
|        - |  9131 | ` * int rand()` |
|        - |  9132 | ` * int mt_rand()` |
|        - |  9133 | ` * int rand(int $min,int $max)` |
|        - |  9134 | ` * int mt_rand(int $min,int $max)` |
|        - |  9135 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9136 | ` * Parameter` |
|        - |  9137 | ` *  $min` |
|        - |  9138 | ` *    The lowest value to return (default: 0)` |
|        - |  9139 | ` *  $max` |
|        - |  9140 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9141 | ` * Return` |
|        - |  9142 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9143 | ` * Note:` |
|        - |  9144 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9145 | ` *  by te SQLite3 library.` |
|        - |  9146 | ` */` |
|       20 |  9147 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9148 |  |
|        - |  9149 | `	sxu32 iNum;` |
|        - |  9150 | `	/* Generate the random number */` |
|       21 |  9151 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9152 | `	if( nArg > 1 ){` |
|        - |  9153 | `		sxu32 iMin,iMax;` |
|        3 |  9154 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9155 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9156 | `		if( iMin < iMax ){` |
|        3 |  9157 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9158 | `			if( iDiv > 0 ){` |
|        3 |  9159 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9160 | `			}` |
|        1 |  9161 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9162 | `			iNum %= iMax;` |
|      ! 0 |  9163 | `		}` |
|        1 |  9164 | `	}` |
|        - |  9165 | `	/* Return the number */` |
|       21 |  9166 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9167 | `	return SXRET_OK;` |
|        1 |  9168 |  |
|        - |  9169 | `/*` |
|        - |  9170 | ` * int getrandmax(void)` |
|        - |  9171 | ` * int mt_getrandmax(void)` |
|        - |  9172 | ` * int rc4_getrandmax(void)` |
|        - |  9173 | ` *   Show largest possible random value` |
|        - |  9174 | ` * Return` |
|        - |  9175 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9176 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9177 | ` * Note:` |
|        - |  9178 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9179 | ` *  by te SQLite3 library.` |
|        - |  9180 | ` */` |
|        4 |  9181 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9182 |  |
|        2 |  9183 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9184 | `	SXUNUSED(apArg);` |
|        5 |  9185 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9186 | `	return SXRET_OK;` |
|        1 |  9187 |  |
|        - |  9188 | `/*` |
|        - |  9189 | ` * string rand_str()` |
|        - |  9190 | ` * string rand_str(int $len)` |
|        - |  9191 | ` *  Generate a random string (English alphabet).` |
|        - |  9192 | ` * Parameter` |
|        - |  9193 | ` *  $len` |
|        - |  9194 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9195 | ` * Return` |
|        - |  9196 | ` *   A pseudo random string.` |
|        - |  9197 | ` * Note:` |
|        - |  9198 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9199 | ` *  by te SQLite3 library.` |
|        - |  9200 | ` *  This function is a symisc extension.` |
|        - |  9201 | ` */` |
|      120 |  9202 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9203 |  |
|        - |  9204 | `	char zString[1024];` |
|      122 |  9205 | `	int iLen = 0x10;` |
|      122 |  9206 | `	if( nArg > 0 ){` |
|        - |  9207 | `		/* Get the desired length */` |
|      122 |  9208 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9209 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9210 | `			/* Default length */` |
|        3 |  9211 | `			iLen = 0x10;` |
|        1 |  9212 | `		}` |
|       60 |  9213 | `	}` |
|        - |  9214 | `	/* Generate the random string */` |
|      122 |  9215 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9216 | `	/* Return the generated string */` |
|      122 |  9217 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9218 | `	return SXRET_OK;` |
|        2 |  9219 |  |
|        - |  9220 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9221 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9222 | `/* Unique ID private data */` |
|        - |  9223 | `struct unique_id_data` |
|        - |  9224 |  |
|        - |  9225 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9226 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9227 | `};` |
|        - |  9228 | `/*` |
|        - |  9229 | ` * Binary to hex consumer callback.` |
|        - |  9230 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9231 | ` * defined below.` |
|        - |  9232 | ` */` |
|      192 |  9233 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9234 |  |
|      193 |  9235 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9236 | `	sxu32 nBuflen;` |
|        - |  9237 | `	/* Extract result buffer length */` |
|      193 |  9238 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9239 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9240 | `			/*` |
|        - |  9241 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9242 | `			 * string will be 13 characters long` |
|        - |  9243 | `			 */` |
|       25 |  9244 | `		return SXERR_ABORT;` |
|        - |  9245 | `	}` |
|      169 |  9246 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9247 | `		return SXERR_ABORT;` |
|        - |  9248 | `	}` |
|        - |  9249 | `	/* Safely Consume the hex stream */` |
|      169 |  9250 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9251 | `	return SXRET_OK;` |
|       97 |  9252 |  |
|        - |  9253 | `/*` |
|        - |  9254 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9255 | ` *  Generate a unique ID` |
|        - |  9256 | ` * Parameter` |
|        - |  9257 | ` * $prefix` |
|        - |  9258 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9259 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9260 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9261 | ` * $more_entropy` |
|        - |  9262 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9263 | ` *  that the result will be unique.` |
|        - |  9264 | ` * Return` |
|        - |  9265 | ` *  Returns the unique identifier, as a string.` |
|        - |  9266 | ` */` |
|       24 |  9267 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9268 |  |
|        - |  9269 | `	struct unique_id_data sUniq;` |
|        - |  9270 | `	unsigned char zDigest[20];` |
|       25 |  9271 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9272 | `	const char *zPrefix;` |
|        - |  9273 | `	SHA1Context sCtx;` |
|        - |  9274 | `	char zRandom[7];` |
|        - |  9275 | `	int nPrefix;` |
|        - |  9276 | `	int entropy;` |
|        - |  9277 | `	/* Generate a random string first */` |
|       25 |  9278 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9279 | `	/* Initialize fields */` |
|       25 |  9280 | `	zPrefix = 0;` |
|       25 |  9281 | `	nPrefix = 0;` |
|       25 |  9282 | `	entropy = 0;` |
|       25 |  9283 | `	if( nArg > 0 ){` |
|        - |  9284 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9285 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9286 | `		if( nArg > 1 ){` |
|      ! 0 |  9287 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9288 | `		}` |
|      ! 0 |  9289 | `	}` |
|       25 |  9290 | `	SHA1Init(&sCtx);` |
|        - |  9291 | `	/* Generate the random ID */` |
|       25 |  9292 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9293 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9294 | `	}` |
|        - |  9295 | `	/* Append the random ID */` |
|       25 |  9296 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9297 | `	/* Append the random string */` |
|       25 |  9298 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9299 | `	/* Increment the number */` |
|       25 |  9300 | `	pVm->unique_id++;` |
|       25 |  9301 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9302 | `	/* Hexify the digest */` |
|       25 |  9303 | `	sUniq.pCtx = pCtx;` |
|       25 |  9304 | `	sUniq.entropy = entropy;` |
|       25 |  9305 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9306 | `	/* All done */` |
|       25 |  9307 | `	return PH7_OK;` |
|        1 |  9308 |  |
|        - |  9309 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9310 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9311 | `/*` |
|        - |  9312 | ` * Section:` |
|        - |  9313 | ` *  Language construct implementation as foreign functions.` |
|        - |  9314 | ` * Status:` |
|        - |  9315 | ` *    Stable.` |
|        - |  9316 | ` */` |
|        - |  9317 | `/*` |
|        - |  9318 | ` * void echo($string...)` |
|        - |  9319 | ` *  Output one or more messages.` |
|        - |  9320 | ` * Parameters` |
|        - |  9321 | ` *  $string` |
|        - |  9322 | ` *   Message to output.` |
|        - |  9323 | ` * Return` |
|        - |  9324 | ` *  NULL.` |
|        - |  9325 | ` */` |
|      ! 0 |  9326 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9327 |  |
|        - |  9328 | `	const char *zData;` |
|      ! 0 |  9329 | `	int nDataLen = 0;` |
|        - |  9330 | `	ph7_vm *pVm;` |
|        - |  9331 | `	int i,rc;` |
|        - |  9332 | `	/* Point to the target VM */` |
|      ! 0 |  9333 | `	pVm = pCtx->pVm;` |
|        - |  9334 | `	/* Output */` |
|      ! 0 |  9335 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9336 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9337 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9338 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9339 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9340 | `			if( rc == SXERR_ABORT ){` |
|        - |  9341 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9342 | `				return PH7_ABORT;` |
|        - |  9343 | `			}` |
|      ! 0 |  9344 | `		}` |
|      ! 0 |  9345 | `	}` |
|      ! 0 |  9346 | `	return SXRET_OK;` |
|      ! 0 |  9347 |  |
|        - |  9348 | `/*` |
|        - |  9349 | ` * int print($string...)` |
|        - |  9350 | ` *  Output one or more messages.` |
|        - |  9351 | ` * Parameters` |
|        - |  9352 | ` *  $string` |
|        - |  9353 | ` *   Message to output.` |
|        - |  9354 | ` * Return` |
|        - |  9355 | ` *  1 always.` |
|        - |  9356 | ` */` |
|        2 |  9357 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9358 |  |
|        - |  9359 | `	const char *zData;` |
|        3 |  9360 | `	int nDataLen = 0;` |
|        - |  9361 | `	ph7_vm *pVm;` |
|        - |  9362 | `	int i,rc;` |
|        - |  9363 | `	/* Point to the target VM */` |
|        3 |  9364 | `	pVm = pCtx->pVm;` |
|        - |  9365 | `	/* Output */` |
|        5 |  9366 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9367 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9368 | `		if( nDataLen > 0 ){` |
|        3 |  9369 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9370 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9371 | `			if( rc == SXERR_ABORT ){` |
|        - |  9372 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9373 | `				return PH7_ABORT;` |
|        - |  9374 | `			}` |
|        1 |  9375 | `		}` |
|        2 |  9376 | `	}` |
|        - |  9377 | `	/* Return 1 */` |
|        3 |  9378 | `	ph7_result_int(pCtx,1);` |
|        3 |  9379 | `	return SXRET_OK;` |
|        2 |  9380 |  |
|        - |  9381 | `/*` |
|        - |  9382 | ` * void exit(string $msg)` |
|        - |  9383 | ` * void exit(int $status)` |
|        - |  9384 | ` * void die(string $ms)` |
|        - |  9385 | ` * void die(int $status)` |
|        - |  9386 | ` *   Output a message and terminate program execution.` |
|        - |  9387 | ` * Parameter` |
|        - |  9388 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9389 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9390 | ` *  and not printed` |
|        - |  9391 | ` * Return` |
|        - |  9392 | ` *  NULL` |
|        - |  9393 | ` */` |
|      ! 0 |  9394 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9395 |  |
|      ! 0 |  9396 | `	if( nArg > 0 ){` |
|      ! 0 |  9397 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9398 | `			const char *zData;` |
|      ! 0 |  9399 | `			int iLen = 0;` |
|        - |  9400 | `			/* Print exit message */` |
|      ! 0 |  9401 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9402 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9403 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9404 | `			sxi32 iExitStatus;` |
|        - |  9405 | `			/* Record exit status code */` |
|      ! 0 |  9406 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9407 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9408 | `		}` |
|      ! 0 |  9409 | `	}` |
|        - |  9410 | `	/* Check if we are in an included file */` |
|      ! 0 |  9411 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9412 | `		/* Exit the entire process */` |
|      ! 0 |  9413 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9414 | `	}` |
|        - |  9415 | `	/* Abort processing immediately */` |
|      ! 0 |  9416 | `	return PH7_ABORT;` |
|      ! 0 |  9417 |  |
|        - |  9418 | `/*` |
|        - |  9419 | ` * bool isset($var,...)` |
|        - |  9420 | ` *  Finds out whether a variable is set.` |
|        - |  9421 | ` * Parameters` |
|        - |  9422 | ` *  One or more variable to check.` |
|        - |  9423 | ` * Return` |
|        - |  9424 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9425 | ` */` |
|    75310 |  9426 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9427 |  |
|        - |  9428 | `	ph7_value *pObj;` |
|    75312 |  9429 | `	int res = 0;` |
|        - |  9430 | `	int i;` |
|    75312 |  9431 | `	if( nArg < 1 ){` |
|        - |  9432 | `		/* Missing arguments,return false */` |
|      ! 0 |  9433 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9434 | `		return SXRET_OK;` |
|        - |  9435 | `	}` |
|        - |  9436 | `	/* Iterate over available arguments */` |
|    99238 |  9437 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    75312 |  9438 | `		pObj = apArg[i];` |
|    75312 |  9439 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    50868 |  9440 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9441 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9442 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9443 | `			}` |
|    25433 |  9444 | `		}` |
|    75312 |  9445 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    75312 |  9446 | `		if( !res ){` |
|        - |  9447 | `			/* Variable not set,return FALSE */` |
|    51386 |  9448 | `			ph7_result_bool(pCtx,0);` |
|    51386 |  9449 | `			return SXRET_OK;` |
|        - |  9450 | `		}` |
|    11965 |  9451 | `	}` |
|        - |  9452 | `	/* All given variable are set,return TRUE */` |
|    23928 |  9453 | `	ph7_result_bool(pCtx,1);` |
|    23928 |  9454 | `	return SXRET_OK;` |
|    37657 |  9455 |  |
|        - |  9456 | `/*` |
|        - |  9457 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9458 | ` * frame,the reference table and discard it's contents.` |
|        - |  9459 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9460 | ` */` |
|  3022470 |  9461 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9462 |  |
|        - |  9463 | `	ph7_value *pObj;` |
|        - |  9464 | `	VmRefObj *pRef;` |
|  3022472 |  9465 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3022472 |  9466 | `	if( pObj ){` |
|        - |  9467 | `		/* Release the object */` |
|  3022472 |  9468 | `		PH7_MemObjRelease(pObj);` |
|  1511235 |  9469 | `	}` |
|        - |  9470 | `	/* Remove old reference links */` |
|  3022472 |  9471 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3022472 |  9472 | `	if( pRef ){` |
|  3022466 |  9473 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9474 | `		/* Unlink from the reference table */` |
|  3022466 |  9475 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3022466 |  9476 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9477 | `			VmSlot sFree;` |
|        - |  9478 | `			/* Restore to the free list */` |
|  3022460 |  9479 | `			sFree.nIdx = nObjIdx;` |
|  3022460 |  9480 | `			sFree.pUserData = 0;` |
|  3022460 |  9481 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1511229 |  9482 | `		}` |
|  1511232 |  9483 | `	}` |
|  3022472 |  9484 | `	return SXRET_OK;` |
|        2 |  9485 |  |
|        - |  9486 | `/*` |
|        - |  9487 | ` * void unset($var,...)` |
|        - |  9488 | ` *   Unset one or more given variable.` |
|        - |  9489 | ` * Parameters` |
|        - |  9490 | ` *  One or more variable to unset.` |
|        - |  9491 | ` * Return` |
|        - |  9492 | ` *  Nothing.` |
|        - |  9493 | ` */` |
|     6764 |  9494 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9495 |  |
|        - |  9496 | `	ph7_value *pObj;` |
|        - |  9497 | `	ph7_vm *pVm;` |
|        - |  9498 | `	int i;` |
|        - |  9499 | `	/* Point to the target VM */` |
|     6766 |  9500 | `	pVm = pCtx->pVm;` |
|        - |  9501 | `	/* Iterate and unset */` |
|    13530 |  9502 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6766 |  9503 | `		pObj = apArg[i];` |
|     6766 |  9504 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9505 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9506 | `				/* Throw an error */` |
|      ! 0 |  9507 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9508 | `			}` |
|      ! 0 |  9509 | `		}else{` |
|     6766 |  9510 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9511 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6766 |  9512 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6760 |  9513 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3379 |  9514 | `			}` |
|        - |  9515 | `		}` |
|     3384 |  9516 | `	}` |
|     6766 |  9517 | `	return SXRET_OK;` |
|        2 |  9518 |  |
|        - |  9519 | `/*` |
|        - |  9520 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9521 | ` */` |
|      110 |  9522 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9523 |  |
|      111 |  9524 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9525 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9526 | `	ph7_value *pObj;` |
|        - |  9527 | `	sxu32 nIdx;` |
|        - |  9528 | `	/* Extract the memory object */` |
|      111 |  9529 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9530 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9531 | `	if( pObj ){` |
|      111 |  9532 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9533 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9534 | `				SyString sName;` |
|        - |  9535 | `				ph7_value sKey;` |
|        - |  9536 | `				/* Perform the insertion */` |
|      109 |  9537 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9538 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9539 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9540 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9541 | `			}` |
|       54 |  9542 | `		}` |
|       55 |  9543 | `	}` |
|      111 |  9544 | `	return SXRET_OK;` |
|        1 |  9545 |  |
|        - |  9546 | `/*` |
|        - |  9547 | ` * array get_defined_vars(void)` |
|        - |  9548 | ` *  Returns an array of all defined variables.` |
|        - |  9549 | ` * Parameter` |
|        - |  9550 | ` *  None` |
|        - |  9551 | ` * Return` |
|        - |  9552 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9553 | ` */` |
|        2 |  9554 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9555 |  |
|        3 |  9556 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9557 | `	ph7_value *pArray;` |
|        - |  9558 | `	/* Create a new array */` |
|        3 |  9559 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9560 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9561 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9562 | `		SXUNUSED(apArg);` |
|        - |  9563 | `		/* Return NULL */` |
|      ! 0 |  9564 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9565 | `		return SXRET_OK;` |
|        - |  9566 | `	}` |
|        - |  9567 | `	/* Superglobals first */` |
|        3 |  9568 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9569 | `	/* Then variable defined in the current frame */` |
|        3 |  9570 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9571 | `	/* Finally,return the created array */` |
|        3 |  9572 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9573 | `	return SXRET_OK;` |
|        2 |  9574 |  |
|        - |  9575 | `/*` |
|        - |  9576 | ` * bool gettype($var)` |
|        - |  9577 | ` *  Get the type of a variable` |
|        - |  9578 | ` * Parameters` |
|        - |  9579 | ` *   $var` |
|        - |  9580 | ` *    The variable being type checked.` |
|        - |  9581 | ` * Return` |
|        - |  9582 | ` *   String representation of the given variable type.` |
|        - |  9583 | ` */` |
|       32 |  9584 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9585 |  |
|       34 |  9586 | `	const char *zType = "Empty";` |
|       34 |  9587 | `	if( nArg > 0 ){` |
|       34 |  9588 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9589 | `	}` |
|        - |  9590 | `	/* Return the variable type */` |
|       34 |  9591 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9592 | `	return SXRET_OK;` |
|        2 |  9593 |  |
|        - |  9594 | `/*` |
|        - |  9595 | ` * string get_resource_type(resource $handle)` |
|        - |  9596 | ` *  This function gets the type of the given resource.` |
|        - |  9597 | ` * Parameters` |
|        - |  9598 | ` *  $handle` |
|        - |  9599 | ` *  The evaluated resource handle.` |
|        - |  9600 | ` * Return` |
|        - |  9601 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9602 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9603 | ` *  the return value will be the string Unknown.` |
|        - |  9604 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9605 | ` *  is not a resource.` |
|        - |  9606 | ` */` |
|        2 |  9607 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9608 |  |
|        3 |  9609 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9610 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9611 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9612 | `		return PH7_OK;` |
|        - |  9613 | `	}` |
|        3 |  9614 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9615 | `	return SXRET_OK;` |
|        2 |  9616 |  |
|        - |  9617 | `/*` |
|        - |  9618 | ` * void var_dump(expression,....)` |
|        - |  9619 | ` *   var_dump � Dumps information about a variable` |
|        - |  9620 | ` * Parameters` |
|        - |  9621 | ` *   One or more expression to dump.` |
|        - |  9622 | ` * Returns` |
|        - |  9623 | ` *  Nothing.` |
|        - |  9624 | ` */` |
|      218 |  9625 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9626 |  |
|        - |  9627 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9628 | `	int i;` |
|      220 |  9629 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9630 | `	/* Dump one or more expressions */` |
|      444 |  9631 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9632 | `		ph7_value *pObj = apArg[i];` |
|        - |  9633 | `		/* Reset the working buffer */` |
|      226 |  9634 | `		SyBlobReset(&sDump);` |
|        - |  9635 | `		/* Dump the given expression */` |
|      226 |  9636 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9637 | `		/* Output */` |
|      226 |  9638 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9639 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9640 | `		}` |
|      114 |  9641 | `	}` |
|        - |  9642 | `	/* Release the working buffer */` |
|      220 |  9643 | `	SyBlobRelease(&sDump);` |
|      220 |  9644 | `	return SXRET_OK;` |
|        2 |  9645 |  |
|        - |  9646 | `/*` |
|        - |  9647 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9648 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9649 | ` * Parameters` |
|        - |  9650 | ` *   expression: Expression to dump` |
|        - |  9651 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9652 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9653 | ` *            print_r() will return the information rather than print it.` |
|        - |  9654 | ` * Return` |
|        - |  9655 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9656 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9657 | ` */` |
|       16 |  9658 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9659 |  |
|       17 |  9660 | `	int ret_string = 0;` |
|        - |  9661 | `	SyBlob sDump;` |
|       17 |  9662 | `	if( nArg < 1 ){` |
|        - |  9663 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9664 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9665 | `		return SXRET_OK;` |
|        - |  9666 | `	}` |
|       17 |  9667 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9668 | `	if ( nArg > 1 ){` |
|        - |  9669 | `		/* Where to redirect output */` |
|       11 |  9670 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9671 | `	}` |
|        - |  9672 | `	/* Generate dump */` |
|       17 |  9673 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9674 | `	if( !ret_string ){` |
|        - |  9675 | `		/* Output dump */` |
|        7 |  9676 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9677 | `		/* Return true */` |
|        7 |  9678 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9679 | `	}else{` |
|        - |  9680 | `		/* Generated dump as return value */` |
|       11 |  9681 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9682 | `	}` |
|        - |  9683 | `	/* Release the working buffer */` |
|       17 |  9684 | `	SyBlobRelease(&sDump);` |
|       17 |  9685 | `	return SXRET_OK;` |
|        9 |  9686 |  |
|        - |  9687 | `/*` |
|        - |  9688 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9689 | ` * Same job as print_r. (see coment above)` |
|        - |  9690 | ` */` |
|        2 |  9691 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9692 |  |
|        3 |  9693 | `	int ret_string = 0;` |
|        - |  9694 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9695 | `	if( nArg < 1 ){` |
|        - |  9696 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9697 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9698 | `		return SXRET_OK;` |
|        - |  9699 | `	}` |
|        3 |  9700 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9701 | `	if ( nArg > 1 ){` |
|        - |  9702 | `		/* Where to redirect output */` |
|        3 |  9703 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9704 | `	}` |
|        - |  9705 | `	/* Generate dump */` |
|        3 |  9706 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9707 | `	if( !ret_string ){` |
|        - |  9708 | `		/* Output dump */` |
|      ! 0 |  9709 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9710 | `		/* Return NULL */` |
|      ! 0 |  9711 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9712 | `	}else{` |
|        - |  9713 | `		/* Generated dump as return value */` |
|        3 |  9714 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9715 | `	}` |
|        - |  9716 | `	/* Release the working buffer */` |
|        3 |  9717 | `	SyBlobRelease(&sDump);` |
|        3 |  9718 | `	return SXRET_OK;` |
|        2 |  9719 |  |
|        - |  9720 | `/*` |
|        - |  9721 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9722 | ` *  Set/get the various assert flags.` |
|        - |  9723 | ` * Parameter` |
|        - |  9724 | ` * $what` |
|        - |  9725 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9726 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9727 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9728 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9729 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9730 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9731 | ` * $value` |
|        - |  9732 | ` *   An optional new value for the option.` |
|        - |  9733 | ` * Return` |
|        - |  9734 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9735 | ` */` |
|       28 |  9736 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9737 |  |
|       30 |  9738 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9739 | `	int iOption;` |
|        - |  9740 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9741 | `	if( nArg < 1 ){` |
|        3 |  9742 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9743 | `			"ArgumentCountError",` |
|        - |  9744 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9745 | `			);` |
|        - |  9746 | `	}` |
|        - |  9747 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9748 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9749 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9750 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9751 | `			"TypeError",` |
|        - |  9752 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9753 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9754 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9755 | `			);` |
|        - |  9756 | `	}` |
|       28 |  9757 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9758 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9759 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9760 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9761 | `	switch( iOption ){` |
|        5 |  9762 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9763 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9764 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9765 | `		if( nArg > 1 ){` |
|        5 |  9766 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9767 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9768 | `			}else{` |
|        3 |  9769 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9770 | `			}` |
|        2 |  9771 | `		}` |
|       12 |  9772 | `		break;` |
|        1 |  9773 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9774 | `		/* Return old callback or null */` |
|        3 |  9775 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9776 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9777 | `		}else{` |
|        3 |  9778 | `			ph7_result_null(pCtx);` |
|        - |  9779 | `		}` |
|        3 |  9780 | `		if( nArg > 1 ){` |
|      ! 0 |  9781 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9782 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9783 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9784 | `			}else{` |
|      ! 0 |  9785 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9786 | `			}` |
|      ! 0 |  9787 | `		}` |
|        3 |  9788 | `		break;` |
|        5 |  9789 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9790 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9791 | `		if( nArg > 1 ){` |
|        5 |  9792 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9793 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9794 | `			}else{` |
|        3 |  9795 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9796 | `			}` |
|        2 |  9797 | `		}` |
|       11 |  9798 | `		break;` |
|      ! 0 |  9799 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9800 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9801 | `		break;` |
|        1 |  9802 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9803 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9804 | `		break;` |
|      ! 0 |  9805 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9806 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9807 | `		break;` |
|        1 |  9808 | `	default:` |
|        - |  9809 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9810 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9811 | `			"ValueError",` |
|        - |  9812 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9813 | `			);` |
|        - |  9814 | `	}` |
|       26 |  9815 | `	return PH7_OK;` |
|       16 |  9816 |  |
|        - |  9817 | `/*` |
|        - |  9818 | ` * bool assert(mixed $assertion)` |
|        - |  9819 | ` *  Checks if assertion is FALSE.` |
|        - |  9820 | ` * Parameter` |
|        - |  9821 | ` *  $assertion` |
|        - |  9822 | ` *    The assertion to test.` |
|        - |  9823 | ` * Return` |
|        - |  9824 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9825 | ` */` |
|       24 |  9826 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9827 |  |
|       26 |  9828 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9829 | `	int iFlags,iResult;` |
|        - |  9830 | `	const char *zDesc;` |
|        - |  9831 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9832 | `	if( nArg < 1 ){` |
|        3 |  9833 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9834 | `			"ArgumentCountError",` |
|        - |  9835 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9836 | `			);` |
|        - |  9837 | `	}` |
|       24 |  9838 | `	iFlags = pVm->iAssertFlags;` |
|       24 |  9839 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9840 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9841 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9842 | `		return PH7_OK;` |
|        - |  9843 | `	}` |
|        - |  9844 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 |  9845 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 |  9846 | `	if( !iResult ){` |
|        - |  9847 | `		/* Assertion failed */` |
|        - |  9848 | `		/* Extract optional description */` |
|       13 |  9849 | `		zDesc = 0;` |
|       13 |  9850 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9851 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9852 | `		}` |
|       13 |  9853 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9854 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9855 | `			ph7_value sFile,sLine;` |
|        - |  9856 | `			ph7_value *apCbArg[3];` |
|        - |  9857 | `			SyString *pFile;` |
|        - |  9858 | `			/* Extract the processed script */` |
|      ! 0 |  9859 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9860 | `			if( pFile == 0 ){` |
|      ! 0 |  9861 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9862 | `			}` |
|        - |  9863 | `			/* Invoke the callback */` |
|      ! 0 |  9864 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9865 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9866 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9867 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9868 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9869 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9870 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9871 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9872 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9873 | `		}` |
|       13 |  9874 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9875 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9876 | `			return PH7_ABORT;` |
|        - |  9877 | `		}` |
|        - |  9878 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9879 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9880 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9881 | `				"AssertionError",` |
|        - |  9882 | `				"%s",` |
|        1 |  9883 | `				zDesc` |
|        - |  9884 | `				);` |
|      ! 0 |  9885 | `		}else{` |
|       11 |  9886 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9887 | `				"AssertionError",` |
|        - |  9888 | `				"assert(false)"` |
|        - |  9889 | `				);` |
|        - |  9890 | `		}` |
|        - |  9891 | `	}` |
|        - |  9892 | `	/* Assertion passed */` |
|       11 |  9893 | `	ph7_result_bool(pCtx,1);` |
|       11 |  9894 | `	return PH7_OK;` |
|       14 |  9895 |  |
|        - |  9896 | `/*` |
|        - |  9897 | ` * Section:` |
|        - |  9898 | ` *  Error reporting functions.` |
|        - |  9899 | ` * Status:` |
|        - |  9900 | ` *    Stable.` |
|        - |  9901 | ` */` |
|        - |  9902 | `/*` |
|        - |  9903 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9904 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9905 | ` * Parameters` |
|        - |  9906 | ` *  $error_msg` |
|        - |  9907 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9908 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9909 | ` * $error_type` |
|        - |  9910 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9911 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9912 | ` * Return` |
|        - |  9913 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9914 | ` */` |
|       12 |  9915 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9916 |  |
|       14 |  9917 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9918 | `	int rc = PH7_OK;` |
|       14 |  9919 | `	if( nArg > 0 ){` |
|        - |  9920 | `		const char *zErr;` |
|        - |  9921 | `		int nLen;` |
|        - |  9922 | `		/* Extract the error message */` |
|       12 |  9923 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9924 | `		if( nArg > 1 ){` |
|        - |  9925 | `			/* Extract the error type */` |
|       12 |  9926 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9927 | `			switch( nErr ){` |
|        1 |  9928 | `			case 1:   /* E_ERROR */` |
|        - |  9929 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9930 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9931 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9932 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9933 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9934 | `				break;` |
|        1 |  9935 | `			case 2:   /* E_WARNING */` |
|        - |  9936 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9937 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9938 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9939 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9940 | `				break;` |
|        3 |  9941 | `			default:` |
|        8 |  9942 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9943 | `				break;` |
|        - |  9944 | `			}` |
|        5 |  9945 | `		}` |
|        - |  9946 | `		/* Report error */` |
|       12 |  9947 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9948 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9949 | `			return rc;` |
|        - |  9950 | `		}` |
|        - |  9951 | `		/* Return true */` |
|       12 |  9952 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9953 | `	}else{` |
|        - |  9954 | `		/* Missing arguments,return FALSE */` |
|        3 |  9955 | `		ph7_result_bool(pCtx,0);` |
|        - |  9956 | `	}` |
|       14 |  9957 | `	return rc;` |
|        8 |  9958 |  |
|        - |  9959 | `/*` |
|        - |  9960 | ` * int error_reporting([int $level])` |
|        - |  9961 | ` *  Sets which PHP errors are reported.` |
|        - |  9962 | ` * Parameters` |
|        - |  9963 | ` *  $level` |
|        - |  9964 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9965 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9966 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9967 | ` *   levels will not always behave as expected.` |
|        - |  9968 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9969 | ` *   in the predefined constants.` |
|        - |  9970 | ` * Return` |
|        - |  9971 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9972 | ` *   parameter is given.` |
|        - |  9973 | ` */` |
|       38 |  9974 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9975 |  |
|       40 |  9976 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9977 | `	int nOld;` |
|        - |  9978 | `	/* Extract the old reporting level */` |
|       40 |  9979 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 |  9980 | `	if( nArg > 0 ){` |
|        - |  9981 | `		int nNew;` |
|        - |  9982 | `		/* Extract the desired error reporting level */` |
|       32 |  9983 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 |  9984 | `		if( !nNew ){` |
|        - |  9985 | `			/* Do not report errors at all */` |
|        5 |  9986 | `			pVm->bErrReport = 0;` |
|        3 |  9987 | `		}else{` |
|        - |  9988 | `			/* Report all errors */` |
|       28 |  9989 | `			pVm->bErrReport = 1;` |
|        - |  9990 | `		}` |
|       15 |  9991 | `	}` |
|        - |  9992 | `	/* Return the old level */` |
|       40 |  9993 | `	ph7_result_int(pCtx,nOld);` |
|       40 |  9994 | `	return PH7_OK;` |
|        2 |  9995 |  |
|        - |  9996 | `/*` |
|        - |  9997 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9998 | ` *  Send an error message somewhere.` |
|        - |  9999 | ` * Parameter` |
|        - | 10000 | ` *  $message` |
|        - | 10001 | ` *   The error message that should be logged.` |
|        - | 10002 | ` *  $message_type` |
|        - | 10003 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10004 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10005 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10006 | ` *       This is the default option.` |
|        - | 10007 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10008 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10009 | ` *    2  No longer an option.` |
|        - | 10010 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10011 | ` *       to the end of the message string.` |
|        - | 10012 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10013 | ` *  $destination` |
|        - | 10014 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10015 | ` *  $extra_headers` |
|        - | 10016 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10017 | ` * Return` |
|        - | 10018 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10019 | ` * NOTE:` |
|        - | 10020 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10021 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10022 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10023 | ` *  Otherwise this function is no-op.` |
|        - | 10024 | ` */` |
|        4 | 10025 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10026 |  |
|        - | 10027 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10028 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10029 | `	int iType = 0;` |
|        5 | 10030 | `	if( nArg < 1 ){` |
|        - | 10031 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10032 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10033 | `		return PH7_OK;` |
|        - | 10034 | `	}` |
|        5 | 10035 | `	if( pVm->xErrLog  ){` |
|        - | 10036 | `		/* Invoke the user callback */` |
|      ! 0 | 10037 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10038 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10039 | `		if( nArg > 1 ){` |
|      ! 0 | 10040 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10041 | `			if( nArg > 2 ){` |
|      ! 0 | 10042 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10043 | `				if( nArg > 3 ){` |
|      ! 0 | 10044 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10045 | `				}` |
|      ! 0 | 10046 | `			}` |
|      ! 0 | 10047 | `		}` |
|      ! 0 | 10048 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10049 | `	}` |
|        - | 10050 | `	/* Retun TRUE */` |
|        5 | 10051 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10052 | `	return PH7_OK;` |
|        3 | 10053 |  |
|        - | 10054 | `/*` |
|        - | 10055 | ` * bool restore_exception_handler(void)` |
|        - | 10056 | ` *  Restores the previously defined exception handler function.` |
|        - | 10057 | ` * Parameter` |
|        - | 10058 | ` *  None` |
|        - | 10059 | ` * Return` |
|        - | 10060 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10061 | ` */` |
|        4 | 10062 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10063 |  |
|        5 | 10064 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10065 | `	ph7_value *pOld,*pNew;` |
|        - | 10066 | `	/* Point to the old and the new handler */` |
|        5 | 10067 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10068 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10069 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10070 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10071 | `		SXUNUSED(apArg);` |
|        - | 10072 | `		/* No installed handler,return FALSE */` |
|        5 | 10073 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10074 | `		return PH7_OK;` |
|        - | 10075 | `	}` |
|        - | 10076 | `	/* Copy the old handler */` |
|      ! 0 | 10077 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10078 | `	PH7_MemObjRelease(pOld);` |
|        - | 10079 | `	/* Return TRUE */` |
|      ! 0 | 10080 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10081 | `	return PH7_OK;` |
|        3 | 10082 |  |
|        - | 10083 | `/*` |
|        - | 10084 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10085 | ` *  Sets a user-defined exception handler function.` |
|        - | 10086 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10087 | ` * NOTE` |
|        - | 10088 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10089 | ` *  the satndard PHP engine.` |
|        - | 10090 | ` * Parameters` |
|        - | 10091 | ` *  $exception_handler` |
|        - | 10092 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10093 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10094 | ` *   that was thrown.` |
|        - | 10095 | ` *  Note:` |
|        - | 10096 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10097 | ` * Return` |
|        - | 10098 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10099 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10100 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10101 | ` */` |
|        4 | 10102 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10103 |  |
|        6 | 10104 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10105 | `	ph7_value *pOld,*pNew;` |
|        - | 10106 | `	/* Point to the old and the new handler */` |
|        6 | 10107 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10108 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10109 | `	/* Return the old handler */` |
|        6 | 10110 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10111 | `	if( nArg > 0 ){` |
|        6 | 10112 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10113 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10114 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10115 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10116 | `		}else{` |
|        6 | 10117 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10118 | `			/* Install the new handler */` |
|        6 | 10119 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10120 | `		}` |
|        2 | 10121 | `	}` |
|        6 | 10122 | `	return PH7_OK;` |
|        2 | 10123 |  |
|        - | 10124 | `/*` |
|        - | 10125 | ` * bool restore_error_handler(void)` |
|        - | 10126 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10127 | ` * Parameters:` |
|        - | 10128 | ` *  None.` |
|        - | 10129 | ` * Return` |
|        - | 10130 | ` *  Always TRUE.` |
|        - | 10131 | ` */` |
|        4 | 10132 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10133 |  |
|        5 | 10134 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10135 | `	ph7_value *pOld,*pNew;` |
|        - | 10136 | `	/* Point to the old and the new handler */` |
|        5 | 10137 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10138 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10139 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10140 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10141 | `		SXUNUSED(apArg);` |
|        - | 10142 | `		/* No installed callback,return FALSE */` |
|        5 | 10143 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10144 | `		return PH7_OK;` |
|        - | 10145 | `	}` |
|        - | 10146 | `	/* Copy the old callback */` |
|      ! 0 | 10147 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10148 | `	PH7_MemObjRelease(pOld);` |
|        - | 10149 | `	/* Return TRUE */` |
|      ! 0 | 10150 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10151 | `	return PH7_OK;` |
|        3 | 10152 |  |
|        - | 10153 | `/*` |
|        - | 10154 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10155 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10156 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10157 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10158 | ` *  Sets a user-defined error handler function.` |
|        - | 10159 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10160 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10161 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10162 | ` *  conditions (using trigger_error()).` |
|        - | 10163 | ` * Parameters` |
|        - | 10164 | ` *  $error_handler` |
|        - | 10165 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10166 | ` *   describing the error.` |
|        - | 10167 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10168 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10169 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10170 | ` *   The function can be shown as:` |
|        - | 10171 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10172 | ` *     errno` |
|        - | 10173 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10174 | ` *   errstr` |
|        - | 10175 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10176 | ` *   errfile` |
|        - | 10177 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10178 | ` *     was raised in, as a string.` |
|        - | 10179 | ` *  Note:` |
|        - | 10180 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10181 | ` * Return` |
|        - | 10182 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10183 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10184 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10185 | ` */` |
|     9250 | 10186 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10187 |  |
|     9252 | 10188 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10189 | `	ph7_value *pOld,*pNew;` |
|        - | 10190 | `	/* Point to the old and the new handler */` |
|     9252 | 10191 | `	pOld = &pVm->aErrCB[0];` |
|     9252 | 10192 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10193 | `	/* Return the old handler */` |
|     9252 | 10194 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9252 | 10195 | `	if( nArg > 0 ){` |
|     9252 | 10196 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10197 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4625 | 10198 | `			PH7_MemObjRelease(pNew);` |
|     4625 | 10199 | `			ph7_result_bool(pCtx,1);` |
|     2313 | 10200 | `		}else{` |
|     4628 | 10201 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10202 | `			/* Install the new handler */` |
|     4628 | 10203 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10204 | `		}` |
|     4625 | 10205 | `	}` |
|     9252 | 10206 | `	return PH7_OK;` |
|        2 | 10207 |  |
|        - | 10208 | `/*` |
|        - | 10209 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10210 | ` *  Generates a backtrace.` |
|        - | 10211 | ` * Paramaeter` |
|        - | 10212 | ` *  $options` |
|        - | 10213 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10214 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10215 | ` *   all the function/method arguments, to save memory.` |
|        - | 10216 | ` * $limit` |
|        - | 10217 | ` *   (Not Used)` |
|        - | 10218 | ` * Return` |
|        - | 10219 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10220 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10221 | ` *          Name        Type      Description` |
|        - | 10222 | ` *          ------      ------     -----------` |
|        - | 10223 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10224 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10225 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10226 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10227 | ` *          object      object    The current object.` |
|        - | 10228 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10229 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10230 | ` */` |
|      514 | 10231 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10232 |  |
|      516 | 10233 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10234 | `	ph7_value *pArray;` |
|        - | 10235 | `	ph7_class *pClass;` |
|        - | 10236 | `	ph7_value *pValue;` |
|        - | 10237 | `	SyString *pFile;` |
|        - | 10238 | `	/* Create a new array */` |
|      516 | 10239 | `	pArray = ph7_context_new_array(pCtx);` |
|      516 | 10240 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      516 | 10241 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10242 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10243 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10244 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10245 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10246 | `		SXUNUSED(apArg);` |
|      ! 0 | 10247 | `		return PH7_OK;` |
|        - | 10248 | `	}` |
|        - | 10249 | `	/* Dump running function name and it's arguments  */` |
|      516 | 10250 | `	if( pVm->pFrame->pParent ){` |
|      516 | 10251 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10252 | `		ph7_vm_func *pFunc;` |
|        - | 10253 | `		ph7_value *pArg;` |
|      516 | 10254 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      516 | 10255 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      516 | 10256 | `		if( pFrame->pParent && pFunc ){` |
|      516 | 10257 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      516 | 10258 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      516 | 10259 | `			ph7_value_reset_string_cursor(pValue);` |
|      257 | 10260 | `		}` |
|        - | 10261 | `		/* Function arguments */` |
|      516 | 10262 | `		pArg = ph7_context_new_array(pCtx);` |
|      516 | 10263 | `		if( pArg  ){` |
|        - | 10264 | `			ph7_value *pObj;` |
|        - | 10265 | `			VmSlot *aSlot;` |
|        - | 10266 | `			sxu32 n;` |
|        - | 10267 | `			/* Start filling the array with the given arguments */` |
|      516 | 10268 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2050 | 10269 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1536 | 10270 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1536 | 10271 | `				if( pObj ){` |
|     1536 | 10272 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      767 | 10273 | `				}` |
|      769 | 10274 | `			}` |
|        - | 10275 | `			/* Save the array */` |
|      516 | 10276 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      257 | 10277 | `		}` |
|      257 | 10278 | `	}` |
|      516 | 10279 | `	ph7_value_int(pValue,1);` |
|        - | 10280 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10281 | `	 * line numbers at run-time. )` |
|        - | 10282 | `	 */` |
|      516 | 10283 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10284 | `	/* Current processed script */` |
|      516 | 10285 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      516 | 10286 | `	if( pFile ){` |
|      516 | 10287 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      516 | 10288 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      516 | 10289 | `		ph7_value_reset_string_cursor(pValue);` |
|      257 | 10290 | `	}` |
|        - | 10291 | `	/* Top class */` |
|      516 | 10292 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      516 | 10293 | `	if( pClass ){` |
|      512 | 10294 | `		ph7_value_reset_string_cursor(pValue);` |
|      512 | 10295 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      512 | 10296 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      255 | 10297 | `	}` |
|        - | 10298 | `	/* Return the freshly created array */` |
|      516 | 10299 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10300 | `	/*` |
|        - | 10301 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10302 | `	 * as soon we return from this function.` |
|        - | 10303 | `	 */` |
|      516 | 10304 | `	return PH7_OK;` |
|      259 | 10305 |  |
|        - | 10306 | `/*` |
|        - | 10307 | ` * Generate a small backtrace.` |
|        - | 10308 | ` * Store the generated dump in the given BLOB` |
|        - | 10309 | ` */` |
|        4 | 10310 | `static int VmMiniBacktrace(` |
|        - | 10311 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10312 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10313 | `	)` |
|        1 | 10314 |  |
|        5 | 10315 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10316 | `	ph7_vm_func *pFunc;` |
|        - | 10317 | `	ph7_class *pClass;` |
|        - | 10318 | `	SyString *pFile;` |
|        - | 10319 | `	/* Called function */` |
|        5 | 10320 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10321 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10322 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10323 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10324 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10325 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10326 | `	}else{` |
|      ! 0 | 10327 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10328 | `	}` |
|        5 | 10329 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10330 | `	/* Current processed script */` |
|        5 | 10331 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10332 | `	if( pFile ){` |
|        5 | 10333 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10334 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10335 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10336 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10337 | `	}` |
|        - | 10338 | `	/* Top class */` |
|        5 | 10339 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10340 | `	if( pClass ){` |
|      ! 0 | 10341 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10342 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10343 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10344 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10345 | `	}` |
|        5 | 10346 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10347 | `	/* All done */` |
|        5 | 10348 | `	return SXRET_OK;` |
|        1 | 10349 |  |
|        - | 10350 | `/*` |
|        - | 10351 | ` * void debug_print_backtrace()` |
|        - | 10352 | ` *  Prints a backtrace` |
|        - | 10353 | ` * Parameters` |
|        - | 10354 | ` * None` |
|        - | 10355 | ` * Return` |
|        - | 10356 | ` * NULL` |
|        - | 10357 | ` */` |
|        2 | 10358 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10359 |  |
|        3 | 10360 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10361 | `	SyBlob sDump;` |
|        3 | 10362 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10363 | `	/* Generate the backtrace */` |
|        3 | 10364 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10365 | `	/* Output backtrace */` |
|        3 | 10366 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10367 | `	/* All done,cleanup */` |
|        3 | 10368 | `	SyBlobRelease(&sDump);` |
|        1 | 10369 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10370 | `	SXUNUSED(apArg);` |
|        3 | 10371 | `	return PH7_OK;` |
|        1 | 10372 |  |
|        - | 10373 | `/*` |
|        - | 10374 | ` * string debug_string_backtrace()` |
|        - | 10375 | ` *  Generate a backtrace` |
|        - | 10376 | ` * Parameters` |
|        - | 10377 | ` * None` |
|        - | 10378 | ` * Return` |
|        - | 10379 | ` *  A mini backtrace().` |
|        - | 10380 | ` * Note that this is a symisc extension.` |
|        - | 10381 | ` */` |
|        2 | 10382 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10383 |  |
|        3 | 10384 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10385 | `	SyBlob sDump;` |
|        3 | 10386 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10387 | `	/* Generate the backtrace */` |
|        3 | 10388 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10389 | `	/* Return the backtrace */` |
|        3 | 10390 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10391 | `	/* All done,cleanup */` |
|        3 | 10392 | `	SyBlobRelease(&sDump);` |
|        1 | 10393 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10394 | `	SXUNUSED(apArg);` |
|        3 | 10395 | `	return PH7_OK;` |
|        1 | 10396 |  |
|        - | 10397 | `/*` |
|        - | 10398 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10399 | ` * exception is triggered.` |
|        - | 10400 | ` */` |
|      472 | 10401 | `static sxi32 VmUncaughtException(` |
|        - | 10402 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10403 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10404 | `	)` |
|        1 | 10405 |  |
|        - | 10406 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10407 | `	int nArg = 1;` |
|        - | 10408 | `	sxi32 rc;` |
|      473 | 10409 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10410 | `		/* Nesting limit reached */` |
|      ! 0 | 10411 | `		return SXRET_OK;` |
|        - | 10412 | `	}` |
|        - | 10413 | `	/* Call any exception handler if available */` |
|      473 | 10414 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10415 | `	if( pThis ){` |
|        - | 10416 | `		/* Load the exception instance */` |
|      473 | 10417 | `		sArg.x.pOther = pThis;` |
|      473 | 10418 | `		pThis->iRef++;` |
|      473 | 10419 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10420 | `	}else{` |
|      ! 0 | 10421 | `		nArg = 0;` |
|        - | 10422 | `	}` |
|      473 | 10423 | `	apArg[0] = &sArg;` |
|        - | 10424 | `	/* Call the exception handler if available */` |
|      473 | 10425 | `	pVm->nExceptDepth++;` |
|      473 | 10426 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10427 | `	pVm->nExceptDepth--;` |
|      473 | 10428 | `	if( rc != SXRET_OK ){` |
|        - | 10429 | `		SyBlob sMsgBuf;` |
|      471 | 10430 | `		const char *zClass = "Exception";` |
|      471 | 10431 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10432 | `		const char *zMsg;` |
|        - | 10433 | `		sxu32 nMsg;` |
|        - | 10434 | `		const char *zFuncName;` |
|        - | 10435 | `		int nFuncLen;` |
|      471 | 10436 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10437 | `		if( pThis ){` |
|        - | 10438 | `			ph7_class_method *pGetMessage;` |
|        - | 10439 | `			ph7_value sMsg;` |
|        - | 10440 | `			const char *zTmp;` |
|        - | 10441 | `			int nTmp;` |
|      471 | 10442 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10443 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10444 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10445 | `			if( pGetMessage ){` |
|      471 | 10446 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10447 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10448 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10449 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10450 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10451 | `					}` |
|      235 | 10452 | `				}` |
|      471 | 10453 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10454 | `			}` |
|      235 | 10455 | `		}` |
|      471 | 10456 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10457 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10458 | `		}` |
|      471 | 10459 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10460 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10461 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10462 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10463 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10464 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10465 | `		rc = SXERR_ABORT;` |
|      235 | 10466 | `	}` |
|      473 | 10467 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10468 | `	return rc;` |
|      237 | 10469 |  |
|        - | 10470 | `/*` |
|        - | 10471 | ` * Throw a user exception.` |
|        - | 10472 | ` *` |
|        - | 10473 | ` * Exception dispatch follows this sequence:` |
|        - | 10474 | ` *` |
|        - | 10475 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10476 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10477 | ` *` |
|        - | 10478 | ` * 2. If NO catch matches:` |
|        - | 10479 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10480 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10481 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10482 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10483 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10484 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10485 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10486 | ` *` |
|        - | 10487 | ` * 3. If a catch DOES match:` |
|        - | 10488 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10489 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10490 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10491 | ` *       finally block.` |
|        - | 10492 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10493 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10494 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10495 | ` *       in pPendingException (step 2c).` |
|        - | 10496 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10497 | ` *    d. Run finally (if present).` |
|        - | 10498 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10499 | ` *       that handlers are restored and finally has run.` |
|        - | 10500 | ` */` |
|      514 | 10501 | `static sxi32 VmThrowException(` |
|        - | 10502 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10503 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10504 | `	)` |
|        2 | 10505 |  |
|        - | 10506 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10507 | `	ph7_exception **apException;` |
|        - | 10508 | `	ph7_exception *pException;` |
|        - | 10509 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10510 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10511 | `	pException = 0;` |
|      516 | 10512 | `	pCatch = 0;` |
|      516 | 10513 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10514 | `		ph7_exception_block *aCatch;` |
|        - | 10515 | `		ph7_class *pClass;` |
|        - | 10516 | `		sxu32 j;` |
|        - | 10517 | `		/* Locate the appropriate block to execute */` |
|       40 | 10518 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10519 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10520 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10521 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10522 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10523 | `			/* Extract the target class */` |
|       38 | 10524 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10525 | `			if( pClass == 0 ){` |
|        - | 10526 | `				/* No such class */` |
|      ! 0 | 10527 | `				continue;` |
|        - | 10528 | `			}` |
|       38 | 10529 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10530 | `				/* Catch block found,break immeditaley */` |
|       38 | 10531 | `				pCatch = &aCatch[j];` |
|       38 | 10532 | `				break;` |
|        - | 10533 | `			}` |
|      ! 0 | 10534 | `		}` |
|       19 | 10535 | `	}` |
|        - | 10536 | `	/* Execute the cached block if available */` |
|      516 | 10537 | `	if( pCatch == 0 ){` |
|        - | 10538 | `		sxi32 rc;` |
|        - | 10539 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10540 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10541 | `			pException->iFinallyDone = 1;` |
|        3 | 10542 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10543 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10544 | `				return SXERR_ABORT;` |
|        - | 10545 | `			}` |
|        1 | 10546 | `		}` |
|        - | 10547 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10548 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10549 | `			/* Re-throw to the outer handler */` |
|        3 | 10550 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10551 | `		}` |
|        - | 10552 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10553 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10554 | `		 * exception instead of reporting it uncaught.` |
|        - | 10555 | `		 */` |
|      478 | 10556 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10557 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10558 | `			 * by looking for a catch frame on the stack.` |
|        - | 10559 | `			 */` |
|      478 | 10560 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10561 | `			int inCatch = 0;` |
|      956 | 10562 | `			while( pF ){` |
|      484 | 10563 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        5 | 10564 | `					inCatch = 1;` |
|        5 | 10565 | `					break;` |
|        - | 10566 | `				}` |
|      479 | 10567 | `				pF = pF->pParent;` |
|        1 | 10568 | `			}` |
|      478 | 10569 | `			if( inCatch ){` |
|        - | 10570 | `				/* Defer — will be re-thrown after finally runs */` |
|        5 | 10571 | `				pThis->iRef++;` |
|        5 | 10572 | `				pVm->pPendingException = pThis;` |
|        5 | 10573 | `				return SXRET_OK;` |
|        - | 10574 | `			}` |
|      236 | 10575 | `		}` |
|        - | 10576 | `		/* Truly uncaught */` |
|      473 | 10577 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10578 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10579 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10580 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10581 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10582 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10583 | `			}` |
|      ! 0 | 10584 | `		}` |
|      473 | 10585 | `		return rc;` |
|      ! 0 | 10586 | `	}else{` |
|       38 | 10587 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10588 | `		ph7_exception **apSaved = 0;` |
|        - | 10589 | `		sxu32 nSavedCount;` |
|        - | 10590 | `		sxi32 rc;` |
|       38 | 10591 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10592 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10593 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10594 | `		}` |
|        - | 10595 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10596 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10597 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10598 | `		 */` |
|       38 | 10599 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10600 | `		if( nSavedCount > 0 ){` |
|       10 | 10601 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10602 | `				nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10603 | `			if( apSaved ){` |
|       10 | 10604 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10605 | `					nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10606 | `				SySetReset(&pVm->aException);` |
|        3 | 10607 | `			}` |
|        3 | 10608 | `		}` |
|        - | 10609 | `		/* Create a private frame first */` |
|       38 | 10610 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10611 | `		if( rc == SXRET_OK ){` |
|       38 | 10612 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10613 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10614 | `			if( pObj ){` |
|       38 | 10615 | `				pThis->iRef++;` |
|       38 | 10616 | `				pObj->x.pOther = pThis;` |
|       38 | 10617 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10618 | `			}` |
|        - | 10619 | `			/* Execute the catch block */` |
|       38 | 10620 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10621 | `			/* Leave the frame */` |
|       38 | 10622 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10623 | `		}` |
|        - | 10624 | `		/* Restore the outer exception handlers */` |
|       38 | 10625 | `		if( apSaved ){` |
|        - | 10626 | `			sxu32 k;` |
|        - | 10627 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10628 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10629 | `			 * Restore the original outer entries.` |
|        - | 10630 | `			 */` |
|        7 | 10631 | `			SySetReset(&pVm->aException);` |
|       13 | 10632 | `			for(k = 0; k < nSavedCount; k++){` |
|        7 | 10633 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        4 | 10634 | `			}` |
|        7 | 10635 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10636 | `		}` |
|        - | 10637 | `		/* Execute the finally block after catch */` |
|       38 | 10638 | `		if( pException->iHasFinally ){` |
|       12 | 10639 | `			pException->iFinallyDone = 1;` |
|        - | 10640 | `			{` |
|       12 | 10641 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       12 | 10642 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10643 | `					return SXERR_ABORT;` |
|        - | 10644 | `				}` |
|        - | 10645 | `			}` |
|        5 | 10646 | `		}` |
|       38 | 10647 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10648 | `			return SXERR_ABORT;` |
|        - | 10649 | `		}` |
|        - | 10650 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10651 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10652 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10653 | `		 */` |
|       38 | 10654 | `		if( pVm->pPendingException ){` |
|        5 | 10655 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        5 | 10656 | `			pVm->pPendingException = 0;` |
|        5 | 10657 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10658 | `		}` |
|        - | 10659 | `	}` |
|        - | 10660 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10661 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10662 | `	 */` |
|       34 | 10663 | `	return SXRET_OK;` |
|      259 | 10664 |  |
|        - | 10665 | `/*` |
|        - | 10666 | ` * Section:` |
|        - | 10667 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10668 | ` * Status:` |
|        - | 10669 | ` *    Stable.` |
|        - | 10670 | ` */` |
|        - | 10671 | `/*` |
|        - | 10672 | ` * string ph7version(void)` |
|        - | 10673 | ` *  Returns the running version of the PH7 version.` |
|        - | 10674 | ` * Parameters` |
|        - | 10675 | ` *  None` |
|        - | 10676 | ` * Return` |
|        - | 10677 | ` * Current PH7 version.` |
|        - | 10678 | ` */` |
|        2 | 10679 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10680 |  |
|        1 | 10681 | `	SXUNUSED(nArg);` |
|        1 | 10682 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10683 | `	/* Current engine version */` |
|        3 | 10684 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10685 | `	return PH7_OK;` |
|        1 | 10686 |  |
|        - | 10687 | `/*` |
|        - | 10688 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10689 | ` */` |
|        - | 10690 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10691 | ` "<html><head>"\` |
|        - | 10692 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10693 | ` "<style type=\"text/css\">"\` |
|        - | 10694 | ` "div {"\` |
|        - | 10695 | `     "border: 1px solid #cccccc;"\` |
|        - | 10696 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10697 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10698 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10699 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10700 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10701 | `     "-o-border-radius: 10px;"\` |
|        - | 10702 | `     "border-radius: 10px;"\` |
|        - | 10703 | `     "padding-left: 2em;"\` |
|        - | 10704 | `     "background-color: white;"\` |
|        - | 10705 | `     "margin-left: auto;"\` |
|        - | 10706 | `     "font-family: verdana;"\` |
|        - | 10707 | `     "padding-right: 2em;"\` |
|        - | 10708 | `     "margin-right: auto;"\` |
|        - | 10709 | `     "}"\` |
|        - | 10710 | `     "body {"\` |
|        - | 10711 | `     "padding: 0.2em;"\` |
|        - | 10712 | `     "font-style: normal;"\` |
|        - | 10713 | `     "font-size: medium;"\` |
|        - | 10714 | `     "background-color: #f2f2f2;"\` |
|        - | 10715 | `     "}"\` |
|        - | 10716 | `     "hr {"\` |
|        - | 10717 | `     "border-style: solid none none;"\` |
|        - | 10718 | `     "border-width: 1px medium medium;"\` |
|        - | 10719 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10720 | `     "height: 1px;"\` |
|        - | 10721 | `     "}"\` |
|        - | 10722 | `     "a {"\` |
|        - | 10723 | `     "color: #3366cc;"\` |
|        - | 10724 | `     "text-decoration: none;"\` |
|        - | 10725 | `     "}"\` |
|        - | 10726 | `     "a:hover {"\` |
|        - | 10727 | `     "color: #999999;"\` |
|        - | 10728 | `     "}"\` |
|        - | 10729 | `     "a:active {"\` |
|        - | 10730 | `     "color: #663399;"\` |
|        - | 10731 | `     "}"\` |
|        - | 10732 | `     "h1 {"\` |
|        - | 10733 | `     "margin: 0;"\` |
|        - | 10734 | `     "padding: 0;"\` |
|        - | 10735 | `     "font-family: Verdana;"\` |
|        - | 10736 | `     "font-weight: bold;"\` |
|        - | 10737 | `     "font-style: normal;"\` |
|        - | 10738 | `     "font-size: medium;"\` |
|        - | 10739 | `     "text-transform: capitalize;"\` |
|        - | 10740 | `     "color: #0a328c;"\` |
|        - | 10741 | `     "}"\` |
|        - | 10742 | `     "p {"\` |
|        - | 10743 | `     "margin: 0 auto;"\` |
|        - | 10744 | `     "font-size: medium;"\` |
|        - | 10745 | `     "font-style: normal;"\` |
|        - | 10746 | `     "font-family: verdana;"\` |
|        - | 10747 | `     "}"\` |
|        - | 10748 | `"</style></head><body>"\` |
|        - | 10749 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10750 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10751 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10752 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10753 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10754 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10755 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10756 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10757 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10758 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10759 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10760 |  |
|        - | 10761 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10762 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10763 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10764 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10765 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10766 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10767 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10768 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10769 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10770 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10771 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10772 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10773 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10774 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10775 |  |
|        - | 10776 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10777 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10778 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10779 | `"&nbsp;*<br>"\` |
|        - | 10780 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10781 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10782 | `"&nbsp;* are met:<br>"\` |
|        - | 10783 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10784 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10785 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10786 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10787 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10788 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10789 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10790 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10791 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10792 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10793 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10794 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10795 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10796 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10797 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10798 | `"&nbsp;*<br>"\` |
|        - | 10799 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10800 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10801 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10802 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10803 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10804 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10805 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10806 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10807 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10808 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10809 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10810 | `"&nbsp;*/<br>"\` |
|        - | 10811 | `"</span></small></small></p>"\` |
|        - | 10812 | `"</div></body></html>"` |
|        - | 10813 | `/*` |
|        - | 10814 | ` * bool ph7credits(void)` |
|        - | 10815 | ` * bool ph7info(void)` |
|        - | 10816 | ` * bool ph7copyright(void)` |
|        - | 10817 | ` *  Prints out the credits for PH7 engine` |
|        - | 10818 | ` * Parameters` |
|        - | 10819 | ` *  None` |
|        - | 10820 | ` * Return` |
|        - | 10821 | ` *  Always TRUE` |
|        - | 10822 | ` */` |
|        2 | 10823 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10824 |  |
|        3 | 10825 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10826 | `	/* Expand the HTML page above*/` |
|        3 | 10827 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10828 | `	ph7_context_output_format(` |
|        1 | 10829 | `		pCtx,` |
|        - | 10830 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10831 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10832 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10833 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10834 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10835 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10836 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10837 | `#ifdef __WINNT__` |
|        - | 10838 | `		"Windows NT"` |
|        - | 10839 | `#elif defined(__UNIXES__)` |
|        - | 10840 | `		"UNIX-Like"` |
|        - | 10841 | `#else` |
|        - | 10842 | `		"Other OS"` |
|        - | 10843 | `#endif` |
|        - | 10844 | `		);` |
|        3 | 10845 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10846 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10847 | `	SXUNUSED(apArg);` |
|        - | 10848 | `	/* Return TRUE */` |
|        - | 10849 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10850 | `	return PH7_OK;` |
|        1 | 10851 |  |
|        - | 10852 | `/*` |
|        - | 10853 | ` * Section:` |
|        - | 10854 | ` *    URL related routines.` |
|        - | 10855 | ` * Status:` |
|        - | 10856 | ` *    Stable.` |
|        - | 10857 | ` */` |
|        - | 10858 | `/*` |
|        - | 10859 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10860 | ` *  Parse a URL and return its fields.` |
|        - | 10861 | ` * Parameters` |
|        - | 10862 | ` *  $url` |
|        - | 10863 | ` *   The URL to parse.` |
|        - | 10864 | ` * $component` |
|        - | 10865 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10866 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10867 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10868 | ` *  in which case the return value will be an integer).` |
|        - | 10869 | ` * Return` |
|        - | 10870 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10871 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10872 | ` *  this array are:` |
|        - | 10873 | ` *   scheme - e.g. http` |
|        - | 10874 | ` *   host` |
|        - | 10875 | ` *   port` |
|        - | 10876 | ` *   user` |
|        - | 10877 | ` *   pass` |
|        - | 10878 | ` *   path` |
|        - | 10879 | ` *   query - after the question mark ?` |
|        - | 10880 | ` *   fragment - after the hashmark #` |
|        - | 10881 | ` * Note:` |
|        - | 10882 | ` *  FALSE is returned on failure.` |
|        - | 10883 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10884 | ` *  with the standard PHP engine.` |
|        - | 10885 | ` */` |
|       28 | 10886 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10887 |  |
|        - | 10888 | `	const char *zStr; /* Input string */` |
|        - | 10889 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10890 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10891 | `	int nLen;` |
|        - | 10892 | `	sxi32 rc;` |
|       29 | 10893 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10894 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10895 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10896 | `		return PH7_OK;` |
|        - | 10897 | `	}` |
|        - | 10898 | `	/* Extract the given URI */` |
|       29 | 10899 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10900 | `	if( nLen < 1 ){` |
|        - | 10901 | `		/* Nothing to process,return FALSE */` |
|        3 | 10902 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10903 | `		return PH7_OK;` |
|        - | 10904 | `	}` |
|        - | 10905 | `	/* Get a parse */` |
|       27 | 10906 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10907 | `	if( rc != SXRET_OK ){` |
|        - | 10908 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10910 | `		return PH7_OK;` |
|        - | 10911 | `	}` |
|       27 | 10912 | `	if( nArg > 1 ){` |
|      ! 0 | 10913 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10914 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10915 | `		switch(nComponent){` |
|      ! 0 | 10916 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10917 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10918 | `			if( pComp->nByte < 1 ){` |
|        - | 10919 | `				/* No available value,return NULL */` |
|      ! 0 | 10920 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10921 | `			}else{` |
|      ! 0 | 10922 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10923 | `			}` |
|      ! 0 | 10924 | `			break;` |
|      ! 0 | 10925 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10926 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10927 | `			if( pComp->nByte < 1 ){` |
|        - | 10928 | `				/* No available value,return NULL */` |
|      ! 0 | 10929 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10930 | `			}else{` |
|      ! 0 | 10931 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10932 | `			}` |
|      ! 0 | 10933 | `			break;` |
|      ! 0 | 10934 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10935 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10936 | `			if( pComp->nByte < 1 ){` |
|        - | 10937 | `				/* No available value,return NULL */` |
|      ! 0 | 10938 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10939 | `			}else{` |
|      ! 0 | 10940 | `				int iPort = 0;` |
|        - | 10941 | `				/* Cast the value to integer */` |
|      ! 0 | 10942 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10943 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10944 | `			}` |
|      ! 0 | 10945 | `			break;` |
|      ! 0 | 10946 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10947 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10948 | `			if( pComp->nByte < 1 ){` |
|        - | 10949 | `				/* No available value,return NULL */` |
|      ! 0 | 10950 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10951 | `			}else{` |
|      ! 0 | 10952 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10953 | `			}` |
|      ! 0 | 10954 | `			break;` |
|      ! 0 | 10955 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10956 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10957 | `			if( pComp->nByte < 1 ){` |
|        - | 10958 | `				/* No available value,return NULL */` |
|      ! 0 | 10959 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10960 | `			}else{` |
|      ! 0 | 10961 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10962 | `			}` |
|      ! 0 | 10963 | `			break;` |
|      ! 0 | 10964 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10965 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10966 | `			if( pComp->nByte < 1 ){` |
|        - | 10967 | `				/* No available value,return NULL */` |
|      ! 0 | 10968 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10969 | `			}else{` |
|      ! 0 | 10970 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10971 | `			}` |
|      ! 0 | 10972 | `			break;` |
|      ! 0 | 10973 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10974 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10975 | `			if( pComp->nByte < 1 ){` |
|        - | 10976 | `				/* No available value,return NULL */` |
|      ! 0 | 10977 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10978 | `			}else{` |
|      ! 0 | 10979 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10980 | `			}` |
|      ! 0 | 10981 | `			break;` |
|      ! 0 | 10982 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10983 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10984 | `			if( pComp->nByte < 1 ){` |
|        - | 10985 | `				/* No available value,return NULL */` |
|      ! 0 | 10986 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10987 | `			}else{` |
|      ! 0 | 10988 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10989 | `			}` |
|      ! 0 | 10990 | `			break;` |
|      ! 0 | 10991 | `		default:` |
|        - | 10992 | `			/* No such entry,return NULL */` |
|      ! 0 | 10993 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10994 | `			break;` |
|        - | 10995 | `		}` |
|      ! 0 | 10996 | `	}else{` |
|        - | 10997 | `		ph7_value *pArray,*pValue;` |
|        - | 10998 | `		/* Return an associative array */` |
|       27 | 10999 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11000 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11001 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11002 | `			/* Out of memory */` |
|      ! 0 | 11003 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11004 | `			/* Return false */` |
|      ! 0 | 11005 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11006 | `			return PH7_OK;` |
|        - | 11007 | `		}` |
|        - | 11008 | `		/* Fill the array */` |
|       27 | 11009 | `		pComp = &sURI.sScheme;` |
|       27 | 11010 | `		if( pComp->nByte > 0 ){` |
|       19 | 11011 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11012 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11013 | `		}` |
|        - | 11014 | `		/* Reset the string cursor */` |
|       27 | 11015 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11016 | `		pComp = &sURI.sHost;` |
|       27 | 11017 | `		if( pComp->nByte > 0 ){` |
|       25 | 11018 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11019 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11020 | `		}` |
|        - | 11021 | `		/* Reset the string cursor */` |
|       27 | 11022 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11023 | `		pComp = &sURI.sPort;` |
|       27 | 11024 | `		if( pComp->nByte > 0 ){` |
|       11 | 11025 | `			int iPort = 0;/* cc warning */` |
|        - | 11026 | `			/* Convert to integer */` |
|       11 | 11027 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11028 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11029 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11030 | `		}` |
|        - | 11031 | `		/* Reset the string cursor */` |
|       27 | 11032 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11033 | `		pComp = &sURI.sUser;` |
|       27 | 11034 | `		if( pComp->nByte > 0 ){` |
|        7 | 11035 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11036 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11037 | `		}` |
|        - | 11038 | `		/* Reset the string cursor */` |
|       27 | 11039 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11040 | `		pComp = &sURI.sPass;` |
|       27 | 11041 | `		if( pComp->nByte > 0 ){` |
|        7 | 11042 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11043 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11044 | `		}` |
|        - | 11045 | `		/* Reset the string cursor */` |
|       27 | 11046 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11047 | `		pComp = &sURI.sPath;` |
|       27 | 11048 | `		if( pComp->nByte > 0 ){` |
|       17 | 11049 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11050 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11051 | `		}` |
|        - | 11052 | `		/* Reset the string cursor */` |
|       27 | 11053 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11054 | `		pComp = &sURI.sQuery;` |
|       27 | 11055 | `		if( pComp->nByte > 0 ){` |
|        5 | 11056 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11057 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11058 | `		}` |
|        - | 11059 | `		/* Reset the string cursor */` |
|       27 | 11060 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11061 | `		pComp = &sURI.sFragment;` |
|       27 | 11062 | `		if( pComp->nByte > 0 ){` |
|        5 | 11063 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11064 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11065 | `		}` |
|        - | 11066 | `		/* Return the created array */` |
|       27 | 11067 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11068 | `		/* NOTE:` |
|        - | 11069 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11070 | `		 * automatically as soon we return from this function.` |
|        - | 11071 | `		 */` |
|        - | 11072 | `	}` |
|        - | 11073 | `	/* All done */` |
|       27 | 11074 | `	return PH7_OK;` |
|       15 | 11075 |  |
|        - | 11076 | `/*` |
|        - | 11077 | ` * Section:` |
|        - | 11078 | ` *   Array related routines.` |
|        - | 11079 | ` * Status:` |
|        - | 11080 | ` *    Stable.` |
|        - | 11081 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11082 | ` *  Array related functions that need access to the underlying` |
|        - | 11083 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11084 | ` */` |
|        - | 11085 | `/*` |
|        - | 11086 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11087 | ` * of the following structure.` |
|        - | 11088 | ` */` |
|        - | 11089 | `struct compact_data` |
|        - | 11090 |  |
|        - | 11091 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11092 | `	int nRecCount;      /* Recursion count */` |
|        - | 11093 | `};` |
|        - | 11094 | `/*` |
|        - | 11095 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11096 | ` */` |
|      ! 0 | 11097 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11098 |  |
|      ! 0 | 11099 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11100 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11101 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11102 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11103 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11104 | `		SyString sVar;` |
|      ! 0 | 11105 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11106 | `		if( sVar.nByte > 0 ){` |
|        - | 11107 | `			/* Query the current frame */` |
|      ! 0 | 11108 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11109 | `			/* ^` |
|        - | 11110 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11111 | `			 */` |
|      ! 0 | 11112 | `			if( pKey ){` |
|        - | 11113 | `				/* Perform the insertion */` |
|      ! 0 | 11114 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11115 | `			}` |
|      ! 0 | 11116 | `		}` |
|      ! 0 | 11117 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11118 | `		int rc;` |
|        - | 11119 | `		/* Recursively traverse this array */` |
|      ! 0 | 11120 | `		pData->nRecCount++;` |
|      ! 0 | 11121 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11122 | `		pData->nRecCount--;` |
|      ! 0 | 11123 | `		return rc;` |
|        - | 11124 | `	}` |
|      ! 0 | 11125 | `	return SXRET_OK;` |
|      ! 0 | 11126 |  |
|        - | 11127 | `/*` |
|        - | 11128 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11129 | ` *  Create array containing variables and their values.` |
|        - | 11130 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11131 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11132 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11133 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11134 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11135 | ` * Parameters` |
|        - | 11136 | ` *  $varname` |
|        - | 11137 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11138 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11139 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11140 | ` *   it recursively.` |
|        - | 11141 | ` * Return` |
|        - | 11142 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11143 | ` */` |
|        2 | 11144 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11145 |  |
|        - | 11146 | `	ph7_value *pArray,*pObj;` |
|        3 | 11147 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11148 | `	const char *zName;` |
|        - | 11149 | `	SyString sVar;` |
|        - | 11150 | `	int i,nLen;` |
|        3 | 11151 | `	if( nArg < 1 ){` |
|        - | 11152 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11153 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11154 | `		return PH7_OK;` |
|        - | 11155 | `	}` |
|        - | 11156 | `	/* Create the array */` |
|        3 | 11157 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11158 | `	if( pArray == 0 ){` |
|        - | 11159 | `		/* Out of memory */` |
|      ! 0 | 11160 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11161 | `		/* Return NULL */` |
|      ! 0 | 11162 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11163 | `		return PH7_OK;` |
|        - | 11164 | `	}` |
|        - | 11165 | `	/* Perform the requested operation */` |
|        7 | 11166 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11167 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11168 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11169 | `				struct compact_data sData;` |
|      ! 0 | 11170 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11171 | `				/* Recursively walk the array */` |
|      ! 0 | 11172 | `				sData.nRecCount = 0;` |
|      ! 0 | 11173 | `				sData.pArray = pArray;` |
|      ! 0 | 11174 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11175 | `			}` |
|      ! 0 | 11176 | `		}else{` |
|        - | 11177 | `			/* Extract variable name */` |
|        5 | 11178 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11179 | `			if( nLen > 0 ){` |
|        5 | 11180 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11181 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11182 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11183 | `				if( pObj ){` |
|        5 | 11184 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11185 | `				}` |
|        2 | 11186 | `			}` |
|        - | 11187 | `		}` |
|        3 | 11188 | `	}` |
|        - | 11189 | `	/* Return the array */` |
|        3 | 11190 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11191 | `	return PH7_OK;` |
|        2 | 11192 |  |
|        - | 11193 | `/*` |
|        - | 11194 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11195 | ` * of the following structure.` |
|        - | 11196 | ` */` |
|        - | 11197 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11198 | `struct extract_aux_data` |
|        - | 11199 |  |
|        - | 11200 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11201 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11202 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11203 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11204 | `	int iFlags;           /* Control flags */` |
|        - | 11205 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11206 | `};` |
|        - | 11207 | `/* Forward declaration */` |
|        - | 11208 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11209 | `/*` |
|        - | 11210 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11211 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11212 | ` * Parameters` |
|        - | 11213 | ` * $var_array` |
|        - | 11214 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11215 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11216 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11217 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11218 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11219 | ` * $extract_type` |
|        - | 11220 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11221 | ` *  It can be one of the following values:` |
|        - | 11222 | ` *   EXTR_OVERWRITE` |
|        - | 11223 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11224 | ` *   EXTR_SKIP` |
|        - | 11225 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11226 | ` *   EXTR_PREFIX_SAME` |
|        - | 11227 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11228 | ` *   EXTR_PREFIX_ALL` |
|        - | 11229 | ` *       Prefix all variable names with prefix.` |
|        - | 11230 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11231 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11232 | ` *   EXTR_IF_EXISTS` |
|        - | 11233 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11234 | ` *       otherwise do nothing.` |
|        - | 11235 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11236 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11237 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11238 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11239 | ` *      the current symbol table.` |
|        - | 11240 | ` * $prefix` |
|        - | 11241 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11242 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11243 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11244 | ` *  underscore character.` |
|        - | 11245 | ` * Return` |
|        - | 11246 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11247 | ` */` |
|        4 | 11248 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11249 |  |
|        - | 11250 | `	extract_aux_data sAux;` |
|        - | 11251 | `	ph7_hashmap *pMap;` |
|        5 | 11252 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11253 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11254 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11255 | `		return PH7_OK;` |
|        - | 11256 | `	}` |
|        - | 11257 | `	/* Point to the target hashmap */` |
|        5 | 11258 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11259 | `	if( pMap->nEntry < 1 ){` |
|        - | 11260 | `		/* Empty map,return  0 */` |
|      ! 0 | 11261 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11262 | `		return PH7_OK;` |
|        - | 11263 | `	}` |
|        - | 11264 | `	/* Prepare the aux data */` |
|        5 | 11265 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11266 | `	if( nArg > 1 ){` |
|        3 | 11267 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11268 | `		if( nArg > 2 ){` |
|      ! 0 | 11269 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11270 | `		}` |
|        1 | 11271 | `	}` |
|        5 | 11272 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11273 | `	/* Invoke the worker callback */` |
|        5 | 11274 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11275 | `	/* Number of variables successfully imported */` |
|        5 | 11276 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11277 | `	return PH7_OK;` |
|        3 | 11278 |  |
|        - | 11279 | `/*` |
|        - | 11280 | ` * Worker callback for the [extract()] function defined` |
|        - | 11281 | ` * below.` |
|        - | 11282 | ` */` |
|        8 | 11283 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11284 |  |
|        9 | 11285 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11286 | `	int iFlags = pAux->iFlags;` |
|        9 | 11287 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11288 | `	ph7_value *pObj;` |
|        - | 11289 | `	SyString sVar;` |
|        9 | 11290 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11291 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11292 | `	}` |
|        - | 11293 | `	/* Perform a string cast */` |
|        9 | 11294 | `	PH7_MemObjToString(pKey);` |
|        9 | 11295 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11296 | `		/* Unavailable variable name */` |
|      ! 0 | 11297 | `		return SXRET_OK;` |
|        - | 11298 | `	}` |
|        9 | 11299 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11300 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11301 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11302 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11303 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11304 | `			);` |
|      ! 0 | 11305 | `	}else{` |
|       13 | 11306 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11307 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11308 | `	}` |
|        9 | 11309 | `	sVar.zString = pAux->zWorker;` |
|        - | 11310 | `	/* Try to extract the variable */` |
|        9 | 11311 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11312 | `	if( pObj ){` |
|        - | 11313 | `		/* Collision */` |
|        5 | 11314 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11315 | `			return SXRET_OK;` |
|        - | 11316 | `		}` |
|        5 | 11317 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11318 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11319 | `				/* Already prefixed */` |
|      ! 0 | 11320 | `				return SXRET_OK;` |
|        - | 11321 | `			}` |
|      ! 0 | 11322 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11323 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11324 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11325 | `				);` |
|      ! 0 | 11326 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11327 | `		}` |
|        3 | 11328 | `	}else{` |
|        - | 11329 | `		/* Create the variable */` |
|        5 | 11330 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11331 | `	}` |
|        9 | 11332 | `	if( pObj ){` |
|        - | 11333 | `		/* Overwrite the old value */` |
|        9 | 11334 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11335 | `		/* Increment counter */` |
|        9 | 11336 | `		pAux->iCount++;` |
|        4 | 11337 | `	}` |
|        9 | 11338 | `	return SXRET_OK;` |
|        5 | 11339 |  |
|        - | 11340 | `/*` |
|        - | 11341 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11342 | ` * defined below.` |
|        - | 11343 | ` */` |
|        2 | 11344 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11345 |  |
|        3 | 11346 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11347 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11348 | `	ph7_value *pObj;` |
|        - | 11349 | `	SyString sVar;` |
|        - | 11350 | `	/* Perform a string cast */` |
|        3 | 11351 | `	PH7_MemObjToString(pKey);` |
|        3 | 11352 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11353 | `		/* Unavailable variable name */` |
|      ! 0 | 11354 | `		return SXRET_OK;` |
|        - | 11355 | `	}` |
|        3 | 11356 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11357 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11358 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11359 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11360 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11361 | `			);` |
|        2 | 11362 | `	}else{` |
|      ! 0 | 11363 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11364 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11365 | `	}` |
|        3 | 11366 | `	sVar.zString = pAux->zWorker;` |
|        - | 11367 | `	/* Extract the variable */` |
|        3 | 11368 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11369 | `	if( pObj ){` |
|        3 | 11370 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11371 | `	}` |
|        3 | 11372 | `	return SXRET_OK;` |
|        2 | 11373 |  |
|        - | 11374 | `/*` |
|        - | 11375 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11376 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11377 | ` * Parameters` |
|        - | 11378 | ` * $types` |
|        - | 11379 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11380 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11381 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11382 | ` *  POST includes the POST uploaded file information.` |
|        - | 11383 | ` *  Note:` |
|        - | 11384 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11385 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11386 | ` * $prefix` |
|        - | 11387 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11388 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11389 | ` *  variable named $pref_userid.` |
|        - | 11390 | ` * Return` |
|        - | 11391 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11392 | ` */` |
|        2 | 11393 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11394 |  |
|        - | 11395 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11396 | `	extract_aux_data sAux;` |
|        - | 11397 | `	int nLen,nPrefixLen;` |
|        - | 11398 | `	ph7_value *pSuper;` |
|        - | 11399 | `	ph7_vm *pVm;` |
|        - | 11400 | `	/* By default import only $_GET variables  */` |
|        3 | 11401 | `	zImport = "G";` |
|        3 | 11402 | `	nLen = (int)sizeof(char);` |
|        3 | 11403 | `	zPrefix = 0;` |
|        3 | 11404 | `	nPrefixLen = 0;` |
|        3 | 11405 | `	if( nArg > 0 ){` |
|        3 | 11406 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11407 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11408 | `		}` |
|        3 | 11409 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11410 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11411 | `		}` |
|        1 | 11412 | `	}` |
|        - | 11413 | `	/* Point to the underlying VM */` |
|        3 | 11414 | `	pVm = pCtx->pVm;` |
|        - | 11415 | `	/* Initialize the aux data */` |
|        3 | 11416 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11417 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11418 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11419 | `	sAux.pVm = pVm;` |
|        - | 11420 | `	/* Extract */` |
|        3 | 11421 | `	zEnd = &zImport[nLen];` |
|        5 | 11422 | `	while( zImport < zEnd ){` |
|        3 | 11423 | `		int c = zImport[0];` |
|        3 | 11424 | `		pSuper = 0;` |
|        3 | 11425 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11426 | `			/* Import $_GET variables */` |
|        3 | 11427 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11428 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11429 | `			/* Import $_POST variables */` |
|      ! 0 | 11430 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11431 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11432 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11433 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11434 | `		}` |
|        3 | 11435 | `		if( pSuper ){` |
|        - | 11436 | `			/* Iterate throw array entries */` |
|        3 | 11437 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11438 | `		}` |
|        - | 11439 | `		/* Advance the cursor */` |
|        3 | 11440 | `		zImport++;` |
|        1 | 11441 | `	}` |
|        - | 11442 | `	/* All done,return TRUE*/` |
|        3 | 11443 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11444 | `	return PH7_OK;` |
|        1 | 11445 |  |
|        - | 11446 | `/*` |
|        - | 11447 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11448 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11449 | ` * information.` |
|        - | 11450 | ` */` |
|    10652 | 11451 | `static sxi32 VmEvalChunk(` |
|        - | 11452 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11453 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11454 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11455 | `	int iFlags,         /* Compile flag */` |
|        - | 11456 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11457 | `	)` |
|        2 | 11458 |  |
|        - | 11459 | `	SySet *pByteCode,aByteCode;` |
|        - | 11460 | `	SyBlob sSavedNs;` |
|    10654 | 11461 | `	ProcConsumer xErr = 0;` |
|    10654 | 11462 | `	void *pErrData = 0;` |
|        - | 11463 | `	/* Initialize bytecode container */` |
|    10654 | 11464 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10654 | 11465 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11466 | `	/* Reset the code generator */` |
|    10654 | 11467 | `	if( bTrueReturn ){` |
|        - | 11468 | `		/* Included file,log compile-time errors */` |
|     8058 | 11469 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8058 | 11470 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4028 | 11471 | `	}` |
|    10654 | 11472 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11473 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11474 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11475 | `	 * the caller's namespace is restored. */` |
|    10654 | 11476 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10654 | 11477 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10654 | 11478 | `	if( bTrueReturn ){` |
|        - | 11479 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8058 | 11480 | `		SyBlobReset(&pVm->sNamespace);` |
|     4028 | 11481 | `	}` |
|        - | 11482 | `	/* Swap bytecode container */` |
|    10654 | 11483 | `	pByteCode = pVm->pByteContainer;` |
|    10654 | 11484 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11485 | `	/* Compile the chunk */` |
|    10654 | 11486 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15980 | 11487 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11488 | `		/* Compilation error,return false */` |
|        3 | 11489 | `		if( pCtx ){` |
|        3 | 11490 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11491 | `		}` |
|        2 | 11492 | `	}else{` |
|        - | 11493 | `		/* Mount any newly defined classes */` |
|        - | 11494 | `		SyHashEntry *pEntry;` |
|        - | 11495 | `		ph7_class *pClass;` |
|        - | 11496 | `		ph7_value sResult; /* Return value */` |
|        - | 11497 | `		sxi32 rc;` |
|    10652 | 11498 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   345419 | 11499 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   329444 | 11500 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11501 | `			/* Only mount classes that haven't been mounted yet */` |
|   329444 | 11502 | `			if( !pClass->bMounted ){` |
|    76598 | 11503 | `				rc = VmMountUserClass(pVm,pClass);` |
|    76598 | 11504 | `				if( rc != SXRET_OK ){` |
|        - | 11505 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11506 | `					if( pCtx ){` |
|      ! 0 | 11507 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11508 | `					}` |
|      ! 0 | 11509 | `					goto Cleanup;` |
|        - | 11510 | `				}` |
|    38298 | 11511 | `			}` |
|        2 | 11512 | `		}` |
|    10652 | 11513 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11514 | `			/* Out of memory */` |
|      ! 0 | 11515 | `			if( pCtx ){` |
|      ! 0 | 11516 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11517 | `			}` |
|      ! 0 | 11518 | `			goto Cleanup;` |
|        - | 11519 | `		}` |
|    10652 | 11520 | `		if( bTrueReturn ){` |
|        - | 11521 | `			/* Assume a boolean true return value */` |
|     8058 | 11522 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4030 | 11523 | `		}else{` |
|        - | 11524 | `			/* Assume a null return value */` |
|     2596 | 11525 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11526 | `		}` |
|        - | 11527 | `		/* Execute the compiled chunk */` |
|    10652 | 11528 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10652 | 11529 | `		if( pCtx ){` |
|        - | 11530 | `			/* Set the execution result */` |
|     8076 | 11531 | `			ph7_result_value(pCtx,&sResult);` |
|     4037 | 11532 | `		}` |
|    10652 | 11533 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11534 | `	}` |
|     5326 | 11535 | `Cleanup:` |
|        - | 11536 | `	/* Cleanup the mess left behind */` |
|    10654 | 11537 | `	pVm->pByteContainer = pByteCode;` |
|    10654 | 11538 | `	SySetRelease(&aByteCode);` |
|        - | 11539 | `	/* Restore caller's namespace state */` |
|    10654 | 11540 | `	SyBlobReset(&pVm->sNamespace);` |
|    10654 | 11541 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10654 | 11542 | `	SyBlobRelease(&sSavedNs);` |
|    10654 | 11543 | `	return SXRET_OK;` |
|        2 | 11544 |  |
|        - | 11545 | `/*` |
|        - | 11546 | ` * value eval(string $code)` |
|        - | 11547 | ` *   Evaluate a string as PHP code.` |
|        - | 11548 | ` * Parameter` |
|        - | 11549 | ` *  code: PHP code to evaluate.` |
|        - | 11550 | ` * Return` |
|        - | 11551 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11552 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11553 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11554 | ` */` |
|       22 | 11555 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11556 |  |
|        - | 11557 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11558 | `	if( nArg < 1 ){` |
|        - | 11559 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11560 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11561 | `		return SXRET_OK;` |
|        - | 11562 | `	}` |
|        - | 11563 | `	/* Chunk to evaluate */` |
|       24 | 11564 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11565 | `	if( sChunk.nByte < 1 ){` |
|        - | 11566 | `		/* Empty string,return NULL */` |
|        3 | 11567 | `		ph7_result_null(pCtx);` |
|        3 | 11568 | `		return SXRET_OK;` |
|        - | 11569 | `	}` |
|        - | 11570 | `	/* Eval the chunk */` |
|       22 | 11571 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11572 | `	return SXRET_OK;` |
|       13 | 11573 |  |
|        - | 11574 | `/*` |
|        - | 11575 | ` * Check if a file path is already included.` |
|        - | 11576 | ` */` |
|    16108 | 11577 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11578 |  |
|        - | 11579 | `	SyString *aEntries;` |
|        - | 11580 | `	sxu32 n;` |
|    16110 | 11581 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11582 | `	/* Perform a linear search */` |
| 64822618 | 11583 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 64806516 | 11584 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11585 | `			/* Already included */` |
|        7 | 11586 | `			return TRUE;` |
|        - | 11587 | `		}` |
| 32403256 | 11588 | `	}` |
|    16104 | 11589 | `	return FALSE;` |
|     8056 | 11590 |  |
|        - | 11591 | `/*` |
|        - | 11592 | ` * Push a file path in the appropriate VM container.` |
|        - | 11593 | ` */` |
|    18676 | 11594 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11595 |  |
|        - | 11596 | `	SyString sPath;` |
|        - | 11597 | `	char *zDup;` |
|        - | 11598 | `#ifdef __WINNT__` |
|        - | 11599 | `	char *zCur;` |
|        - | 11600 | `#endif` |
|        - | 11601 | `	sxi32 rc;` |
|    18678 | 11602 | `	if( nLen < 0 ){` |
|     2570 | 11603 | `		nLen = SyStrlen(zPath);` |
|     1284 | 11604 | `	}` |
|        - | 11605 | `	/* Duplicate the file path first */` |
|    18678 | 11606 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18678 | 11607 | `	if( zDup == 0 ){` |
|      ! 0 | 11608 | `		return SXERR_MEM;` |
|        - | 11609 | `	}` |
|        - | 11610 | `#ifdef __WINNT__` |
|        - | 11611 | `	/* Normalize path on windows` |
|        - | 11612 | `	 * Example:` |
|        - | 11613 | `	 *    Path/To/File.php` |
|        - | 11614 | `	 * becomes` |
|        - | 11615 | `	 *   path\to\file.php` |
|        - | 11616 | `	 */` |
|        2 | 11617 | `	zCur = zDup;` |
|        2 | 11618 | `	while( zCur[0] != 0 ){` |
|        2 | 11619 | `		if( zCur[0] == '/' ){` |
|        2 | 11620 | `			zCur[0] = '\\';` |
|        2 | 11621 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11622 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11623 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11624 | `		}` |
|        2 | 11625 | `		zCur++;` |
|        2 | 11626 | `	}` |
|        - | 11627 | `#endif` |
|        - | 11628 | `	/* Install the file path */` |
|    18678 | 11629 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18678 | 11630 | `	if( !bMain ){` |
|    16110 | 11631 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11632 | `			/* Already included */` |
|        7 | 11633 | `			*pNew = 0;` |
|        4 | 11634 | `		}else{` |
|        - | 11635 | `			/* Insert in the corresponding container */` |
|    16104 | 11636 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16104 | 11637 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11638 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11639 | `				return rc;` |
|        - | 11640 | `			}` |
|    16104 | 11641 | `			*pNew = 1;` |
|        - | 11642 | `		}` |
|     8054 | 11643 | `	}` |
|    18678 | 11644 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18678 | 11645 | `	return SXRET_OK;` |
|     9340 | 11646 |  |
|        - | 11647 | `/*` |
|        - | 11648 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11649 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11650 | ` * indicates failure.` |
|        - | 11651 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11652 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11653 | ` * operations.` |
|        - | 11654 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11655 | ` * this function is a no-op.` |
|        - | 11656 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11657 | ` * constructs for more information.` |
|        - | 11658 | ` */` |
|     8066 | 11659 | `static sxi32 VmExecIncludedFile(` |
|        - | 11660 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11661 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11662 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11663 | `	 )` |
|        2 | 11664 |  |
|        - | 11665 | `	sxi32 rc;` |
|        - | 11666 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11667 | `	const ph7_io_stream *pStream;` |
|        - | 11668 | `	SyBlob sContents;` |
|        - | 11669 | `	void *pHandle;` |
|        - | 11670 | `	ph7_vm *pVm;` |
|        - | 11671 | `	int isNew;` |
|        - | 11672 | `	/* Initialize fields */` |
|     8068 | 11673 | `	pVm = pCtx->pVm;` |
|     8068 | 11674 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8068 | 11675 | `	isNew = 0;` |
|        - | 11676 | `	/* Extract the associated stream */` |
|     8068 | 11677 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11678 | `	/*` |
|        - | 11679 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11680 | `	 * in a read-only mode.` |
|        - | 11681 | `	 */` |
|     8068 | 11682 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8068 | 11683 | `	if( pHandle == 0 ){` |
|        8 | 11684 | `		return SXERR_IO;` |
|        - | 11685 | `	}` |
|     8062 | 11686 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8062 | 11687 | `	if( IncludeOnce && !isNew ){` |
|        - | 11688 | `		/* Already included */` |
|        5 | 11689 | `		rc = SXERR_EXISTS;` |
|        3 | 11690 | `	}else{` |
|        - | 11691 | `		/* Read the whole file contents */` |
|     8058 | 11692 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8058 | 11693 | `		if( rc == SXRET_OK ){` |
|        - | 11694 | `			SyString sScript;` |
|        - | 11695 | `			/* Compile and execute the script */` |
|     8058 | 11696 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8058 | 11697 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4028 | 11698 | `		}` |
|        - | 11699 | `	}` |
|        - | 11700 | `	/* Pop from the set of included file */` |
|     8062 | 11701 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11702 | `	/* Close the handle */` |
|     8062 | 11703 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11704 | `	/* Release the working buffer */` |
|     8062 | 11705 | `	SyBlobRelease(&sContents);` |
|        - | 11706 | `#else` |
|        - | 11707 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11708 | `	SXUNUSED(pPath);` |
|        - | 11709 | `	SXUNUSED(IncludeOnce);` |
|        - | 11710 | `	rc = SXERR_IO;` |
|        - | 11711 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8062 | 11712 | `	return rc;` |
|     4035 | 11713 |  |
|        - | 11714 | `/*` |
|        - | 11715 | ` * string get_include_path(void)` |
|        - | 11716 | ` *  Gets the current include_path configuration option.` |
|        - | 11717 | ` * Parameter` |
|        - | 11718 | ` *  None` |
|        - | 11719 | ` * Return` |
|        - | 11720 | ` *  Included paths as a string` |
|        - | 11721 | ` */` |
|        2 | 11722 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11723 |  |
|        3 | 11724 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11725 | `	SyString *aEntry;` |
|        - | 11726 | `	int dir_sep;` |
|        - | 11727 | `	sxu32 n;` |
|        - | 11728 | `#ifdef __WINNT__` |
|        1 | 11729 | `	dir_sep = ';';` |
|        - | 11730 | `#else` |
|        - | 11731 | `	/* Assume UNIX path separator */` |
|        2 | 11732 | `	dir_sep = ':';` |
|        - | 11733 | `#endif` |
|        1 | 11734 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11735 | `	SXUNUSED(apArg);` |
|        - | 11736 | `	/* Point to the list of import paths */` |
|        3 | 11737 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11738 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11739 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11740 | `		if( n > 0 ){` |
|        - | 11741 | `			/* Append dir seprator */` |
|      ! 0 | 11742 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11743 | `		}` |
|        - | 11744 | `		/* Append path */` |
|        3 | 11745 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11746 | `	}` |
|        3 | 11747 | `	return PH7_OK;` |
|        1 | 11748 |  |
|        - | 11749 | `/*` |
|        - | 11750 | ` * string get_get_included_files(void)` |
|        - | 11751 | ` *  Gets the current include_path configuration option.` |
|        - | 11752 | ` * Parameter` |
|        - | 11753 | ` *  None` |
|        - | 11754 | ` * Return` |
|        - | 11755 | ` *  Included paths as a string` |
|        - | 11756 | ` */` |
|        2 | 11757 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11758 |  |
|        3 | 11759 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11760 | `	ph7_value *pArray,*pWorker;` |
|        - | 11761 | `	SyString *pEntry;` |
|        - | 11762 | `	int c,d;` |
|        - | 11763 | `	/* Create an array and a working value */` |
|        3 | 11764 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11765 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11766 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11767 | `		/* Out of memory,return null */` |
|      ! 0 | 11768 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11769 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11770 | `		SXUNUSED(apArg);` |
|      ! 0 | 11771 | `		return PH7_OK;` |
|        - | 11772 | `	}` |
|        3 | 11773 | `	c = d = '/';` |
|        - | 11774 | `#ifdef __WINNT__` |
|        1 | 11775 | `	d = '\\';` |
|        - | 11776 | `#endif` |
|        - | 11777 | `	/* Iterate throw entries */` |
|        3 | 11778 | `	SySetResetCursor(pFiles);` |
|     3811 | 11779 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11780 | `		const char *zBase,*zEnd;` |
|        - | 11781 | `		int iLen;` |
|        - | 11782 | `		/* reset the string cursor */` |
|     3809 | 11783 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11784 | `		/* Extract base name */` |
|     3809 | 11785 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11786 | `		/* Ignore trailing '/' */` |
|     5713 | 11787 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11788 | `			zEnd--;` |
|      ! 0 | 11789 | `		}` |
|     3809 | 11790 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   117501 | 11791 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   111789 | 11792 | `			zEnd--;` |
|        1 | 11793 | `		}` |
|     3809 | 11794 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3809 | 11795 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11796 | `		/* Copy entry name */` |
|     3809 | 11797 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11798 | `		/* Perform the insertion */` |
|     3809 | 11799 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11800 | `	}` |
|        - | 11801 | `	/* All done,return the created array */` |
|        3 | 11802 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11803 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11804 | `	 * by the engine as soon we return from this foreign` |
|        - | 11805 | `	 * function.` |
|        - | 11806 | `	 */` |
|        3 | 11807 | `	return PH7_OK;` |
|        2 | 11808 |  |
|        - | 11809 | `/*` |
|        - | 11810 | ` * include:` |
|        - | 11811 | ` * According to the PHP reference manual.` |
|        - | 11812 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11813 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11814 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11815 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11816 | ` *  and the current working directory before failing. The include()` |
|        - | 11817 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11818 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11819 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11820 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11821 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11822 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11823 | ` *  directory to find the requested file.` |
|        - | 11824 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11825 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11826 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11827 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11828 | ` */` |
|     8048 | 11829 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11830 |  |
|        - | 11831 | `	SyString sFile;` |
|        - | 11832 | `	sxi32 rc;` |
|     8050 | 11833 | `	if( nArg < 1 ){` |
|        - | 11834 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11835 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11836 | `		return SXRET_OK;` |
|        - | 11837 | `	}` |
|        - | 11838 | `	/* File to include */` |
|     8050 | 11839 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8050 | 11840 | `	if( sFile.nByte < 1 ){` |
|        - | 11841 | `		/* Empty string,return NULL */` |
|      ! 0 | 11842 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11843 | `		return SXRET_OK;` |
|        - | 11844 | `	}` |
|        - | 11845 | `	/* Open,compile and execute the desired script */` |
|     8050 | 11846 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8050 | 11847 | `	if( rc != SXRET_OK ){` |
|        - | 11848 | `		/* Emit a warning and return false */` |
|        3 | 11849 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11850 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11851 | `	}` |
|     8050 | 11852 | `	return SXRET_OK;` |
|     4026 | 11853 |  |
|        - | 11854 | `/*` |
|        - | 11855 | ` * include_once:` |
|        - | 11856 | ` *  According to the PHP reference manual.` |
|        - | 11857 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11858 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11859 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11860 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11861 | ` *   just once.` |
|        - | 11862 | ` */` |
|        4 | 11863 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11864 |  |
|        - | 11865 | `	SyString sFile;` |
|        - | 11866 | `	sxi32 rc;` |
|        5 | 11867 | `	if( nArg < 1 ){` |
|        - | 11868 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11869 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11870 | `		return SXRET_OK;` |
|        - | 11871 | `	}` |
|        - | 11872 | `	/* File to include */` |
|        5 | 11873 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11874 | `	if( sFile.nByte < 1 ){` |
|        - | 11875 | `		/* Empty string,return NULL */` |
|      ! 0 | 11876 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11877 | `		return SXRET_OK;` |
|        - | 11878 | `	}` |
|        - | 11879 | `	/* Open,compile and execute the desired script */` |
|        5 | 11880 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11881 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11882 | `		/* File already included,return TRUE */` |
|        3 | 11883 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11884 | `		return SXRET_OK;` |
|        - | 11885 | `	}` |
|        3 | 11886 | `	if( rc != SXRET_OK ){` |
|        - | 11887 | `		/* Emit a warning and return false */` |
|      ! 0 | 11888 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11889 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11890 | ` 	}` |
|        3 | 11891 | `	return SXRET_OK;` |
|        3 | 11892 |  |
|        - | 11893 | `/*` |
|        - | 11894 | ` * require.` |
|        - | 11895 | ` *  According to the PHP reference manual.` |
|        - | 11896 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11897 | ` *   also produce a fatal level error.` |
|        - | 11898 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11899 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11900 | ` */` |
|        6 | 11901 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11902 |  |
|        - | 11903 | `	SyString sFile;` |
|        - | 11904 | `	sxi32 rc;` |
|        8 | 11905 | `	if( nArg < 1 ){` |
|        - | 11906 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11907 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11908 | `		return SXRET_OK;` |
|        - | 11909 | `	}` |
|        - | 11910 | `	/* File to include */` |
|        8 | 11911 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 11912 | `	if( sFile.nByte < 1 ){` |
|        - | 11913 | `		/* Empty string,return NULL */` |
|      ! 0 | 11914 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11915 | `		return SXRET_OK;` |
|        - | 11916 | `	}` |
|        - | 11917 | `	/* Open,compile and execute the desired script */` |
|        8 | 11918 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 11919 | `	if( rc != SXRET_OK ){` |
|        - | 11920 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11921 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11922 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11923 | `		return PH7_ABORT;` |
|        - | 11924 | `	}` |
|        8 | 11925 | `	return SXRET_OK;` |
|        5 | 11926 |  |
|        - | 11927 | `/*` |
|        - | 11928 | ` * require_once:` |
|        - | 11929 | ` *  According to the PHP reference manual.` |
|        - | 11930 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11931 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11932 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11933 | ` *   and how it differs from its non _once siblings.` |
|        - | 11934 | ` */` |
|        4 | 11935 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11936 |  |
|        - | 11937 | `	SyString sFile;` |
|        - | 11938 | `	sxi32 rc;` |
|        5 | 11939 | `	if( nArg < 1 ){` |
|        - | 11940 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11941 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11942 | `		return SXRET_OK;` |
|        - | 11943 | `	}` |
|        - | 11944 | `	/* File to include */` |
|        5 | 11945 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11946 | `	if( sFile.nByte < 1 ){` |
|        - | 11947 | `		/* Empty string,return NULL */` |
|      ! 0 | 11948 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11949 | `		return SXRET_OK;` |
|        - | 11950 | `	}` |
|        - | 11951 | `	/* Open,compile and execute the desired script */` |
|        5 | 11952 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11953 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11954 | `		/* File already included,return TRUE */` |
|        3 | 11955 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11956 | `		return SXRET_OK;` |
|        - | 11957 | `	}` |
|        3 | 11958 | `	if( rc != SXRET_OK ){` |
|        - | 11959 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11960 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11961 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11962 | `		return PH7_ABORT;` |
|        - | 11963 | `	}` |
|        3 | 11964 | `	return SXRET_OK;` |
|        3 | 11965 |  |
|        - | 11966 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11967 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11968 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11969 | `/*` |
|        - | 11970 | ` * Section:` |
|        - | 11971 | ` *  SPL Autoloading functions.` |
|        - | 11972 | ` * Status:` |
|        - | 11973 | ` *  Stable.` |
|        - | 11974 | ` */` |
|        - | 11975 | `/*` |
|        - | 11976 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 11977 | ` *  Register given function as __autoload() implementation.` |
|        - | 11978 | ` * Parameters` |
|        - | 11979 | ` *  callback` |
|        - | 11980 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 11981 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 11982 | ` *  throw` |
|        - | 11983 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 11984 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 11985 | ` *  prepend` |
|        - | 11986 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 11987 | ` *   autoload stack instead of appending it.` |
|        - | 11988 | ` * Return` |
|        - | 11989 | ` *  TRUE on success, FALSE on failure.` |
|        - | 11990 | ` */` |
|       34 | 11991 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11992 |  |
|        - | 11993 | `	VmAutoloadCB sEntry;` |
|       36 | 11994 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 11995 | `	int iPrepend = 0;` |
|        - | 11996 | `	sxu32 n;` |
|       36 | 11997 | `	if( nArg < 1 ){` |
|        - | 11998 | `		/* No callback provided — register default spl_autoload.` |
|        - | 11999 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12000 | `		/* Check for duplicates first */` |
|        9 | 12001 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12002 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12003 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12004 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12005 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12006 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12007 | `				return SXRET_OK;` |
|        - | 12008 | `			}` |
|      ! 0 | 12009 | `		}` |
|        5 | 12010 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12011 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12012 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12013 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12014 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12015 | `		return SXRET_OK;` |
|        - | 12016 | `	}` |
|        - | 12017 | `	/* Validate that the callback is callable */` |
|       28 | 12018 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12019 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12020 | `		if( nArg >= 2 ){` |
|      ! 0 | 12021 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12022 | `		}` |
|      ! 0 | 12023 | `		if( iThrow ){` |
|      ! 0 | 12024 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12025 | `				"Argument is not callable");` |
|      ! 0 | 12026 | `		}` |
|      ! 0 | 12027 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12028 | `		return SXRET_OK;` |
|        - | 12029 | `	}` |
|        - | 12030 | `	/* Check for duplicates */` |
|       46 | 12031 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12032 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12033 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12034 | `			/* Already registered */` |
|      ! 0 | 12035 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12036 | `			return SXRET_OK;` |
|        - | 12037 | `		}` |
|       11 | 12038 | `	}` |
|        - | 12039 | `	/* Check prepend flag */` |
|       28 | 12040 | `	if( nArg >= 3 ){` |
|        3 | 12041 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12042 | `	}` |
|        - | 12043 | `	/* Store the callback */` |
|       28 | 12044 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12045 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12046 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12047 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12048 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12049 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12050 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12051 | `		VmAutoloadCB *aBase;` |
|        3 | 12052 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12053 | `		/* Rotate: move last entry to front */` |
|        3 | 12054 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12055 | `		if( aBase ){` |
|        - | 12056 | `			VmAutoloadCB sTemp;` |
|        - | 12057 | `			sxu32 i;` |
|        3 | 12058 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12059 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12060 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12061 | `			}` |
|        3 | 12062 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12063 | `		}` |
|        2 | 12064 | `	}else{` |
|       26 | 12065 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12066 | `	}` |
|       28 | 12067 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12068 | `	return SXRET_OK;` |
|       19 | 12069 |  |
|        - | 12070 | `/*` |
|        - | 12071 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12072 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12073 | ` * Parameters` |
|        - | 12074 | ` *  callback` |
|        - | 12075 | ` *   The autoload function being unregistered.` |
|        - | 12076 | ` * Return` |
|        - | 12077 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12078 | ` */` |
|       32 | 12079 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12080 |  |
|       34 | 12081 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12082 | `	sxu32 n,nEntry;` |
|       34 | 12083 | `	if( nArg < 1 ){` |
|      ! 0 | 12084 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12085 | `		return SXRET_OK;` |
|        - | 12086 | `	}` |
|       34 | 12087 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12088 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12089 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12090 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12091 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12092 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12093 | `			sxu32 i;` |
|       32 | 12094 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12095 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12096 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12097 | `			}` |
|        - | 12098 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12099 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12100 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12101 | `			return SXRET_OK;` |
|        - | 12102 | `		}` |
|        3 | 12103 | `	}` |
|        3 | 12104 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12105 | `	return SXRET_OK;` |
|       18 | 12106 |  |
|        - | 12107 | `/*` |
|        - | 12108 | ` * array spl_autoload_functions(void)` |
|        - | 12109 | ` *  Return all registered __autoload() functions.` |
|        - | 12110 | ` * Return` |
|        - | 12111 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12112 | ` *  an empty array is returned.` |
|        - | 12113 | ` */` |
|       20 | 12114 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12115 |  |
|       21 | 12116 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12117 | `	ph7_value *pArray;` |
|        - | 12118 | `	sxu32 n,nEntry;` |
|       10 | 12119 | `	SXUNUSED(nArg);` |
|       10 | 12120 | `	SXUNUSED(apArg);` |
|       21 | 12121 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12122 | `	if( pArray == 0 ){` |
|      ! 0 | 12123 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12124 | `		return SXRET_OK;` |
|        - | 12125 | `	}` |
|       21 | 12126 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12127 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12128 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12129 | `		if( pEntry ){` |
|       15 | 12130 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12131 | `		}` |
|        8 | 12132 | `	}` |
|       21 | 12133 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12134 | `	return SXRET_OK;` |
|       11 | 12135 |  |
|        - | 12136 | `/*` |
|        - | 12137 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12138 | ` *  Default implementation of __autoload().` |
|        - | 12139 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12140 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12141 | ` * Parameters` |
|        - | 12142 | ` *  class` |
|        - | 12143 | ` *   The class name being searched.` |
|        - | 12144 | ` *  file_extensions` |
|        - | 12145 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12146 | ` */` |
|        2 | 12147 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12148 |  |
|        - | 12149 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12150 | `	SyBlob sPath;` |
|        - | 12151 | `	int nClass;` |
|        - | 12152 | `	sxi32 rc;` |
|        3 | 12153 | `	if( nArg < 1 ){` |
|      ! 0 | 12154 | `		return SXRET_OK;` |
|        - | 12155 | `	}` |
|        3 | 12156 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12157 | `	if( nClass < 1 ){` |
|      ! 0 | 12158 | `		return SXRET_OK;` |
|        - | 12159 | `	}` |
|        - | 12160 | `	/* Default extensions */` |
|        3 | 12161 | `	zExt = ".php,.inc";` |
|        3 | 12162 | `	if( nArg >= 2 ){` |
|        - | 12163 | `		int nExt;` |
|      ! 0 | 12164 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12165 | `		if( nExt < 1 ){` |
|      ! 0 | 12166 | `			zExt = ".php,.inc";` |
|      ! 0 | 12167 | `		}` |
|      ! 0 | 12168 | `	}` |
|        3 | 12169 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12170 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12171 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12172 | `	zCur = zExt;` |
|        7 | 12173 | `	while( zCur < zEnd ){` |
|        - | 12174 | `		const char *zComma;` |
|        - | 12175 | `		SyString sFile;` |
|        - | 12176 | `		int i;` |
|        - | 12177 | `		/* Find next comma or end */` |
|        5 | 12178 | `		zComma = zCur;` |
|       21 | 12179 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12180 | `			zComma++;` |
|        1 | 12181 | `		}` |
|        - | 12182 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12183 | `		SyBlobReset(&sPath);` |
|       69 | 12184 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12185 | `			char c = zClass[i];` |
|       65 | 12186 | `			if( c == '\\' ){` |
|      ! 0 | 12187 | `				c = '/';` |
|       65 | 12188 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12189 | `				c = c + ('a' - 'A');` |
|        6 | 12190 | `			}` |
|       65 | 12191 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12192 | `		}` |
|        - | 12193 | `		/* Append extension */` |
|        5 | 12194 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12195 | `		/* Try to include the file */` |
|        5 | 12196 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12197 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12198 | `		if( rc == SXRET_OK ){` |
|        - | 12199 | `			/* File included successfully */` |
|      ! 0 | 12200 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12201 | `			return SXRET_OK;` |
|        - | 12202 | `		}` |
|        - | 12203 | `		/* Move past the comma */` |
|        5 | 12204 | `		zCur = zComma;` |
|        5 | 12205 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12206 | `			zCur++;` |
|        1 | 12207 | `		}` |
|        1 | 12208 | `	}` |
|        3 | 12209 | `	SyBlobRelease(&sPath);` |
|        3 | 12210 | `	return SXRET_OK;` |
|        2 | 12211 |  |
|        - | 12212 | `/* Table of built-in VM functions. */` |
|        - | 12213 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12214 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12215 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12216 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12217 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12218 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12219 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12220 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12221 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12222 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12223 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12224 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12225 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12226 | `	    /* Constants management */` |
|        - | 12227 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12228 | `	{ "define",   vm_builtin_define               },` |
|        - | 12229 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12230 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12231 | `	   /* Class/Object functions */` |
|        - | 12232 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12233 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12234 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12235 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12236 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12237 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12238 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12239 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12240 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12241 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12242 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12243 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12244 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12245 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12246 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12247 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12248 | `	   /* SPL Autoloading */` |
|        - | 12249 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12250 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12251 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12252 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12253 | `	   /* Random numbers/strings generators */` |
|        - | 12254 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12255 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12256 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12257 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12258 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12259 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12260 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12261 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12262 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12263 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12264 | `	   /* Language constructs functions */` |
|        - | 12265 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12266 | `	{ "print", vm_builtin_print                   },` |
|        - | 12267 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12268 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12269 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12270 | `	  /* Variable handling functions */` |
|        - | 12271 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12272 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12273 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12274 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12275 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12276 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12277 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12278 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12279 | `	  /* Ouput control functions */` |
|        - | 12280 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12281 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12282 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12283 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12284 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12285 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12286 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12287 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12288 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12289 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12290 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12291 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12292 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12293 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12294 | `	  /* Assertion functions */` |
|        - | 12295 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12296 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12297 | `	  /* Error reporting functions */` |
|        - | 12298 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12299 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12300 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12301 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12302 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12303 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12304 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12305 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12306 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12307 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12308 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12309 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12310 | `	  /* Release info */` |
|        - | 12311 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12312 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12313 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12314 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12315 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12316 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12317 | `	  /* hashmap */` |
|        - | 12318 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12319 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12320 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12321 | `	  /* URL related function */` |
|        - | 12322 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12323 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12324 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12325 | `	   /* XML processing functions */` |
|        - | 12326 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12327 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12328 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12329 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12330 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12331 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12332 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12333 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12334 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12335 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12336 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12337 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12338 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12339 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12340 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12341 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12342 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12343 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12344 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12345 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12346 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12347 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12348 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12349 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12350 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12351 | `	   /* Command line processing */` |
|        - | 12352 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12353 | `	   /* JSON encoding/decoding */` |
|        - | 12354 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12355 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12356 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12357 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12358 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12359 | `	   /* Files/URI inclusion facility */` |
|        - | 12360 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12361 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12362 | `	{ "include",      vm_builtin_include          },` |
|        - | 12363 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12364 | `	{ "require",      vm_builtin_require          },` |
|        - | 12365 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12366 | `};` |
|        - | 12367 | `/*` |
|        - | 12368 | ` * Register the built-in VM functions defined above.` |
|        - | 12369 | ` */` |
|     2316 | 12370 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12371 |  |
|        - | 12372 | `	sxi32 rc;` |
|        - | 12373 | `	sxu32 n;` |
|   298766 | 12374 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12375 | `		/* Note that these special functions have access` |
|        - | 12376 | `		 * to the underlying virtual machine as their` |
|        - | 12377 | `		 * private data.` |
|        - | 12378 | `		 */` |
|   296450 | 12379 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   296450 | 12380 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12381 | `			return rc;` |
|        - | 12382 | `		}` |
|   148226 | 12383 | `	}` |
|     2318 | 12384 | `	return SXRET_OK;` |
|     1160 | 12385 |  |
|        - | 12386 | `/*` |
|        - | 12387 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12388 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12389 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12390 | ` */` |
|    27344 | 12391 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12392 |  |
|    27346 | 12393 | `	if( !iLoadable ){` |
|    26140 | 12394 | `		return pClass;` |
|        - | 12395 | `	}` |
|     1208 | 12396 | `	while(pClass){` |
|     1208 | 12397 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1208 | 12398 | `			return pClass;` |
|        - | 12399 | `		}` |
|      ! 0 | 12400 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12401 | `	}` |
|      ! 0 | 12402 | `	return 0;` |
|    13674 | 12403 |  |
|        - | 12404 | `/*` |
|        - | 12405 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12406 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12407 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12408 | ` * registered in the VM's class table.` |
|        - | 12409 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12410 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12411 | ` */` |
|       30 | 12412 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12413 |  |
|        - | 12414 | `	VmAutoloadCB *pEntry;` |
|        - | 12415 | `	ph7_value sArg,sResult;` |
|        - | 12416 | `	SyHashEntry *pHashEntry;` |
|        - | 12417 | `	ph7_class *pClass;` |
|        - | 12418 | `	sxu32 n,nEntry;` |
|       32 | 12419 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       32 | 12420 | `	if( nEntry < 1 ){` |
|       18 | 12421 | `		return 0;` |
|        - | 12422 | `	}` |
|        - | 12423 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12424 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12425 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12426 | `	}` |
|        - | 12427 | `	/* Mark this class as being autoloaded */` |
|       14 | 12428 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12429 | `	/* Prepare the class name argument */` |
|       14 | 12430 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12431 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12432 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12433 | `	pClass = 0;` |
|       28 | 12434 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12435 | `		ph7_value *apArg[1];` |
|       24 | 12436 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12437 | `		if( pEntry == 0 ){` |
|      ! 0 | 12438 | `			continue;` |
|        - | 12439 | `		}` |
|       24 | 12440 | `		apArg[0] = &sArg;` |
|       24 | 12441 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12442 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12443 | `			continue;` |
|        - | 12444 | `		}` |
|        - | 12445 | `		/* Check if the class is now available */` |
|       24 | 12446 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12447 | `		if( pHashEntry ){` |
|       10 | 12448 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12449 | `			if( pClass ){` |
|       10 | 12450 | `				break;` |
|        - | 12451 | `			}` |
|      ! 0 | 12452 | `		}` |
|        9 | 12453 | `	}` |
|       14 | 12454 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12455 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12456 | `	/* Remove reentrancy guard */` |
|       14 | 12457 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12458 | `	return pClass;` |
|       17 | 12459 |  |
|        - | 12460 | `/*` |
|        - | 12461 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12462 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12463 | ` */` |
|       18 | 12464 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12465 |  |
|       20 | 12466 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12467 |  |
|        - | 12468 | `/*` |
|        - | 12469 | ` * Check if the given name refer to an installed class.` |
|        - | 12470 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12471 | ` */` |
|    27348 | 12472 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12473 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12474 | `	const char *zName,  /* Name of the target class */` |
|        - | 12475 | `	sxu32 nByte,        /* zName length */` |
|        - | 12476 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12477 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12478 | `						 */` |
|        - | 12479 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12480 | `	)` |
|        2 | 12481 |  |
|        - | 12482 | `	SyHashEntry *pEntry;` |
|        - | 12483 | `	ph7_class *pClass;` |
|    13674 | 12484 | `	SXUNUSED(iNest);` |
|        - | 12485 | `	/* Exact class lookup.` |
|        - | 12486 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12487 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    27350 | 12488 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    27350 | 12489 | `	if( pEntry == 0 ){` |
|        - | 12490 | `		/* Class not found in hash table — try autoload before giving up */` |
|       14 | 12491 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12492 | `	}` |
|    27338 | 12493 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    27338 | 12494 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    13676 | 12495 |  |
|        - | 12496 | `/*` |
|        - | 12497 | ` * Reference Table Implementation` |
|        - | 12498 | ` * Status: stable <chm@symisc.net>` |
|        - | 12499 | ` * Intro` |
|        - | 12500 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12501 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12502 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12503 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12504 | ` *  Refer to the official for more information on this powerful` |
|        - | 12505 | ` *  extension.` |
|        - | 12506 | ` */` |
|        - | 12507 | `/*` |
|        - | 12508 | ` * Allocate a new reference entry.` |
|        - | 12509 | ` */` |
|  3055834 | 12510 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12511 |  |
|        - | 12512 | `	VmRefObj *pRef;` |
|        - | 12513 | `	/* Allocate a new instance */` |
|  3055836 | 12514 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3055836 | 12515 | `	if( pRef == 0 ){` |
|      ! 0 | 12516 | `		return 0;` |
|        - | 12517 | `	}` |
|        - | 12518 | `	/* Zero the structure */` |
|  3055836 | 12519 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12520 | `	/* Initialize fields */` |
|  3055836 | 12521 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3055836 | 12522 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3055836 | 12523 | `	pRef->nIdx = nIdx;` |
|  3055836 | 12524 | `	return pRef;` |
|  1527919 | 12525 |  |
|        - | 12526 | `/*` |
|        - | 12527 | ` * Default hash function used by the reference table` |
|        - | 12528 | ` * for lookup/insertion operations.` |
|        - | 12529 | ` */` |
| 16879181 | 12530 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12531 |  |
|        - | 12532 | `	/* Calculate the hash based on the memory object index */` |
| 16879183 | 12533 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12534 |  |
|        - | 12535 | `/*` |
|        - | 12536 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12537 | ` * in the reference table.` |
|        - | 12538 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12539 | ` * otherwise.` |
|        - | 12540 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12541 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12542 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12543 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12544 | ` * Refer to the official for more information on this powerful` |
|        - | 12545 | ` * extension.` |
|        - | 12546 | ` */` |
|  9120944 | 12547 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12548 |  |
|        - | 12549 | `	VmRefObj *pRef;` |
|        - | 12550 | `	sxu32 nBucket;` |
|        - | 12551 | `	/* Point to the appropriate bucket */` |
|  9120946 | 12552 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12553 | `	/* Perform the lookup */` |
|  9120946 | 12554 | `	pRef = pVm->apRefObj[nBucket];` |
| 19848038 | 12555 | `	for(;;){` |
| 39684546 | 12556 | `		if( pRef == 0 ){` |
|  3135250 | 12557 | `			break;` |
|        - | 12558 | `		}` |
| 36549298 | 12559 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12560 | `			/* Entry found */` |
|  5985698 | 12561 | `			return pRef;` |
|        - | 12562 | `		}` |
|        - | 12563 | `		/* Point to the next entry */` |
| 30563602 | 12564 | `		pRef = pRef->pNextCollide;` |
|        2 | 12565 | `	}` |
|        - | 12566 | `	/* No such entry,return NULL */` |
|  3135250 | 12567 | `	return 0;` |
|  4560474 | 12568 |  |
|        - | 12569 | `/*` |
|        - | 12570 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12571 | ` *` |
|        - | 12572 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12573 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12574 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12575 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12576 | ` * Refer to the official for more information on this powerful` |
|        - | 12577 | ` * extension.` |
|        - | 12578 | ` */` |
|  3055834 | 12579 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12580 |  |
|        - | 12581 | `	sxu32 nBucket;` |
|  3055836 | 12582 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12583 | `		VmRefObj **apNew;` |
|        - | 12584 | `		sxu32 nNew;` |
|        - | 12585 | `		/* Allocate a larger table */` |
|     3958 | 12586 | `		nNew = pVm->nRefSize << 1;` |
|     3958 | 12587 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3958 | 12588 | `		if( apNew ){` |
|     3958 | 12589 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12590 | `			sxu32 n;` |
|        - | 12591 | `			/* Zero the structure */` |
|     3958 | 12592 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12593 | `			/* Rehash all referenced entries */` |
|  2840384 | 12594 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12595 | `				/* Remove old collision links */` |
|  2836428 | 12596 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12597 | `				/* Point to the appropriate bucket */` |
|  2836428 | 12598 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12599 | `				/* Insert the entry  */` |
|  2836428 | 12600 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2836428 | 12601 | `				if( apNew[nBucket] ){` |
|  2298896 | 12602 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12603 | `				}` |
|  2836428 | 12604 | `				apNew[nBucket] = pEntry;` |
|        - | 12605 | `				/* Point to the next entry */` |
|  2836428 | 12606 | `				pEntry = pEntry->pNext;` |
|  1418215 | 12607 | `			}` |
|        - | 12608 | `			/* Release the old table */` |
|     3958 | 12609 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12610 | `			/* Install the new one */` |
|     3958 | 12611 | `			pVm->apRefObj = apNew;` |
|     3958 | 12612 | `			pVm->nRefSize = nNew;` |
|     1978 | 12613 | `		}` |
|     1978 | 12614 | `	}` |
|        - | 12615 | `	/* Point to the appropriate bucket */` |
|  3055836 | 12616 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12617 | `	/* Insert the entry */` |
|  3055836 | 12618 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3055836 | 12619 | `	if( pVm->apRefObj[nBucket] ){` |
|  2525652 | 12620 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1262661 | 12621 | `	}` |
|  3055836 | 12622 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3055836 | 12623 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3055836 | 12624 | `	pVm->nRefUsed++;` |
|  3055836 | 12625 | `	return SXRET_OK;` |
|        2 | 12626 |  |
|        - | 12627 | `/*` |
|        - | 12628 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12629 | ` * the reference table.` |
|        - | 12630 | ` * This function is invoked when the user perform an unset` |
|        - | 12631 | ` * call [i.e: unset($var); ].` |
|        - | 12632 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12633 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12634 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12635 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12636 | ` * Refer to the official for more information on this powerful` |
|        - | 12637 | ` * extension.` |
|        - | 12638 | ` */` |
|  3022464 | 12639 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12640 |  |
|        - | 12641 | `	ph7_hashmap_node **apNode;` |
|        - | 12642 | `	SyHashEntry **apEntry;` |
|        - | 12643 | `	sxu32 n;` |
|        - | 12644 | `	/* Point to the reference table */` |
|  3022466 | 12645 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3022466 | 12646 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12647 | `	/* Unlink the entry from the reference table */` |
|  3107832 | 12648 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85368 | 12649 | `		if( apEntry[n] ){` |
|    85318 | 12650 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42658 | 12651 | `		}` |
|    42685 | 12652 | `	}` |
|  5962310 | 12653 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2939846 | 12654 | `		if( apNode[n] ){` |
|     6880 | 12655 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3439 | 12656 | `		}` |
|  1469924 | 12657 | `	}` |
|  3022466 | 12658 | `	if( pRef->pPrevCollide ){` |
|  1156489 | 12659 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   578222 | 12660 | `	}else{` |
|  1865979 | 12661 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12662 | `	}` |
|  3022466 | 12663 | `	if( pRef->pNextCollide ){` |
|  1714676 | 12664 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   857155 | 12665 | `	}` |
|  3022466 | 12666 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12667 | `	/* Release the node */` |
|  3022466 | 12668 | `	SySetRelease(&pRef->aReference);` |
|  3022466 | 12669 | `	SySetRelease(&pRef->aArrEntries);` |
|  3022466 | 12670 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3022466 | 12671 | `	pVm->nRefUsed--;` |
|  3022466 | 12672 | `	return SXRET_OK;` |
|        2 | 12673 |  |
|        - | 12674 | `/*` |
|        - | 12675 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12676 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12677 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12678 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12679 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12680 | ` * Refer to the official for more information on this powerful` |
|        - | 12681 | ` * extension.` |
|        - | 12682 | ` */` |
|  3086038 | 12683 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12684 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12685 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12686 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12687 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12688 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12689 | `	)` |
|        2 | 12690 |  |
|  3086040 | 12691 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12692 | `	VmRefObj *pRef;` |
|        - | 12693 | `	/* Check if the referenced object already exists */` |
|  3086040 | 12694 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3086040 | 12695 | `	if( pRef == 0 ){` |
|        - | 12696 | `		/* Create a new entry */` |
|  3055836 | 12697 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3055836 | 12698 | `		if( pRef == 0 ){` |
|      ! 0 | 12699 | `			return SXERR_MEM;` |
|        - | 12700 | `		}` |
|  3055836 | 12701 | `		pRef->iFlags = iFlags;` |
|        - | 12702 | `		/* Install the entry */` |
|  3055836 | 12703 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1527917 | 12704 | `	}` |
|  3086040 | 12705 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3086040 | 12706 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12707 | `		VmSlot sRef;` |
|        - | 12708 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12709 | `		 * be deleted when we leave this frame.` |
|        - | 12710 | `		 */` |
|    79494 | 12711 | `		sRef.nIdx = nIdx;` |
|    79494 | 12712 | `		sRef.pUserData = pEntry;` |
|    79494 | 12713 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12714 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12715 | `		}` |
|    39746 | 12716 | `	}` |
|  3086040 | 12717 | `	if( pEntry ){` |
|        - | 12718 | `		/* Address of the hash-entry */` |
|   109506 | 12719 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    54752 | 12720 | `	}` |
|  3086040 | 12721 | `	if( pMapEntry ){` |
|        - | 12722 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2971538 | 12723 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1485768 | 12724 | `	}` |
|  3086040 | 12725 | `	return SXRET_OK;` |
|  1543021 | 12726 |  |
|        - | 12727 | `/*` |
|        - | 12728 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12729 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12730 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12731 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12732 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12733 | ` * Refer to the official for more information on this powerful` |
|        - | 12734 | ` * extension.` |
|        - | 12735 | ` */` |
|  3012436 | 12736 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12737 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12738 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12739 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12740 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12741 | `	)` |
|        2 | 12742 |  |
|        - | 12743 | `	VmRefObj *pRef;` |
|        - | 12744 | `	sxu32 n;` |
|        - | 12745 | `	/* Check if the referenced object already exists */` |
|  3012438 | 12746 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3012438 | 12747 | `	if( pRef == 0 ){` |
|        - | 12748 | `		/* Not such entry */` |
|    79410 | 12749 | `		return SXERR_NOTFOUND;` |
|        - | 12750 | `	}` |
|        - | 12751 | `	/* Remove the desired entry */` |
|  2933030 | 12752 | `	if( pEntry ){` |
|        - | 12753 | `		SyHashEntry **apEntry;` |
|       56 | 12754 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12755 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12756 | `			if( apEntry[n] == pEntry ){` |
|        - | 12757 | `				/* Nullify the entry */` |
|       56 | 12758 | `				apEntry[n] = 0;` |
|        - | 12759 | `				/*` |
|        - | 12760 | `				 * NOTE:` |
|        - | 12761 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12762 | `				 * we avoid wasting spaces.` |
|        - | 12763 | `				 */` |
|       27 | 12764 | `			}` |
|       79 | 12765 | `		}` |
|       27 | 12766 | `	}` |
|  2933030 | 12767 | `	if( pMapEntry ){` |
|        - | 12768 | `		ph7_hashmap_node **apNode;` |
|  2932976 | 12769 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5866044 | 12770 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2933070 | 12771 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12772 | `				/* nullify the entry */` |
|  2932976 | 12773 | `				apNode[n] = 0;` |
|  1466487 | 12774 | `			}` |
|  1466536 | 12775 | `		}` |
|  1466487 | 12776 | `	}` |
|  2933030 | 12777 | `	return SXRET_OK;` |
|  1506220 | 12778 |  |
|        - | 12779 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12780 | `/*` |
|        - | 12781 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12782 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12783 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12784 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12785 | ` * For more information on how to register IO stream devices,please` |
|        - | 12786 | ` * refer to the official documentation.` |
|        - | 12787 | ` */` |
|    24436 | 12788 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12789 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12790 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12791 | `	int nByte              /* *pzDevice length*/` |
|        - | 12792 | `	)` |
|        2 | 12793 |  |
|        - | 12794 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12795 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12796 | `	SyString sDev,sCur;` |
|        - | 12797 | `	sxu32 n,nEntry;` |
|        - | 12798 | `	int rc;` |
|        - | 12799 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    24438 | 12800 | `	zNext = zCur = zIn = *pzDevice;` |
|    24438 | 12801 | `	zEnd = &zIn[nByte];` |
|  1556824 | 12802 | `	while( zIn < zEnd ){` |
|  1532390 | 12803 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12804 | `			/* Got one */` |
|        3 | 12805 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12806 | `			break;` |
|        - | 12807 | `		}` |
|        - | 12808 | `		/* Advance the cursor */` |
|  1532388 | 12809 | `		zIn++;` |
|        2 | 12810 | `	}` |
|    24438 | 12811 | `	if( zIn >= zEnd ){` |
|        - | 12812 | `		/* No such scheme,return the default stream */` |
|    24436 | 12813 | `		return pVm->pDefStream;` |
|        - | 12814 | `	}` |
|        3 | 12815 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12816 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12817 | `	SyStringFullTrim(&sDev);` |
|        - | 12818 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12819 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12820 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12821 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12822 | `		pStream = apStream[n];` |
|        3 | 12823 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12824 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12825 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12826 | `		if( rc == 0 ){` |
|        - | 12827 | `			/* Stream device found */` |
|        3 | 12828 | `			*pzDevice = zNext;` |
|        3 | 12829 | `			return pStream;` |
|        - | 12830 | `		}` |
|      ! 0 | 12831 | `	}` |
|        - | 12832 | `	/* No such stream,return NULL */` |
|      ! 0 | 12833 | `	return 0;` |
|    12220 | 12834 |  |
|        - | 12835 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12836 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12837 |  |
