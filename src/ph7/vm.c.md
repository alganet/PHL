# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5135/6723 lines (76.38%)

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
|   809184 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   809186 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   809152 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   809142 |   104 | `	return FALSE;` |
|   404616 |   105 |  |
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
|   518656 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   518658 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   518658 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   518654 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   518654 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   518654 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   518654 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   518654 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   518654 |   152 | `	pCons->xExpand = xExpand;` |
|   518654 |   153 | `	pCons->pUserData = pUserData;` |
|   518654 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   518654 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   518654 |   161 | `	return SXRET_OK;` |
|   259330 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1140304 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1140306 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1140306 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1140306 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1140306 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1140306 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1140306 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1140306 |   195 | `	pFunc->pVm   = pVm;` |
|  1140306 |   196 | `	pFunc->xFunc = xFunc;` |
|  1140306 |   197 | `	pFunc->pUserData = pUserData;` |
|  1140306 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1140306 |   200 | `	*ppOut = pFunc;` |
|  1140306 |   201 | `	return SXRET_OK;` |
|   570154 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1142694 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1142696 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1142696 |   223 | `	if( pEntry ){` |
|     2392 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2392 |   225 | `		pFunc->pUserData = pUserData;` |
|     2392 |   226 | `		pFunc->xFunc = xFunc;` |
|     2392 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2392 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1140306 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1140306 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1140306 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1140306 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1140306 |   243 | `	return SXRET_OK;` |
|   571349 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   163200 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   163202 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   163202 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   163202 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   163202 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   163202 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   163202 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   163202 |   270 | `	pFunc->iFlags = iFlags;` |
|   163202 |   271 | `	pFunc->pUserData = pUserData;` |
|   163202 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   163202 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   641554 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   641556 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    35226 |   293 | `		pName = &pFunc->sName;` |
|    17612 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   641556 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   641556 |   297 | `	if( pEntry ){` |
|   499872 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   499872 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      188 |   301 | `			pFunc->pNextName = pLink;` |
|      188 |   302 | `			pEntry->pUserData = pFunc;` |
|       93 |   303 | `		}` |
|   499872 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   141686 |   307 | `	pFunc->pNextName = 0;` |
|   141686 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   141686 |   309 | `	return rc;` |
|   320779 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    45716 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    45718 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    45718 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    45718 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    45688 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    45688 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    45688 |   334 | `	return rc;` |
|    22860 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3294146 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3294148 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3294148 |   352 | `	sInstr.iP1 = iP1;` |
|  3294148 |   353 | `	sInstr.iP2 = iP2;` |
|  3294148 |   354 | `	sInstr.p3  = p3;` |
|  3294148 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   189922 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    94960 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3294148 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3294148 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3294148 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   390912 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   390914 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   390914 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   390914 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   195456 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   195458 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   187188 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   187190 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   187190 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   987146 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   987148 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   177966 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   177968 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   637928 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   637930 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    27420 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    27422 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    27422 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    27422 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    27422 |   427 | `	return &aInstr[n - 2];` |
|    13712 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    16642 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16644 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16644 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16644 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16644 |   447 | `	pFrame->pUserData = pUserData;` |
|    16644 |   448 | `	pFrame->pThis = pThis;` |
|    16644 |   449 | `	pFrame->pVm = pVm;` |
|    16644 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16644 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16644 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16644 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16644 |   454 | `	return pFrame;` |
|     8323 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    16600 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    16602 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16602 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    16602 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    16602 |   476 | `	pVm->pFrame = pFrame;` |
|    16602 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13946 |   479 | `		*ppFrame = pFrame;` |
|     6972 |   480 | `	}` |
|    16602 |   481 | `	return SXRET_OK;` |
|     8302 |   482 |  |
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
|    13944 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13946 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13946 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13946 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13946 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13848 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    95762 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    81916 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    40959 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13848 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    95818 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    81972 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    40987 |   545 | `			}` |
|     6923 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13946 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13946 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13946 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13946 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13946 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6972 |   554 | `	}` |
|    13946 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6489060 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6489542 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      482 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6489062 |   566 | `	return pFrame;` |
|        2 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Compare two functions signature and return the comparison result.` |
|        - |   570 | ` */` |
|      836 |   571 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   572 |  |
|      837 |   573 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   574 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   575 | `	const char *zSin = pSecond->zString;` |
|      837 |   576 | `	const char *zFin = pFirst->zString;` |
|      837 |   577 | `	const char *zPtr = zFin;` |
|      421 |   578 | `	for(;;){` |
|      843 |   579 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   580 | `			break;` |
|        - |   581 | `		}` |
|       19 |   582 | `		if( zFin[0] != zSin[0] ){` |
|        - |   583 | `			/* mismatch */` |
|       13 |   584 | `			break;` |
|        - |   585 | `		}` |
|        7 |   586 | `		zFin++;` |
|        7 |   587 | `		zSin++;` |
|        1 |   588 | `	}` |
|      837 |   589 | `	return (int)(zFin-zPtr);` |
|        1 |   590 |  |
|        - |   591 | `/*` |
|        - |   592 | ` * Select the appropriate VM function for the current call context.` |
|        - |   593 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   594 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   595 | ` * Refer to the official documentation for more information.` |
|        - |   596 | ` */` |
|      138 |   597 | `static ph7_vm_func * VmOverload(` |
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
|      140 |   610 | `	pLink = pList;` |
|      140 |   611 | `	i = 0;` |
|        - |   612 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   613 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   614 | `		if( pLink == 0 ){` |
|       78 |   615 | `			break;` |
|        - |   616 | `		}` |
|      948 |   617 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   618 | `			/* Candidate for overloading */` |
|      902 |   619 | `			apSet[i++] = pLink;` |
|      450 |   620 | `		}` |
|        - |   621 | `		/* Point to the next entry */` |
|      948 |   622 | `		pLink = pLink->pNextName;` |
|        2 |   623 | `	}` |
|      140 |   624 | `	if( i < 1 ){` |
|        - |   625 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   626 | `		return pList;` |
|        - |   627 | `	}` |
|      140 |   628 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   629 | `		/* Return the only candidate */` |
|       32 |   630 | `		return apSet[0];` |
|        - |   631 | `	}` |
|        - |   632 | `	/* Calculate function signature */` |
|      109 |   633 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   634 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   635 | `		int c = 'n'; /* null */` |
|      259 |   636 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   637 | `			/* Hashmap */` |
|       45 |   638 | `			c = 'h';` |
|      237 |   639 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   640 | `			/* bool */` |
|      ! 0 |   641 | `			c = 'b';` |
|      215 |   642 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   643 | `			/* int */` |
|        7 |   644 | `			c = 'i';` |
|      212 |   645 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   646 | `			/* String */` |
|      107 |   647 | `			c = 's';` |
|      156 |   648 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   649 | `			/* Float */` |
|      ! 0 |   650 | `			c = 'f';` |
|      103 |   651 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   652 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   653 | `			int marker = 'o';` |
|        3 |   654 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   655 | `			SyString *pName = &pClass->sName;` |
|        3 |   656 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   657 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   658 | `			c = -1;` |
|        1 |   659 | `		}` |
|      259 |   660 | `		if( c > 0 ){` |
|      257 |   661 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   662 | `		}` |
|      130 |   663 | `	}` |
|      109 |   664 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   665 | `	iTarget = 0;` |
|      109 |   666 | `	iMax = -1;` |
|        - |   667 | `	/* Select the appropriate function */` |
|      945 |   668 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   669 | `		/* Compare the two signatures */` |
|      837 |   670 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   671 | `		if( iCur > iMax ){` |
|      113 |   672 | `			iMax = iCur;` |
|      113 |   673 | `			iTarget = j;` |
|       56 |   674 | `		}` |
|      419 |   675 | `	}` |
|      109 |   676 | `	SyBlobRelease(&sSig);` |
|        - |   677 | `	/* Appropriate function for the current call context */` |
|      109 |   678 | `	return apSet[iTarget];` |
|       71 |   679 |  |
|        - |   680 | `/* Forward declaration */` |
|        - |   681 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   682 | `/*` |
|        - |   683 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   684 | ` * it can be instanciated from the executed PHP script.` |
|        - |   685 | ` */` |
|   125360 |   686 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   687 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   688 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   689 | `	)` |
|        2 |   690 |  |
|        - |   691 | `	ph7_class_method *pMeth;` |
|        - |   692 | `	ph7_class_attr *pAttr;` |
|        - |   693 | `	SyHashEntry *pEntry;` |
|        - |   694 | `	sxi32 rc;` |
|        - |   695 | `	/* Reset the loop cursor */` |
|   125362 |   696 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   697 | `	/* Process only static and constant attribute */` |
|   527900 |   698 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   699 | `		/* Extract the current attribute */` |
|   339860 |   700 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   339860 |   701 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   702 | `			ph7_value *pMemObj;` |
|        - |   703 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1328 |   704 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1328 |   705 | `			if( pMemObj == 0 ){` |
|      ! 0 |   706 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   707 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   708 | `					&pClass->sName,&pAttr->sName` |
|        - |   709 | `					);` |
|      ! 0 |   710 | `				return SXERR_MEM;` |
|        - |   711 | `			}` |
|     1328 |   712 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   713 | `				/* Initialize attribute default value (any complex expression) */` |
|     1328 |   714 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      663 |   715 | `			}` |
|        - |   716 | `			/* Record attribute index */` |
|     1328 |   717 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   718 | `			/* Install static attribute in the reference table */` |
|     1328 |   719 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      663 |   720 | `		}` |
|        2 |   721 | `	}` |
|        - |   722 | `	/* Install class methods */` |
|   125362 |   723 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   724 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   725 | `		 */` |
|    54294 |   726 | `		return SXRET_OK;` |
|        - |   727 | `	}` |
|        - |   728 | `	/* Create constructor alias if not yet done */` |
|    71070 |   729 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   730 | `		/* User constructor with the same base class name */` |
|     5392 |   731 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5392 |   732 | `		if( pEntry ){` |
|      ! 0 |   733 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   734 | `			/* Create the alias */` |
|      ! 0 |   735 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   736 | `		}` |
|     2695 |   737 | `	}` |
|        - |   738 | `	/* Install the methods now */` |
|    71070 |   739 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   712942 |   740 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   606340 |   741 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   606340 |   742 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   606332 |   743 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   606332 |   744 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   745 | `				return rc;` |
|        - |   746 | `			}` |
|   303165 |   747 | `		}` |
|        2 |   748 | `	}` |
|        - |   749 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    71070 |   750 | `	pClass->bMounted = TRUE;` |
|    71070 |   751 | `	return SXRET_OK;` |
|    62682 |   752 |  |
|        - |   753 | `/*` |
|        - |   754 | ` * Allocate a private frame for attributes of the given` |
|        - |   755 | ` * class instance (Object in the PHP jargon).` |
|        - |   756 | ` */` |
|     1282 |   757 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   758 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   759 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   760 | `	)` |
|        2 |   761 |  |
|     1284 |   762 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   763 | `	ph7_class_attr *pAttr;` |
|        - |   764 | `	SyHashEntry *pEntry;` |
|        - |   765 | `	sxi32 rc;` |
|        - |   766 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1284 |   767 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     5276 |   768 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   769 | `		VmClassAttr *pVmAttr;` |
|        - |   770 | `		/* Extract the current attribute */` |
|     3994 |   771 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3994 |   772 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3994 |   773 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   774 | `			return SXERR_MEM;` |
|        - |   775 | `		}` |
|     3994 |   776 | `		pVmAttr->pAttr = pAttr;` |
|     3994 |   777 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   778 | `			ph7_value *pMemObj;` |
|        - |   779 | `			/* Reserve a memory object for this attribute */` |
|     3970 |   780 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3970 |   781 | `			if( pMemObj == 0 ){` |
|      ! 0 |   782 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   783 | `				return SXERR_MEM;` |
|        - |   784 | `			}` |
|     3970 |   785 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3970 |   786 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   787 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   788 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   789 | `			}` |
|     3970 |   790 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3970 |   791 | `			if( rc != SXRET_OK ){` |
|        - |   792 | `				VmSlot sSlot;` |
|        - |   793 | `				/* Restore memory object */` |
|      ! 0 |   794 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   795 | `				sSlot.pUserData = 0;` |
|      ! 0 |   796 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   797 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   798 | `				return SXERR_MEM;` |
|        - |   799 | `			}` |
|        - |   800 | `			/* Install attribute in the reference table */` |
|     3970 |   801 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1986 |   802 | `		}else{` |
|        - |   803 | `			/* Install static/constant attribute */` |
|       26 |   804 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   805 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   806 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|        - |   810 | `		}` |
|        2 |   811 | `	}` |
|     1284 |   812 | `	return SXRET_OK;` |
|      643 |   813 |  |
|        - |   814 | `/* Forward declaration */` |
|        - |   815 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   816 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   817 | `/*` |
|        - |   818 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   819 | ` */` |
|        - |   820 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a constant memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|   378314 |   825 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|   378316 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|   370348 |   831 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   185173 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|   378316 |   834 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   378316 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|   378316 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   378316 |   842 | `	return pObj;` |
|   189159 |   843 |  |
|        - |   844 | `/*` |
|        - |   845 | ` * Reserve a memory object.` |
|        - |   846 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   847 | ` */` |
|  2143304 |   848 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   849 |  |
|        - |   850 | `	ph7_value *pObj;` |
|        - |   851 | `	sxi32 rc;` |
|  2143306 |   852 | `	if( pIndex ){` |
|        - |   853 | `		/* Object index in the object table */` |
|  2143306 |   854 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071652 |   855 | `	}` |
|        - |   856 | `	/* Reserve a slot for the new object */` |
|  2143306 |   857 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2143306 |   858 | `	if( rc != SXRET_OK ){` |
|        - |   859 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   860 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   861 | `		 */` |
|      ! 0 |   862 | `		return 0;` |
|        - |   863 | `	}` |
|  2143306 |   864 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2143306 |   865 | `	return pObj;` |
|  1071654 |   866 |  |
|        - |   867 | `/* Forward declaration */` |
|        - |   868 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   869 | `/* Forward declarations for Fiber C functions */` |
|        - |   870 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   871 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   872 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   873 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   874 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   875 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   876 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   877 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   878 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   879 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   880 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   881 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   882 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   883 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   884 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   885 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   886 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   887 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   888 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   889 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   890 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   891 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   892 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   893 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   894 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   895 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   896 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   897 | `/*` |
|        - |   898 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   899 | ` * directly as foreign functions.` |
|        - |   900 | ` */` |
|        - |   901 | `#define PH7_BUILTIN_LIB \` |
|        - |   902 | `	"class Exception { "\` |
|        - |   903 | `    "protected $message = 'Unknown exception';"\` |
|        - |   904 | `    "protected $code = 0;"\` |
|        - |   905 | `    "protected $file;"\` |
|        - |   906 | `    "protected $line;"\` |
|        - |   907 | `    "protected $trace;"\` |
|        - |   908 | `    "protected $previous;"\` |
|        - |   909 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   910 | `	"   if( isset($message) ){"\` |
|        - |   911 | `	"	  $this->message = $message;"\` |
|        - |   912 | `	"   }"\` |
|        - |   913 | `	"   $this->code = $code;"\` |
|        - |   914 | `	"   $this->file = __FILE__;"\` |
|        - |   915 | `	"   $this->line = __LINE__;"\` |
|        - |   916 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   917 | `	"   if( isset($previous) ){"\` |
|        - |   918 | `	"     $this->previous = $previous;"\` |
|        - |   919 | `	"   }"\` |
|        - |   920 | `	"}"\` |
|        - |   921 | `	"public function getMessage(){"\` |
|        - |   922 | `	"   return $this->message;"\` |
|        - |   923 | `	"}"\` |
|        - |   924 | `	" public function getCode(){"\` |
|        - |   925 | `	"  return $this->code;"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"public function getFile(){"\` |
|        - |   928 | `	"  return $this->file;"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"public function getLine(){"\` |
|        - |   931 | `	"  return $this->line;"\` |
|        - |   932 | `	"}"\` |
|        - |   933 | `	"public function getTrace(){"\` |
|        - |   934 | `	"   return $this->trace;"\` |
|        - |   935 | `	"}"\` |
|        - |   936 | `	"public function getTraceAsString(){"\` |
|        - |   937 | `	"  return debug_string_backtrace();"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function getPrevious(){"\` |
|        - |   940 | `	"    return $this->previous;"\` |
|        - |   941 | `	"}"\` |
|        - |   942 | `	"public function __toString(){"\` |
|        - |   943 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   944 | `    "}"\` |
|        - |   945 | `	"}"\` |
|        - |   946 | `	"class Error extends Exception { }"\` |
|        - |   947 | `	"class TypeError extends Error { }"\` |
|        - |   948 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   949 | `	"class ValueError extends Error { }"\` |
|        - |   950 | `	"class FiberError extends Error { }"\` |
|        - |   951 | `	"class AssertionError extends Error { }"\` |
|        - |   952 | `	"class ArithmeticError extends Error { }"\` |
|        - |   953 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |   954 | `	"class ErrorException extends Exception { "\` |
|        - |   955 | `	"protected $severity;"\` |
|        - |   956 | `	"public function __construct(string $message = null,"\` |
|        - |   957 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   958 | `	"   if( isset($message) ){"\` |
|        - |   959 | `	"	  $this->message = $message;"\` |
|        - |   960 | `	"   }"\` |
|        - |   961 | `	"   $this->severity = $severity;"\` |
|        - |   962 | `	"   $this->code = $code;"\` |
|        - |   963 | `	"   $this->file = $filename;"\` |
|        - |   964 | `	"   $this->line = $lineno;"\` |
|        - |   965 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   966 | `	"   if( isset($previous) ){"\` |
|        - |   967 | `	"     $this->previous = $previous;"\` |
|        - |   968 | `	"   }"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"public function getSeverity(){"\` |
|        - |   971 | `	"   return $this->severity;"\` |
|        - |   972 | `    "}"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"interface Iterator {"\` |
|        - |   975 | `	"public function current();"\` |
|        - |   976 | `	"public function key();"\` |
|        - |   977 | `	"public function next();"\` |
|        - |   978 | `	"public function rewind();"\` |
|        - |   979 | `	"public function valid();"\` |
|        - |   980 | `	"}"\` |
|        - |   981 | `	"interface IteratorAggregate {"\` |
|        - |   982 | `	"public function getIterator();"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"interface Serializable {"\` |
|        - |   985 | `	"public function serialize();"\` |
|        - |   986 | `	"public function unserialize(string $serialized);"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"/* Directory releated IO */"\` |
|        - |   989 | `	"class Directory {"\` |
|        - |   990 | `	"public $handle = null;"\` |
|        - |   991 | `	"public $path  = null;"\` |
|        - |   992 | `	"public function __construct(string $path)"\` |
|        - |   993 | `	"{"\` |
|        - |   994 | `	"   $this->handle = opendir($path);"\` |
|        - |   995 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   996 | `	"      $this->path = $path;"\` |
|        - |   997 | `	"   }"\` |
|        - |   998 | `	"}"\` |
|        - |   999 | `	"public function __destruct()"\` |
|        - |  1000 | `	"{"\` |
|        - |  1001 | `	"  if( $this->handle != null ){"\` |
|        - |  1002 | `	"       closedir($this->handle);"\` |
|        - |  1003 | `	"  }"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"public function read()"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"    return readdir($this->handle);"\` |
|        - |  1008 | `	"}"\` |
|        - |  1009 | `	"public function rewind()"\` |
|        - |  1010 | `	"{"\` |
|        - |  1011 | `	"    rewinddir($this->handle);"\` |
|        - |  1012 | `	"}"\` |
|        - |  1013 | `	"public function close()"\` |
|        - |  1014 | `	"{"\` |
|        - |  1015 | `	"    closedir($this->handle);"\` |
|        - |  1016 | `	"    $this->handle = null;"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"}"\` |
|        - |  1019 | `	"class Fiber {"\` |
|        - |  1020 | `	"  private $__ctx;"\` |
|        - |  1021 | `	"  private $__callable;"\` |
|        - |  1022 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1023 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1024 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1025 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1026 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1027 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1028 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1029 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1030 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1031 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1032 | `	"}"\` |
|        - |  1033 | `	"class Generator implements Iterator {"\` |
|        - |  1034 | `	"  private $__ctx;"\` |
|        - |  1035 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1036 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1037 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1038 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1039 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1040 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1041 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1042 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1043 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1044 | `	"}"\` |
|        - |  1045 | `	"class stdClass{"\` |
|        - |  1046 | `	"  public $value;"\` |
|        - |  1047 | `	" /* Magic methods */"\` |
|        - |  1048 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1049 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1050 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1051 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1052 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1053 | `	"}"\` |
|        - |  1054 | `	"function dir(string $path){"\` |
|        - |  1055 | `	"   return new Directory($path);"\` |
|        - |  1056 | `	"}"\` |
|        - |  1057 | `	"function Dir(string $path){"\` |
|        - |  1058 | `	"   return new Directory($path);"\` |
|        - |  1059 | `	"}"\` |
|        - |  1060 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1061 | `    "{"\` |
|        - |  1062 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1063 | `	"  $aDir = array();"\` |
|        - |  1064 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1065 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1066 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1067 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1068 | `	"   }"\` |
|        - |  1069 | `	"  closedir($pHandle);"\` |
|        - |  1070 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1071 | `	"      rsort($aDir);"\` |
|        - |  1072 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1073 | `	"      sort($aDir);"\` |
|        - |  1074 | `	"  }"\` |
|        - |  1075 | `	"  return $aDir;"\` |
|        - |  1076 | `	"}"\` |
|        - |  1077 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1078 | `	"/* Open the target directory */"\` |
|        - |  1079 | `	"$zDir = dirname($pattern);"\` |
|        - |  1080 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1081 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1082 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1083 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1084 | `	"	return FALSE;"\` |
|        - |  1085 | `	"}"\` |
|        - |  1086 | `	"$pattern = basename($pattern);"\` |
|        - |  1087 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1088 | `	"/* Loop throw available entries */"\` |
|        - |  1089 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1090 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1091 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1092 | `	"	if( $rc ){"\` |
|        - |  1093 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1094 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1095 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1096 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1097 | `	"		  }"\` |
|        - |  1098 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1099 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1100 | `	"		 continue;"\` |
|        - |  1101 | `	"	   }"\` |
|        - |  1102 | `	"	   /* Add the entry */"\` |
|        - |  1103 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1104 | `	"	}"\` |
|        - |  1105 | `	" }"\` |
|        - |  1106 | `	"/* Close the handle */"\` |
|        - |  1107 | `	"closedir($pHandle);"\` |
|        - |  1108 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1109 | `	"  /* Sort the array */"\` |
|        - |  1110 | `	"  sort($pArray);"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1113 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1114 | `	"  $pArray[] = $pattern;"\` |
|        - |  1115 | `	"}"\` |
|        - |  1116 | `	"/* Return the created array */"\` |
|        - |  1117 | `	"return $pArray;"\` |
|        - |  1118 | `   "}"\` |
|        - |  1119 | `   "/* Creates a temporary file */"\` |
|        - |  1120 | `   "function tmpfile(){"\` |
|        - |  1121 | `   "  /* Extract the temp directory */"\` |
|        - |  1122 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1123 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1124 | `   "    /* Use the current dir */"\` |
|        - |  1125 | `   "    $zTempDir = '.';"\` |
|        - |  1126 | `   "  }"\` |
|        - |  1127 | `   "  /* Create the file */"\` |
|        - |  1128 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1129 | `   "  return $pHandle;"\` |
|        - |  1130 | `   "}"\` |
|        - |  1131 | `   "/* Creates a temporary filename */"\` |
|        - |  1132 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1133 | `   "{"\` |
|        - |  1134 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1135 | `   "}"\` |
|        - |  1136 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1137 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1138 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1139 | `   "/* Copy arguments */"\` |
|        - |  1140 | `   "$nArgs = func_num_args();"\` |
|        - |  1141 | `   "$pNew = array();"\` |
|        - |  1142 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1143 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1144 | `    "}"\` |
|        - |  1145 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1146 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1147 | `	"/* Erase */"\` |
|        - |  1148 | `	"array_erase($pArray);"\` |
|        - |  1149 | `	"/* Unshift */"\` |
|        - |  1150 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1151 | `	"return sizeof($pArray);"\` |
|        - |  1152 | `    "}"\` |
|        - |  1153 | `	"function array_merge_recursive(){"\` |
|        - |  1154 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1155 | `    "$arrays = func_get_args();"\` |
|        - |  1156 | `    "$narrays = count($arrays);"\` |
|        - |  1157 | `    "$ret = array();"\` |
|        - |  1158 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1159 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1160 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1161 | `	 " }"\` |
|        - |  1162 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1163 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1164 | `     "  if( $keyIsInt ) {"\` |
|        - |  1165 | `     "   $ret[] = $value;"\` |
|        - |  1166 | `     "  } else {"\` |
|        - |  1167 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1168 | `     "    $cur = $ret[$key];"\` |
|        - |  1169 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1170 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1171 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1172 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1173 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1174 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1175 | `     "    } else {"\` |
|        - |  1176 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1177 | `     "    }"\` |
|        - |  1178 | `     "   } else {"\` |
|        - |  1179 | `     "    $ret[$key] = $value;"\` |
|        - |  1180 | `     "   }"\` |
|        - |  1181 | `     "  }"\` |
|        - |  1182 | `     " }"\` |
|        - |  1183 | `	 " }"\` |
|        - |  1184 | `	 " return $ret;"\` |
|        - |  1185 | `    "}"\` |
|        - |  1186 | `	"function max(){"\` |
|        - |  1187 | `    "  $pArgs = func_get_args();"\` |
|        - |  1188 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1189 | `	"  return null;"\` |
|        - |  1190 | `    " }"\` |
|        - |  1191 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1192 | `    " $pArg = $pArgs[0];"\` |
|        - |  1193 | `	" if( !is_array($pArg) ){"\` |
|        - |  1194 | `	"   return $pArg; "\` |
|        - |  1195 | `	" }"\` |
|        - |  1196 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1197 | `	"   return null;"\` |
|        - |  1198 | `	" }"\` |
|        - |  1199 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1200 | `	" reset($pArg);"\` |
|        - |  1201 | `	" $max = current($pArg);"\` |
|        - |  1202 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1203 | `	"   if( $val > $max ){"\` |
|        - |  1204 | `	"     $max = $val;"\` |
|        - |  1205 | `    " }"\` |
|        - |  1206 | `	" }"\` |
|        - |  1207 | `	" return $max;"\` |
|        - |  1208 | `    " }"\` |
|        - |  1209 | `    " $max = $pArgs[0];"\` |
|        - |  1210 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1211 | `    " $val = $pArgs[$i];"\` |
|        - |  1212 | `	"if( $val > $max ){"\` |
|        - |  1213 | `	" $max = $val;"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `    " }"\` |
|        - |  1216 | `	" return $max;"\` |
|        - |  1217 | `    "}"\` |
|        - |  1218 | `	"function min(){"\` |
|        - |  1219 | `    "  $pArgs = func_get_args();"\` |
|        - |  1220 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1221 | `	"  return null;"\` |
|        - |  1222 | `    " }"\` |
|        - |  1223 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1224 | `    " $pArg = $pArgs[0];"\` |
|        - |  1225 | `	" if( !is_array($pArg) ){"\` |
|        - |  1226 | `	"   return $pArg; "\` |
|        - |  1227 | `	" }"\` |
|        - |  1228 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1229 | `	"   return null;"\` |
|        - |  1230 | `	" }"\` |
|        - |  1231 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1232 | `	" reset($pArg);"\` |
|        - |  1233 | `	" $min = current($pArg);"\` |
|        - |  1234 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1235 | `	"   if( $val < $min ){"\` |
|        - |  1236 | `	"     $min = $val;"\` |
|        - |  1237 | `    " }"\` |
|        - |  1238 | `	" }"\` |
|        - |  1239 | `	" return $min;"\` |
|        - |  1240 | `    " }"\` |
|        - |  1241 | `    " $min = $pArgs[0];"\` |
|        - |  1242 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1243 | `    " $val = $pArgs[$i];"\` |
|        - |  1244 | `	"if( $val < $min ){"\` |
|        - |  1245 | `	" $min = $val;"\` |
|        - |  1246 | `	" }"\` |
|        - |  1247 | `    " }"\` |
|        - |  1248 | `	" return $min;"\` |
|        - |  1249 | `	"}"\` |
|        - |  1250 | `	"function fileowner(string $file){"\` |
|        - |  1251 | `    " $a = stat($file);"\` |
|        - |  1252 | `	" if( !is_array($a) ){"\` |
|        - |  1253 | `	"	return false;"\` |
|        - |  1254 | `	" }"\` |
|        - |  1255 | `	" return $a['uid'];"\` |
|        - |  1256 | `    "}"\` |
|        - |  1257 | `    "function filegroup(string $file){"\` |
|        - |  1258 | `	" $a = stat($file);"\` |
|        - |  1259 | `	" if( !is_array($a) ){"\` |
|        - |  1260 | `	"	return false;"\` |
|        - |  1261 | `	" }"\` |
|        - |  1262 | `	" return $a['gid'];"\` |
|        - |  1263 | `    "}"\` |
|        - |  1264 | `	 "function fileinode(string $file){"\` |
|        - |  1265 | `	" $a = stat($file);"\` |
|        - |  1266 | `	" if( !is_array($a) ){"\` |
|        - |  1267 | `	"	return false;"\` |
|        - |  1268 | `	" }"\` |
|        - |  1269 | `	" return $a['ino'];"\` |
|        - |  1270 | `    "}"` |
|        - |  1271 |  |
|        - |  1272 | `/*` |
|        - |  1273 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1274 | ` * start compiling the target PHP program.` |
|        - |  1275 | ` */` |
|     2656 |  1276 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1277 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1278 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1279 | `	 )` |
|        2 |  1280 |  |
|        - |  1281 | `	SyString sBuiltin;` |
|        - |  1282 | `	ph7_value *pObj;` |
|        - |  1283 | `	sxi32 rc;` |
|        - |  1284 | `	/* Zero the structure */` |
|     2658 |  1285 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1286 | `	/* Initialize VM fields */` |
|     2658 |  1287 | `	pVm->pEngine = &(*pEngine);` |
|     2658 |  1288 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1289 | `	/* Instructions containers */` |
|     2658 |  1290 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2658 |  1291 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2658 |  1292 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1293 | `	/* Object containers */` |
|     2658 |  1294 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2658 |  1295 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1296 | `	/* Virtual machine internal containers */` |
|     2658 |  1297 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2658 |  1298 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2658 |  1299 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2658 |  1300 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2658 |  1301 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2658 |  1302 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2658 |  1303 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2658 |  1304 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2658 |  1305 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2658 |  1306 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2658 |  1307 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2658 |  1308 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2658 |  1309 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2658 |  1310 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2658 |  1311 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2658 |  1312 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2658 |  1313 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2658 |  1314 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2658 |  1315 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2658 |  1316 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2658 |  1317 | `	pVm->pPendingException = 0;` |
|        - |  1318 | `	/* Configuration containers */` |
|     2658 |  1319 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2658 |  1320 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2658 |  1321 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2658 |  1322 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2658 |  1323 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2658 |  1324 | `	pVm->iResponseStatus = 200;` |
|     2658 |  1325 | `	pVm->bHeadersSent = 0;` |
|     2658 |  1326 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1327 | `	/* Error callbacks containers */` |
|     2658 |  1328 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2658 |  1329 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2658 |  1330 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2658 |  1331 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2658 |  1332 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1333 | `	/* Set a default recursion limit */` |
|        - |  1334 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2658 |  1335 | `	pVm->nMaxDepth = 32;` |
|        - |  1336 | `#else` |
|        - |  1337 | `	pVm->nMaxDepth = 16;` |
|        - |  1338 | `#endif` |
|        - |  1339 | `	/* Default assertion flags */` |
|     2658 |  1340 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1341 | `	/* JSON return status */` |
|     2658 |  1342 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1343 | `	/* PRNG context */` |
|     2658 |  1344 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1345 | `	/* Install the null constant */` |
|     2658 |  1346 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2658 |  1347 | `	if( pObj == 0 ){` |
|      ! 0 |  1348 | `		rc = SXERR_MEM;` |
|      ! 0 |  1349 | `		goto Err;` |
|        - |  1350 | `	}` |
|     2658 |  1351 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1352 | `	/* Install the boolean TRUE constant */` |
|     2658 |  1353 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2658 |  1354 | `	if( pObj == 0 ){` |
|      ! 0 |  1355 | `		rc = SXERR_MEM;` |
|      ! 0 |  1356 | `		goto Err;` |
|        - |  1357 | `	}` |
|     2658 |  1358 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1359 | `	/* Install the boolean FALSE constant */` |
|     2658 |  1360 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2658 |  1361 | `	if( pObj == 0 ){` |
|      ! 0 |  1362 | `		rc = SXERR_MEM;` |
|      ! 0 |  1363 | `		goto Err;` |
|        - |  1364 | `	}` |
|     2658 |  1365 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1366 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1367 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1368 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2658 |  1369 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2658 |  1370 | `	if( pObj == 0 ){` |
|      ! 0 |  1371 | `		rc = SXERR_MEM;` |
|      ! 0 |  1372 | `		goto Err;` |
|        - |  1373 | `	}` |
|     2658 |  1374 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1375 | `	/* Create the global frame */` |
|     2658 |  1376 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2658 |  1377 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1378 | `		goto Err;` |
|        - |  1379 | `	}` |
|        - |  1380 | `	/* Initialize the code generator */` |
|     2658 |  1381 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2658 |  1382 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1383 | `		goto Err;` |
|        - |  1384 | `	}` |
|        - |  1385 | `	/* VM correctly initialized,set the magic number */` |
|     2658 |  1386 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2658 |  1387 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1388 | `	/* Compile the built-in library */` |
|     2658 |  1389 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1390 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2658 |  1391 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1392 | `	/* Register Fiber internal C functions */` |
|     2658 |  1393 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2658 |  1394 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2658 |  1395 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2658 |  1396 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2658 |  1397 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2658 |  1398 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2658 |  1399 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2658 |  1400 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2658 |  1401 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2658 |  1402 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1403 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2658 |  1404 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2658 |  1405 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2658 |  1406 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2658 |  1407 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2658 |  1408 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2658 |  1409 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2658 |  1410 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2658 |  1411 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2658 |  1412 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2658 |  1413 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1414 | `	/* Reset the code generator */` |
|     2658 |  1415 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2658 |  1416 | `	return SXRET_OK;` |
|      ! 0 |  1417 | `Err:` |
|      ! 0 |  1418 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1419 | `	return rc;` |
|     1330 |  1420 |  |
|        - |  1421 | `/*` |
|        - |  1422 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1423 | ` * routine which store the output in an internal blob.` |
|        - |  1424 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1425 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1426 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1427 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1428 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1429 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1430 | ` * to finish executing and extracting the output.` |
|        - |  1431 | ` */` |
|       38 |  1432 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1433 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1434 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1435 | `	void *pUserData     /* User private data */` |
|        - |  1436 | `	)` |
|      ! 0 |  1437 |  |
|        - |  1438 | `	 sxi32 rc;` |
|        - |  1439 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1440 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1441 | `	 return rc;` |
|      ! 0 |  1442 |  |
|        - |  1443 | `/*` |
|        - |  1444 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1445 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1446 | ` */` |
|    14698 |  1447 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1448 |  |
|    14700 |  1449 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14700 |  1450 | `	if( xCons != VmObConsumer ){` |
|     6444 |  1451 | `		pVm->nOutputLen += nLen;` |
|     6444 |  1452 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      836 |  1453 | `			pVm->bHeadersSent = 1;` |
|      417 |  1454 | `		}` |
|     3221 |  1455 | `	}` |
|    14700 |  1456 |  |
|        - |  1457 | `#define VM_STACK_GUARD 16` |
|        - |  1458 | `/*` |
|        - |  1459 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1460 | ` * our compiled PHP program.` |
|        - |  1461 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1462 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1463 | ` */` |
|    34064 |  1464 | `static ph7_value * VmNewOperandStack(` |
|        - |  1465 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1466 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1467 | `	)` |
|        2 |  1468 |  |
|        - |  1469 | `	ph7_value *pStack;` |
|        - |  1470 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1471 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1472 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1473 | `  ** on the maximum stack depth required.` |
|        - |  1474 | `  **` |
|        - |  1475 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1476 | `  */` |
|    34066 |  1477 | `	nInstr += VM_STACK_GUARD;` |
|    34066 |  1478 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    34066 |  1479 | `	if( pStack == 0 ){` |
|      ! 0 |  1480 | `		return 0;` |
|        - |  1481 | `	}` |
|        - |  1482 | `	/* Initialize the operand stack */` |
|  2124488 |  1483 | `	while( nInstr > 0 ){` |
|  2090424 |  1484 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2090424 |  1485 | `		--nInstr;` |
|        2 |  1486 | `	}` |
|        - |  1487 | `	/* Ready for bytecode execution */` |
|    34066 |  1488 | `	return pStack;` |
|    17034 |  1489 |  |
|        - |  1490 | `/* Forward declaration */` |
|        - |  1491 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1492 | `/*` |
|        - |  1493 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1494 | ` * This routine gets called by the PH7 engine after` |
|        - |  1495 | ` * successful compilation of the target PHP program.` |
|        - |  1496 | ` */` |
|     2390 |  1497 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1498 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1499 | `	)` |
|        2 |  1500 |  |
|        - |  1501 | `	SyHashEntry *pEntry;` |
|        - |  1502 | `	sxi32 rc;` |
|     2392 |  1503 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1504 | `		/* Initialize your VM first */` |
|      ! 0 |  1505 | `		return SXERR_CORRUPT;` |
|        - |  1506 | `	}` |
|        - |  1507 | `	/* Mark the VM ready for byte-code execution */` |
|     2392 |  1508 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1509 | `	/* Release the code generator now we have compiled our program */` |
|     2392 |  1510 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1511 | `	/* Emit the DONE instruction */` |
|     2392 |  1512 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2392 |  1513 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1514 | `		return SXERR_MEM;` |
|        - |  1515 | `	}` |
|        - |  1516 | `	/* Script return value */` |
|     2392 |  1517 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1518 | `	/* Allocate a new operand stack */` |
|     2392 |  1519 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2392 |  1520 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1521 | `		return SXERR_MEM;` |
|        - |  1522 | `	}` |
|        - |  1523 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1524 | `	 * private data. */` |
|     2392 |  1525 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2392 |  1526 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1527 | `	/* Allocate the reference table */` |
|     2392 |  1528 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2392 |  1529 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2392 |  1530 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1531 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1532 | `		return SXERR_MEM;` |
|        - |  1533 | `	}` |
|        - |  1534 | `	/* Zero the reference table */` |
|     2392 |  1535 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1536 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2392 |  1537 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2392 |  1538 | `	if( rc != SXRET_OK ){` |
|        - |  1539 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1540 | `		return rc;` |
|        - |  1541 | `	}` |
|        - |  1542 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2392 |  1543 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2392 |  1544 | `	if( rc != SXRET_OK ){` |
|        - |  1545 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1546 | `		return rc;` |
|        - |  1547 | `	}` |
|        - |  1548 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2392 |  1549 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1550 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2392 |  1551 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1552 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2392 |  1553 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1554 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1555 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2392 |  1556 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2392 |  1557 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1558 | `#endif` |
|        - |  1559 | `	/* Initialize and install static and constants class attributes */` |
|     2392 |  1560 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    43204 |  1561 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    40814 |  1562 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    40814 |  1563 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1564 | `			return rc;` |
|        - |  1565 | `		}` |
|        2 |  1566 | `	}` |
|        - |  1567 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2392 |  1568 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1569 | `	/* VM is ready for bytecode execution */` |
|     2392 |  1570 | `	return SXRET_OK;` |
|     1197 |  1571 |  |
|        - |  1572 | `/*` |
|        - |  1573 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1574 | ` */` |
|      ! 0 |  1575 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1576 |  |
|      ! 0 |  1577 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1578 | `		return SXERR_CORRUPT;` |
|        - |  1579 | `	}` |
|        - |  1580 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1581 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1582 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1583 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1584 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1585 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1586 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1587 | `	pVm->bHttpContext = 0;` |
|        - |  1588 | `	/* Set the ready flag */` |
|      ! 0 |  1589 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1590 | `	return SXRET_OK;` |
|      ! 0 |  1591 |  |
|        - |  1592 | `/*` |
|        - |  1593 | ` * Release a Virtual Machine.` |
|        - |  1594 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1595 | ` */` |
|     2382 |  1596 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1597 |  |
|        - |  1598 | `	/* Set the stale magic number */` |
|     2384 |  1599 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1600 | `	/* Release the private memory subsystem */` |
|     2384 |  1601 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2384 |  1602 | `	return SXRET_OK;` |
|        2 |  1603 |  |
|        - |  1604 | `/*` |
|        - |  1605 | ` * Initialize a foreign function call context.` |
|        - |  1606 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1607 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1608 | ` * functions.` |
|        - |  1609 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1610 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1611 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1612 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1613 | ` */` |
|   596774 |  1614 | `static sxi32 VmInitCallContext(` |
|        - |  1615 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1616 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1617 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1618 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1619 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1620 | `	)` |
|        2 |  1621 |  |
|   596776 |  1622 | `	pOut->pFunc = pFunc;` |
|   596776 |  1623 | `	pOut->pVm   = pVm;` |
|   596776 |  1624 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   596776 |  1625 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1626 | `	/* Assume a null return value */` |
|   596776 |  1627 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   596776 |  1628 | `	pOut->pRet = pRet;` |
|   596776 |  1629 | `	pOut->iFlags = iFlags;` |
|   596776 |  1630 | `	return SXRET_OK;` |
|        2 |  1631 |  |
|        - |  1632 | `/*` |
|        - |  1633 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1634 | ` * left behind.` |
|        - |  1635 | ` */` |
|   596774 |  1636 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1637 |  |
|        - |  1638 | `	sxu32 n;` |
|   596776 |  1639 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7254 |  1640 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20760 |  1641 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13508 |  1642 | `			if( apObj[n] == 0 ){` |
|        - |  1643 | `				/* Already released */` |
|      298 |  1644 | `				continue;` |
|        - |  1645 | `			}` |
|    13212 |  1646 | `			PH7_MemObjRelease(apObj[n]);` |
|    13212 |  1647 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6607 |  1648 | `		}` |
|     7254 |  1649 | `		SySetRelease(&pCtx->sVar);` |
|     3626 |  1650 | `	}` |
|   596776 |  1651 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1652 | `		ph7_aux_data *aAux;` |
|        - |  1653 | `		void *pChunk;` |
|        - |  1654 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1655 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1656 | `		 */` |
|        9 |  1657 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1658 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1659 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1660 | `			/* Release the chunk */` |
|       25 |  1661 | `			if( pChunk ){` |
|       25 |  1662 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1663 | `			}` |
|       13 |  1664 | `		}` |
|        9 |  1665 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1666 | `	}` |
|   596776 |  1667 |  |
|        - |  1668 | `/*` |
|        - |  1669 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1670 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1671 | ` */` |
|      296 |  1672 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1673 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1674 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1675 | `	)` |
|        2 |  1676 |  |
|      298 |  1677 | `	if( pValue == 0 ){` |
|        - |  1678 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1679 | `		return;` |
|        - |  1680 | `	}` |
|      298 |  1681 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1682 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1683 | `		sxu32 n;` |
|     1054 |  1684 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1685 | `			if( apObj[n] == pValue ){` |
|      298 |  1686 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1687 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1688 | `				/* Mark as released */` |
|      298 |  1689 | `				apObj[n] = 0;` |
|      298 |  1690 | `				break;` |
|        - |  1691 | `			}` |
|      380 |  1692 | `		}` |
|      148 |  1693 | `	}` |
|      150 |  1694 |  |
|        - |  1695 | `/*` |
|        - |  1696 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1697 | ` */` |
|  3444060 |  1698 | `static void VmPopOperand(` |
|        - |  1699 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1700 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1701 | `	)` |
|        2 |  1702 |  |
|  3444062 |  1703 | `	ph7_value *pTos = *ppTos;` |
|  7324312 |  1704 | `	while( nPop > 0 ){` |
|  3880252 |  1705 | `		PH7_MemObjRelease(pTos);` |
|  3880252 |  1706 | `		pTos--;` |
|  3880252 |  1707 | `		nPop--;` |
|        2 |  1708 | `	}` |
|        - |  1709 | `	/* Top of the stack */` |
|  3444062 |  1710 | `	*ppTos = pTos;` |
|  3444062 |  1711 |  |
|        - |  1712 | `/*` |
|        - |  1713 | ` * Reserve a memory object.` |
|        - |  1714 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1715 | ` */` |
|  3079732 |  1716 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1717 |  |
|  3079734 |  1718 | `	ph7_value *pObj = 0;` |
|        - |  1719 | `	VmSlot *pSlot;` |
|        - |  1720 | `	sxu32 nIdx;` |
|        - |  1721 | `	/* Check for a free slot */` |
|  3079734 |  1722 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3079734 |  1723 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3079734 |  1724 | `	if( pSlot ){` |
|   936430 |  1725 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   936430 |  1726 | `		nIdx = pSlot->nIdx;` |
|   468214 |  1727 | `	}` |
|  3079734 |  1728 | `	if( pObj == 0 ){` |
|        - |  1729 | `		/* Reserve a new memory object */` |
|  2143306 |  1730 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2143306 |  1731 | `		if( pObj == 0 ){` |
|      ! 0 |  1732 | `			return 0;` |
|        - |  1733 | `		}` |
|  1071652 |  1734 | `	}` |
|        - |  1735 | `	/* Set a null default value */` |
|  3079734 |  1736 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3079734 |  1737 | `	pObj->nIdx = nIdx;` |
|  3079734 |  1738 | `	return pObj;` |
|  1539868 |  1739 |  |
|        - |  1740 | `/*` |
|        - |  1741 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1742 | ` */` |
|    30852 |  1743 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1744 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1745 | `	const char *zKey,  /* Entry key */` |
|        - |  1746 | `	sxu32 nByte,       /* Key length */` |
|        - |  1747 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1748 | `	)` |
|        2 |  1749 |  |
|        - |  1750 | `	ph7_value sKey;` |
|        - |  1751 | `	sxi32 rc;` |
|    30854 |  1752 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30854 |  1753 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1754 | `	/* Perform the insertion */` |
|    30854 |  1755 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30854 |  1756 | `	PH7_MemObjRelease(&sKey);` |
|    30854 |  1757 | `	return rc;` |
|        2 |  1758 |  |
|        - |  1759 | `/*` |
|        - |  1760 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1761 | ` * Return a pointer to the variable value on success.` |
|        - |  1762 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1763 | ` */` |
|  3206860 |  1764 | `static ph7_value * VmExtractMemObj(` |
|        - |  1765 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1766 | `	const SyString *pName, /* Variable name */` |
|        - |  1767 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1768 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1769 | `	)` |
|        2 |  1770 |  |
|  3206862 |  1771 | `	int bNullify = FALSE;` |
|        - |  1772 | `	SyHashEntry *pEntry;` |
|        - |  1773 | `	VmFrame *pFrame;` |
|        - |  1774 | `	ph7_value *pObj;` |
|        - |  1775 | `	sxu32 nIdx;` |
|        - |  1776 | `	sxi32 rc;` |
|        - |  1777 | `	/* Point to the top active frame */` |
|  3206862 |  1778 | `	pFrame = pVm->pFrame;` |
|  3206862 |  1779 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1780 | `	/* Perform the lookup */` |
|  3206862 |  1781 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1782 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1783 | `		pName = &sAnnon;` |
|        - |  1784 | `		/* Always nullify the object */` |
|      ! 0 |  1785 | `		bNullify = TRUE;` |
|      ! 0 |  1786 | `		bDup = FALSE;` |
|      ! 0 |  1787 | `	}` |
|        - |  1788 | `	/* Check the superglobals table first */` |
|  3206862 |  1789 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3206862 |  1790 | `	if( pEntry == 0 ){` |
|        - |  1791 | `		/* Query the top active frame */` |
|  3206822 |  1792 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3206822 |  1793 | `		if( pEntry == 0 ){` |
|    88918 |  1794 | `			char *zName = (char *)pName->zString;` |
|        - |  1795 | `			VmSlot sLocal;` |
|    88918 |  1796 | `			if( !bCreate ){` |
|        - |  1797 | `				/* Do not create the variable,return NULL instead */` |
|       42 |  1798 | `				return 0;` |
|        - |  1799 | `			}` |
|        - |  1800 | `			/* No such variable,automatically create a new one and install` |
|        - |  1801 | `			 * it in the current frame.` |
|        - |  1802 | `			 */` |
|    88878 |  1803 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    88878 |  1804 | `			if( pObj == 0 ){` |
|      ! 0 |  1805 | `				return 0;` |
|        - |  1806 | `			}` |
|    88878 |  1807 | `			nIdx = pObj->nIdx;` |
|    88878 |  1808 | `			if( bDup ){` |
|        - |  1809 | `				/* Duplicate name */` |
|      168 |  1810 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1811 | `				if( zName == 0 ){` |
|      ! 0 |  1812 | `					return 0;` |
|        - |  1813 | `				}` |
|       83 |  1814 | `			}` |
|        - |  1815 | `			/* Link to the top active VM frame */` |
|    88878 |  1816 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    88878 |  1817 | `			if( rc != SXRET_OK ){` |
|        - |  1818 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1819 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1820 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1821 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1822 | `				return 0;` |
|        - |  1823 | `			}` |
|    88878 |  1824 | `			if( pFrame->pParent != 0 ){` |
|        - |  1825 | `				/* Local variable */` |
|    81952 |  1826 | `				sLocal.nIdx = nIdx;` |
|    81952 |  1827 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    40977 |  1828 | `			}else{` |
|        - |  1829 | `				/* Register in the $GLOBALS array */` |
|     6928 |  1830 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1831 | `			}` |
|        - |  1832 | `			/* Install in the reference table */` |
|    88878 |  1833 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1834 | `			/* Save object index */` |
|    88878 |  1835 | `			pObj->nIdx = nIdx;` |
|    44440 |  1836 | `		}else{` |
|        - |  1837 | `			/* Extract variable contents */` |
|  3117906 |  1838 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3117906 |  1839 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3117906 |  1840 | `			if( bNullify && pObj ){` |
|      ! 0 |  1841 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1842 | `			}` |
|        - |  1843 | `		}` |
|  1603502 |  1844 | `	}else{` |
|        - |  1845 | `		/* Superglobal */` |
|       42 |  1846 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1847 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1848 | `	}` |
|  3206822 |  1849 | `	return pObj;` |
|  1603542 |  1850 |  |
|        - |  1851 | `/*` |
|        - |  1852 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1853 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1854 | ` */` |
|     2694 |  1855 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1856 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1857 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1858 | `	sxu32 nByte        /* zName length */` |
|        - |  1859 | `	)` |
|        2 |  1860 |  |
|        - |  1861 | `	SyHashEntry *pEntry;` |
|        - |  1862 | `	ph7_value *pValue;` |
|        - |  1863 | `	sxu32 nIdx;` |
|        - |  1864 | `	/* Query the superglobal table */` |
|     2696 |  1865 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2696 |  1866 | `	if( pEntry == 0 ){` |
|        - |  1867 | `		/* No such entry */` |
|      ! 0 |  1868 | `		return 0;` |
|        - |  1869 | `	}` |
|        - |  1870 | `	/* Extract the superglobal index in the global object pool */` |
|     2696 |  1871 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1872 | `	/* Extract the variable value  */` |
|     2696 |  1873 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2696 |  1874 | `	return pValue;` |
|     1349 |  1875 |  |
|        - |  1876 | `/*` |
|        - |  1877 | ` * Perform a raw hashmap insertion.` |
|        - |  1878 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1879 | ` */` |
|     2724 |  1880 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1881 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1882 | `	const char *zKey,   /* Entry key */` |
|        - |  1883 | `	int nKeylen,        /* zKey length*/` |
|        - |  1884 | `	const char *zData,  /* Entry data */` |
|        - |  1885 | `	int nLen            /* zData length */` |
|        - |  1886 | `	)` |
|        2 |  1887 |  |
|        - |  1888 | `	ph7_value sKey,sValue;` |
|        - |  1889 | `	sxi32 rc;` |
|     2726 |  1890 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2726 |  1891 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2726 |  1892 | `	if( zKey ){` |
|     2704 |  1893 | `		if( nKeylen < 0 ){` |
|     2652 |  1894 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1325 |  1895 | `		}` |
|     2704 |  1896 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1351 |  1897 | `	}` |
|     2726 |  1898 | `	if( zData ){` |
|     2726 |  1899 | `		if( nLen < 0 ){` |
|        - |  1900 | `			/* Compute length automatically */` |
|      144 |  1901 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1902 | `		}` |
|     2726 |  1903 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1362 |  1904 | `	}` |
|        - |  1905 | `	/* Perform the insertion */` |
|     2726 |  1906 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2726 |  1907 | `	PH7_MemObjRelease(&sKey);` |
|     2726 |  1908 | `	PH7_MemObjRelease(&sValue);` |
|     2726 |  1909 | `	return rc;` |
|        2 |  1910 |  |
|        - |  1911 | `/*` |
|        - |  1912 | ` * Configure a working virtual machine instance.` |
|        - |  1913 | ` *` |
|        - |  1914 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1915 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1916 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1917 | ` * The second argument to this function is an integer configuration option` |
|        - |  1918 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1919 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1920 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1921 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1922 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1923 | ` */` |
|    38570 |  1924 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1925 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1926 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1927 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1928 | `	)` |
|        2 |  1929 |  |
|    38572 |  1930 | `	sxi32 rc = SXRET_OK;` |
|    38572 |  1931 | `	switch(nOp){` |
|     1187 |  1932 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2376 |  1933 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2376 |  1934 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1935 | `		/* VM output consumer callback */` |
|        - |  1936 | `#ifdef UNTRUST` |
|        - |  1937 | `		if( xConsumer == 0 ){` |
|        - |  1938 | `			rc = SXERR_CORRUPT;` |
|        - |  1939 | `			break;` |
|        - |  1940 | `		}` |
|        - |  1941 | `#endif` |
|        - |  1942 | `		/* Install the output consumer */` |
|     2376 |  1943 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2376 |  1944 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2376 |  1945 | `		break;` |
|        - |  1946 | `							   }` |
|     1195 |  1947 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1948 | `		/* Import path */` |
|        - |  1949 | `		  const char *zPath;` |
|        - |  1950 | `		  SyString sPath;` |
|     2392 |  1951 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1952 | `#if defined(UNTRUST)` |
|        - |  1953 | `		  if( zPath == 0 ){` |
|        - |  1954 | `			  rc = SXERR_EMPTY;` |
|        - |  1955 | `			  break;` |
|        - |  1956 | `		  }` |
|        - |  1957 | `#endif` |
|     2392 |  1958 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1959 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1960 | `#ifdef __WINNT__` |
|        2 |  1961 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1962 | `#endif` |
|     4782 |  1963 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1964 | `		  /* Remove leading and trailing white spaces */` |
|     2392 |  1965 | `		  SyStringFullTrim(&sPath);` |
|     2392 |  1966 | `		  if( sPath.nByte > 0 ){` |
|        - |  1967 | `			  /* Store the path in the corresponding conatiner */` |
|     2392 |  1968 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1195 |  1969 | `		  }` |
|     2392 |  1970 | `		  break;` |
|        - |  1971 | `									 }` |
|     1195 |  1972 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1973 | `		/* Run-Time Error report */` |
|     2392 |  1974 | `		pVm->bErrReport = 1;` |
|     2392 |  1975 | `		break;` |
|      ! 0 |  1976 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1977 | `		/* Recursion depth */` |
|      ! 0 |  1978 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1979 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1980 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1981 | `		}` |
|      ! 0 |  1982 | `		break;` |
|        - |  1983 | `									   }` |
|      ! 0 |  1984 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1985 | `		/* VM output length in bytes */` |
|      ! 0 |  1986 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1987 | `#ifdef UNTRUST` |
|        - |  1988 | `		if( pOut == 0 ){` |
|        - |  1989 | `			rc = SXERR_CORRUPT;` |
|        - |  1990 | `			break;` |
|        - |  1991 | `		}` |
|        - |  1992 | `#endif` |
|      ! 0 |  1993 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1994 | `		break;` |
|        - |  1995 | `							   }` |
|        - |  1996 |  |
|    11950 |  1997 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1998 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1999 | `		/* Create a new superglobal/global variable */` |
|    23902 |  2000 | `		const char *zName = va_arg(ap,const char *);` |
|    23902 |  2001 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2002 | `		SyHashEntry *pEntry;` |
|        - |  2003 | `		ph7_value *pObj;` |
|        - |  2004 | `		sxu32 nByte;` |
|        - |  2005 | `		sxu32 nIdx;` |
|        - |  2006 | `#ifdef UNTRUST` |
|        - |  2007 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2008 | `			rc = SXERR_CORRUPT;` |
|        - |  2009 | `			break;` |
|        - |  2010 | `		}` |
|        - |  2011 | `#endif` |
|    23902 |  2012 | `		nByte = SyStrlen(zName);` |
|    23902 |  2013 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2014 | `			/* Check if the superglobal is already installed */` |
|    23902 |  2015 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11952 |  2016 | `		}else{` |
|        - |  2017 | `			/* Query the top active VM frame */` |
|      ! 0 |  2018 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2019 | `		}` |
|    23902 |  2020 | `		if( pEntry ){` |
|        - |  2021 | `			/* Variable already installed */` |
|      ! 0 |  2022 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2023 | `			/* Extract contents */` |
|      ! 0 |  2024 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2025 | `			if( pObj ){` |
|        - |  2026 | `				/* Overwrite old contents */` |
|      ! 0 |  2027 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2028 | `			}` |
|      ! 0 |  2029 | `		}else{` |
|        - |  2030 | `			/* Install a new variable */` |
|    23902 |  2031 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23902 |  2032 | `			if( pObj == 0 ){` |
|      ! 0 |  2033 | `				rc = SXERR_MEM;` |
|      ! 0 |  2034 | `				break;` |
|        - |  2035 | `			}` |
|    23902 |  2036 | `			nIdx = pObj->nIdx;` |
|        - |  2037 | `			/* Copy value */` |
|    23902 |  2038 | `			PH7_MemObjStore(pValue,pObj);` |
|    23902 |  2039 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2040 | `				/* Install the superglobal */` |
|    23902 |  2041 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11952 |  2042 | `			}else{` |
|        - |  2043 | `				/* Install in the current frame */` |
|      ! 0 |  2044 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2045 | `			}` |
|    23902 |  2046 | `			if( rc == SXRET_OK ){` |
|        - |  2047 | `				SyHashEntry *pRef;` |
|    23902 |  2048 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23902 |  2049 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11952 |  2050 | `				}else{` |
|      ! 0 |  2051 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2052 | `				}` |
|        - |  2053 | `				/* Install in the reference table */` |
|    23902 |  2054 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23902 |  2055 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2056 | `					/* Register in the $GLOBALS array */` |
|    23902 |  2057 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11950 |  2058 | `				}` |
|    11950 |  2059 | `			}` |
|        - |  2060 | `		}` |
|    23902 |  2061 | `		break;` |
|        - |  2062 | `									}` |
|     1325 |  2063 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2065 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2066 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2067 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2068 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2069 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2652 |  2070 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2652 |  2071 | `		const char *zValue = va_arg(ap,const char *);` |
|     2652 |  2072 | `		int nLen = va_arg(ap,int);` |
|        - |  2073 | `		ph7_hashmap *pMap;` |
|        - |  2074 | `		ph7_value *pValue;` |
|     2652 |  2075 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2076 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2077 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2651 |  2078 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2079 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2080 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2650 |  2081 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2082 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2083 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2650 |  2084 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2085 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2086 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2650 |  2087 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2088 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2089 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2650 |  2090 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2091 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2092 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2093 | `		}else{` |
|        - |  2094 | `			/* Extract the $_SERVER superglobal */` |
|     2650 |  2095 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2096 | `		}` |
|     2652 |  2097 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2098 | `			/* No such entry */` |
|      ! 0 |  2099 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2100 | `			break;` |
|        - |  2101 | `		}` |
|        - |  2102 | `		/* Point to the hashmap */` |
|     2652 |  2103 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2104 | `		/* Perform the insertion */` |
|     2652 |  2105 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2652 |  2106 | `		break;` |
|        - |  2107 | `								   }` |
|       11 |  2108 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2109 | `		/* Script arguments */` |
|       24 |  2110 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2111 | `		ph7_hashmap *pMap;` |
|        - |  2112 | `		ph7_value *pValue;` |
|        - |  2113 | `		sxu32 n;` |
|       24 |  2114 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2115 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2116 | `			break;` |
|        - |  2117 | `		}` |
|        - |  2118 | `		/* Extract the $argv array */` |
|       24 |  2119 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2120 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2121 | `			/* No such entry */` |
|      ! 0 |  2122 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2123 | `			break;` |
|        - |  2124 | `		}` |
|        - |  2125 | `		/* Point to the hashmap */` |
|       24 |  2126 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2127 | `		/* Perform the insertion */` |
|       24 |  2128 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2129 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2130 | `		if( rc == SXRET_OK ){` |
|       24 |  2131 | `			if( pMap->nEntry > 1 ){` |
|        - |  2132 | `				/* Append space separator first */` |
|       18 |  2133 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2134 | `			}` |
|       24 |  2135 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2136 | `		}` |
|       24 |  2137 | `		break;` |
|        - |  2138 | `								  }` |
|      ! 0 |  2139 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2140 | `		/* error_log() consumer */` |
|      ! 0 |  2141 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2142 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2143 | `		break;` |
|        - |  2144 | `										}` |
|      ! 0 |  2145 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2146 | `		/* Script return value */` |
|      ! 0 |  2147 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2148 | `#ifdef UNTRUST` |
|        - |  2149 | `		if( ppValue == 0 ){` |
|        - |  2150 | `			rc = SXERR_CORRUPT;` |
|        - |  2151 | `			break;` |
|        - |  2152 | `		}` |
|        - |  2153 | `#endif` |
|      ! 0 |  2154 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2155 | `		break;` |
|        - |  2156 | `								   }` |
|     2390 |  2157 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2158 | `		/* Register an IO stream device */` |
|     4782 |  2159 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2160 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7170 |  2161 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4782 |  2162 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2163 | `				/* Invalid stream */` |
|      ! 0 |  2164 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2165 | `				break;` |
|        - |  2166 | `		}` |
|     4782 |  2167 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2168 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2392 |  2169 | `			pVm->pDefStream = pStream;` |
|     1195 |  2170 | `		}` |
|        - |  2171 | `		/* Insert in the appropriate container */` |
|     4782 |  2172 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4782 |  2173 | `		break;` |
|        - |  2174 | `								  }` |
|        8 |  2175 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2176 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2177 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2178 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2179 | `#ifdef UNTRUST` |
|        - |  2180 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2181 | `			rc = SXERR_CORRUPT;` |
|        - |  2182 | `			break;` |
|        - |  2183 | `		}` |
|        - |  2184 | `#endif` |
|       16 |  2185 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2186 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2187 | `		break;` |
|        - |  2188 | `									   }` |
|        8 |  2189 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2190 | `		/* Raw HTTP request*/` |
|       16 |  2191 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2192 | `		int nByte = va_arg(ap,int);` |
|       16 |  2193 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2194 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2195 | `			break;` |
|        - |  2196 | `		}` |
|       16 |  2197 | `		if( nByte < 0 ){` |
|        - |  2198 | `			/* Compute length automatically */` |
|      ! 0 |  2199 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2200 | `		}` |
|        - |  2201 | `		/* Process the request */` |
|       16 |  2202 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2203 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2204 | `		if( rc == SXRET_OK ){` |
|       16 |  2205 | `			pVm->bHttpContext = 1;` |
|        8 |  2206 | `		}` |
|       16 |  2207 | `		break;` |
|        - |  2208 | `									}` |
|        8 |  2209 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2210 | `		/* Extract HTTP response status code */` |
|       16 |  2211 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2212 | `		if( pStatus ){` |
|       16 |  2213 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2214 | `		}` |
|       16 |  2215 | `		break;` |
|        - |  2216 | `										}` |
|        8 |  2217 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2218 | `		/* Iterate response headers via callback */` |
|        - |  2219 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2220 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2221 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2222 | `		if( xCallback ){` |
|       16 |  2223 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2224 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2225 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2226 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2227 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2228 | `							   pUserData);` |
|       12 |  2229 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2230 | `					break;` |
|        - |  2231 | `				}` |
|        6 |  2232 | `			}` |
|        8 |  2233 | `		}` |
|       16 |  2234 | `		break;` |
|        - |  2235 | `										 }` |
|      ! 0 |  2236 | `	default:` |
|        - |  2237 | `		/* Unknown configuration option */` |
|      ! 0 |  2238 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2239 | `		break;` |
|        - |  2240 | `	}` |
|    38572 |  2241 | `	return rc;` |
|        2 |  2242 |  |
|        - |  2243 | `/* Forward declaration */` |
|        - |  2244 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2245 | `/*` |
|        - |  2246 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2247 | ` * format.` |
|        - |  2248 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2249 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2250 | ` * (STDOUT).` |
|        - |  2251 | ` */` |
|        2 |  2252 | `static sxi32 VmByteCodeDump(` |
|        - |  2253 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2254 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2255 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2256 | `	)` |
|        1 |  2257 |  |
|        - |  2258 | `	static const char zDump[] = {` |
|        - |  2259 | `		"====================================================\n"` |
|        - |  2260 | `		"PH7 VM Dump\n"` |
|        - |  2261 | `		"====================================================\n"` |
|        - |  2262 | `	};` |
|        - |  2263 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2264 | `	sxi32 rc = SXRET_OK;` |
|        - |  2265 | `	sxu32 n;` |
|        - |  2266 | `	/* Point to the PH7 instructions */` |
|        3 |  2267 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2268 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2269 | `	n = 0;` |
|        3 |  2270 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2271 | `	/* Dump instructions */` |
|        7 |  2272 | `	for(;;){` |
|       15 |  2273 | `		if( pInstr >= pEnd ){` |
|        - |  2274 | `			/* No more instructions */` |
|        3 |  2275 | `			break;` |
|        - |  2276 | `		}` |
|        - |  2277 | `		/* Format and call the consumer callback */` |
|       19 |  2278 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2279 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2280 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2281 | `		if( rc != SXRET_OK ){` |
|        - |  2282 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2283 | `			return rc;` |
|        - |  2284 | `		}` |
|       13 |  2285 | `		++n;` |
|       13 |  2286 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2287 | `	}` |
|        3 |  2288 | `	return rc;` |
|        2 |  2289 |  |
|        - |  2290 | `/* Forward declaration */` |
|        - |  2291 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2292 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2293 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2294 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2295 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2296 | `/*` |
|        - |  2297 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2298 | ` * consumer callback.` |
|        - |  2299 | ` */` |
|      558 |  2300 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2301 |  |
|      559 |  2302 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      559 |  2303 | `	sxi32 rc = SXRET_OK;` |
|        - |  2304 | `	/* Append a new line */` |
|        - |  2305 | `#ifdef __WINNT__` |
|        1 |  2306 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2307 | `#else` |
|      558 |  2308 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2309 | `#endif` |
|        - |  2310 | `	/* Invoke the output consumer callback */` |
|      559 |  2311 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      559 |  2312 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      559 |  2313 | `	return rc;` |
|        1 |  2314 |  |
|        - |  2315 | `/*` |
|        - |  2316 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2317 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2318 | ` * information.` |
|        - |  2319 | ` */` |
|      134 |  2320 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2321 |  |
|      136 |  2322 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2323 | `		ph7_value apArg[4];` |
|        - |  2324 | `		ph7_value *apArgPtr[4];` |
|        - |  2325 | `		ph7_value sResult;` |
|        - |  2326 | `		SyString sErr;` |
|        - |  2327 | `		/* Prepare arguments */` |
|       61 |  2328 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2329 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2330 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2331 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2332 | `		if( pFile ){` |
|       61 |  2333 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2334 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2335 | `		}else{` |
|      ! 0 |  2336 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2337 | `		}` |
|       61 |  2338 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2339 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2340 | `		/* Set up pointer array */` |
|       61 |  2341 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2342 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2343 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2344 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2345 | `		/* Call the handler */` |
|       61 |  2346 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2347 | `		/* Check return value */` |
|       61 |  2348 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2349 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2350 | `		}` |
|        - |  2351 | `		/* Release */` |
|       61 |  2352 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2353 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2354 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2355 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2356 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2357 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2358 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2359 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2360 | `	}` |
|        - |  2361 | `	/* No handler, always call error handler */` |
|       75 |  2362 | `	return TRUE;` |
|       69 |  2363 |  |
|       98 |  2364 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2365 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2366 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2367 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2368 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2369 | `	)` |
|        2 |  2370 |  |
|      100 |  2371 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2372 | `	SyString *pFile;` |
|        - |  2373 | `	char *zErr;` |
|      100 |  2374 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2375 | `	if( !pVm->bErrReport ){` |
|        - |  2376 | `		/* Don't bother reporting errors */` |
|        3 |  2377 | `		return SXRET_OK;` |
|        - |  2378 | `	}` |
|        - |  2379 | `	/* Reset the working buffer */` |
|       98 |  2380 | `	SyBlobReset(pWorker);` |
|        - |  2381 | `	/* Peek the processed file if available */` |
|       98 |  2382 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2383 | `	if( pFile ){` |
|        - |  2384 | `		/* Append file name */` |
|       98 |  2385 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2386 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2387 | `	}` |
|        - |  2388 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2389 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2390 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2391 | `	 * E_DEPRECATED). */` |
|       98 |  2392 | `	zErr = "Error:  ";` |
|       98 |  2393 | `	switch(iErr){` |
|       19 |  2394 | `	case PH7_CTX_WARNING:` |
|       40 |  2395 | `		zErr = "Warning:  ";` |
|       40 |  2396 | `		break;` |
|        6 |  2397 | `	case PH7_CTX_NOTICE:` |
|       14 |  2398 | `		zErr = "Notice:  ";` |
|       12 |  2399 | `		break;` |
|       23 |  2400 | `	default:` |
|        - |  2401 | `		/* keep iErr unchanged */` |
|       46 |  2402 | `		break;` |
|        - |  2403 | `	}` |
|       98 |  2404 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2405 | `	if( pFuncName ){` |
|        - |  2406 | `		/* Append function name first */` |
|       23 |  2407 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2408 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2409 | `	}` |
|       98 |  2410 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2411 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2412 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2413 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2414 | `	}` |
|       98 |  2415 | `	return rc;` |
|       51 |  2416 |  |
|        - |  2417 | `/*` |
|        - |  2418 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2419 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2420 | ` * information.` |
|        - |  2421 | ` */` |
|       38 |  2422 | `static sxi32 VmThrowErrorAp(` |
|        - |  2423 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2424 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2425 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2426 | `	const char *zFormat, /* Format message */` |
|        - |  2427 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2428 | `	)` |
|        2 |  2429 |  |
|       40 |  2430 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2431 | `	SyBlob sMsg;` |
|        - |  2432 | `	SyString *pFile;` |
|        - |  2433 | `	char *zErr;` |
|       40 |  2434 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2435 | `	if( !pVm->bErrReport ){` |
|        - |  2436 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2437 | `		return SXRET_OK;` |
|        - |  2438 | `	}` |
|        - |  2439 | `	/* Reset the working buffer */` |
|       40 |  2440 | `	SyBlobReset(pWorker);` |
|        - |  2441 | `	/* Peek the processed file if available */` |
|       40 |  2442 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2443 | `	if( pFile ){` |
|        - |  2444 | `		/* Append file name */` |
|       40 |  2445 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2446 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2447 | `	}` |
|        - |  2448 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2449 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2450 | `	 * the correct errno value. */` |
|       40 |  2451 | `	zErr = "Error:  ";` |
|       40 |  2452 | `	switch(iErr){` |
|        4 |  2453 | `	case PH7_CTX_WARNING:` |
|        9 |  2454 | `		zErr = "Warning:  ";` |
|        9 |  2455 | `		break;` |
|        3 |  2456 | `	case PH7_CTX_NOTICE:` |
|        7 |  2457 | `		zErr = "Notice:  ";` |
|        6 |  2458 | `		break;` |
|       12 |  2459 | `	default:` |
|        - |  2460 | `		/* do not change iErr */` |
|       24 |  2461 | `		break;` |
|        - |  2462 | `	}` |
|       40 |  2463 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2464 | `	if( pFuncName ){` |
|        - |  2465 | `		/* Append function name first */` |
|       26 |  2466 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2467 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2468 | `	}` |
|        - |  2469 | `	/* Format the raw message */` |
|       40 |  2470 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2471 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2472 | `	/* Check if a user error handler is installed */` |
|       40 |  2473 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2474 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2475 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2476 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2477 | `	}` |
|       40 |  2478 | `	SyBlobRelease(&sMsg);` |
|       40 |  2479 | `	return rc;` |
|       21 |  2480 |  |
|        - |  2481 | `/*` |
|        - |  2482 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2483 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2484 | ` * information.` |
|        - |  2485 | ` * ------------------------------------` |
|        - |  2486 | ` * Simple boring wrapper function.` |
|        - |  2487 | ` * ------------------------------------` |
|        - |  2488 | ` */` |
|       14 |  2489 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2490 |  |
|        - |  2491 | `	va_list ap;` |
|        - |  2492 | `	sxi32 rc;` |
|       15 |  2493 | `	va_start(ap,zFormat);` |
|       15 |  2494 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2495 | `	va_end(ap);` |
|       15 |  2496 | `	return rc;` |
|        1 |  2497 |  |
|        - |  2498 | `/*` |
|        - |  2499 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  2500 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  2501 | ` */` |
|       10 |  2502 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  2503 |  |
|        - |  2504 | `	ph7_class *pClass;` |
|        - |  2505 | `	ph7_class_instance *pThis;` |
|        - |  2506 | `	ph7_class_method *pCons;` |
|        - |  2507 | `	ph7_value sArg;` |
|        - |  2508 | `	ph7_value *apArg[1];` |
|        - |  2509 | `	SyBlob sMsg;` |
|        - |  2510 | `	SyString sMsgStr;` |
|        - |  2511 | `	VmFrame *pFrame;` |
|        - |  2512 | `	sxi32 rc;` |
|       11 |  2513 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       11 |  2514 | `	if( pClass == 0 ){` |
|      ! 0 |  2515 | `		return PH7_ABORT;` |
|        - |  2516 | `	}` |
|       11 |  2517 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       11 |  2518 | `	if( pThis == 0 ){` |
|      ! 0 |  2519 | `		return PH7_ABORT;` |
|        - |  2520 | `	}` |
|       11 |  2521 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 |  2522 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|        5 |  2523 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       11 |  2524 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       11 |  2525 | `	if( pCons ){` |
|       11 |  2526 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       11 |  2527 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       11 |  2528 | `		apArg[0] = &sArg;` |
|       11 |  2529 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       11 |  2530 | `		PH7_MemObjRelease(&sArg);` |
|        5 |  2531 | `	}` |
|       11 |  2532 | `	SyBlobRelease(&sMsg);` |
|       11 |  2533 | `	pFrame = pVm->pFrame;` |
|       11 |  2534 | `	if( pFrame ){` |
|       11 |  2535 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       11 |  2536 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        5 |  2537 | `	}` |
|       11 |  2538 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       11 |  2539 | `	PH7_ClassInstanceUnref(pThis);` |
|       11 |  2540 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2541 | `		return PH7_ABORT;` |
|        - |  2542 | `	}` |
|       11 |  2543 | `	return PH7_EXCEPTION;` |
|        6 |  2544 |  |
|        - |  2545 | `/*` |
|        - |  2546 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2547 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2548 | ` * information.` |
|        - |  2549 | ` * ------------------------------------` |
|        - |  2550 | ` * Simple boring wrapper function.` |
|        - |  2551 | ` * ------------------------------------` |
|        - |  2552 | ` */` |
|       24 |  2553 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2554 |  |
|        - |  2555 | `	sxi32 rc;` |
|       26 |  2556 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2557 | `	return rc;` |
|        2 |  2558 |  |
|        - |  2559 | `/*` |
|        - |  2560 | ` * Resolve function context from the current frame.` |
|        - |  2561 | ` */` |
|      954 |  2562 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2563 |  |
|        - |  2564 | `	VmFrame *pFrame;` |
|        - |  2565 | `	ph7_vm_func *pFunc;` |
|      955 |  2566 | `	*pzFuncName = 0;` |
|      955 |  2567 | `	*pnFuncLen = 0;` |
|      955 |  2568 | `	pFrame = pVm->pFrame;` |
|      955 |  2569 | `	if( pFrame == 0 ){` |
|      ! 0 |  2570 | `		return;` |
|        - |  2571 | `	}` |
|      955 |  2572 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      955 |  2573 | `	if( pFrame->pParent == 0 ){` |
|      947 |  2574 | `		return;` |
|        - |  2575 | `	}` |
|        9 |  2576 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        9 |  2577 | `	if( pFunc == 0 ){` |
|      ! 0 |  2578 | `		return;` |
|        - |  2579 | `	}` |
|        9 |  2580 | `	*pzFuncName = pFunc->sName.zString;` |
|        9 |  2581 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      478 |  2582 |  |
|        - |  2583 | `/*` |
|        - |  2584 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2585 | ` */` |
|      482 |  2586 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2587 |  |
|        - |  2588 | `	SyBlob sOut;` |
|        - |  2589 | `	SyString *pFile;` |
|      483 |  2590 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2591 | `		return PH7_OK;` |
|        - |  2592 | `	}` |
|      483 |  2593 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2594 | `		zClass = "Exception";` |
|      ! 0 |  2595 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2596 | `	}` |
|      483 |  2597 | `	if( zMsg == 0 ){` |
|      ! 0 |  2598 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2599 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2600 | `	}` |
|      483 |  2601 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      477 |  2602 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      238 |  2603 | `	}` |
|      483 |  2604 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      483 |  2605 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      483 |  2606 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      483 |  2607 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      483 |  2608 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      483 |  2609 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      483 |  2610 | `	if( pFile ){` |
|      483 |  2611 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      483 |  2612 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2613 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      241 |  2614 | `	}` |
|      483 |  2615 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      483 |  2616 | `	if( pFile ){` |
|      483 |  2617 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      483 |  2618 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2619 | `		if( zFuncName && nFuncLen > 0 ){` |
|        9 |  2620 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        5 |  2621 | `		}else{` |
|      475 |  2622 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2623 | `		}` |
|      241 |  2624 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2625 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2626 | `	}else{` |
|      ! 0 |  2627 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2628 | `	}` |
|      483 |  2629 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      483 |  2630 | `	if( pFile ){` |
|      483 |  2631 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      483 |  2632 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      483 |  2633 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2634 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      241 |  2635 | `	}` |
|      483 |  2636 | `	VmCallErrorHandler(pVm,&sOut);` |
|      483 |  2637 | `	SyBlobRelease(&sOut);` |
|      483 |  2638 | `	return PH7_ABORT;` |
|      242 |  2639 |  |
|        - |  2640 | `/*` |
|        - |  2641 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2642 | ` */` |
|      480 |  2643 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2644 |  |
|        - |  2645 | `	ph7_vm *pVm;` |
|        - |  2646 | `	ph7_class *pClass;` |
|        - |  2647 | `	ph7_class_instance *pThis;` |
|        - |  2648 | `	ph7_class_method *pCons;` |
|        - |  2649 | `	ph7_value sArg;` |
|        - |  2650 | `	ph7_value *apArg[1];` |
|        - |  2651 | `	SyBlob sMsg;` |
|        - |  2652 | `	SyString sMsgStr;` |
|        - |  2653 | `	VmFrame *pFrame;` |
|        - |  2654 | `	va_list ap;` |
|        - |  2655 | `	sxi32 rc;` |
|        - |  2656 |  |
|      482 |  2657 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2658 | `		return PH7_ABORT;` |
|        - |  2659 | `	}` |
|      482 |  2660 | `	pVm = pCtx->pVm;` |
|      482 |  2661 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2662 | `		zClass = "Error";` |
|      ! 0 |  2663 | `	}` |
|      482 |  2664 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  2665 | `	if( pClass == 0 ){` |
|      ! 0 |  2666 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2667 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2668 | `			zClass` |
|        - |  2669 | `			);` |
|        - |  2670 | `	}` |
|      482 |  2671 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  2672 | `	if( pThis == 0 ){` |
|      ! 0 |  2673 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2674 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2675 | `			);` |
|        - |  2676 | `	}` |
|        - |  2677 |  |
|      482 |  2678 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  2679 | `	va_start(ap,zFormat);` |
|      482 |  2680 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  2681 | `	va_end(ap);` |
|        - |  2682 |  |
|      482 |  2683 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  2684 | `	if( pCons ){` |
|      482 |  2685 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  2686 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  2687 | `		apArg[0] = &sArg;` |
|      482 |  2688 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  2689 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  2690 | `	}` |
|      482 |  2691 | `	SyBlobRelease(&sMsg);` |
|        - |  2692 |  |
|      482 |  2693 | `	pFrame = pVm->pFrame;` |
|      482 |  2694 | `	if( pFrame ){` |
|      482 |  2695 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  2696 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  2697 | `	}` |
|      482 |  2698 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  2699 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  2700 | `	if( rc == SXERR_ABORT ){` |
|      471 |  2701 | `		return PH7_ABORT;` |
|        - |  2702 | `	}` |
|       12 |  2703 | `	return PH7_EXCEPTION;` |
|      242 |  2704 |  |
|        - |  2705 | `/*` |
|        - |  2706 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2707 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2708 | ` */` |
|      ! 0 |  2709 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2710 |  |
|        - |  2711 | `	ph7_vm *pVm;` |
|        - |  2712 | `	SyBlob sMsg;` |
|      ! 0 |  2713 | `	const char *zFuncName = 0;` |
|      ! 0 |  2714 | `	int nFuncLen = 0;` |
|        - |  2715 | `	va_list ap;` |
|        - |  2716 | `	sxi32 rc;` |
|        - |  2717 |  |
|      ! 0 |  2718 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2719 | `		return PH7_OK;` |
|        - |  2720 | `	}` |
|      ! 0 |  2721 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2722 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2723 | `		zClass = "Error";` |
|      ! 0 |  2724 | `	}` |
|        - |  2725 |  |
|      ! 0 |  2726 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2727 |  |
|      ! 0 |  2728 | `	va_start(ap,zFormat);` |
|      ! 0 |  2729 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2730 | `	va_end(ap);` |
|        - |  2731 |  |
|      ! 0 |  2732 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2733 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2734 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2735 | `	}` |
|      ! 0 |  2736 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2737 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2738 | `	}` |
|      ! 0 |  2739 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2740 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2741 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2742 | `	return rc;` |
|      ! 0 |  2743 |  |
|        - |  2744 | `/*` |
|        - |  2745 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2746 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2747 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2748 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2749 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2750 | ` * when VmByteCodeExec returns.` |
|        - |  2751 | ` */` |
|      132 |  2752 | `static sxi32 VmSuspendCtx(` |
|        - |  2753 | `	ph7_vm *pVm,` |
|        - |  2754 | `	ph7_exec_ctx *pCtx,` |
|        - |  2755 | `	sxi32 pc,` |
|        - |  2756 | `	sxi32 nTos` |
|        - |  2757 | `	)` |
|        2 |  2758 |  |
|       66 |  2759 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  2760 | `	pCtx->pc = pc;` |
|      134 |  2761 | `	pCtx->nTos = nTos;` |
|      134 |  2762 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  2763 | `	return PH7_SUSPEND;` |
|        2 |  2764 |  |
|        - |  2765 | `/*` |
|        - |  2766 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2767 | ` *` |
|        - |  2768 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2769 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2770 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2771 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2772 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2773 | ` * then the program execution is halted.` |
|        - |  2774 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2775 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2776 | ` * or to reset the VM to it's initial state.` |
|        - |  2777 | ` */` |
|    34150 |  2778 | `static sxi32 VmByteCodeExec(` |
|        - |  2779 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2780 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2781 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2782 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2783 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2784 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2785 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2786 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2787 | `	)` |
|        2 |  2788 |  |
|        - |  2789 | `	VmInstr *pInstr;` |
|        - |  2790 | `	ph7_value *pTos;` |
|        - |  2791 | `	SySet aArg;` |
|        - |  2792 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2793 | `	sxi32 pc;` |
|        - |  2794 | `	sxi32 rc;` |
|        - |  2795 | `	/* Argument container */` |
|    34152 |  2796 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    34152 |  2797 | `	if( nTos < 0 ){` |
|    32052 |  2798 | `		pTos = &pStack[-1];` |
|    16027 |  2799 | `	}else{` |
|     2102 |  2800 | `		pTos = &pStack[nTos];` |
|        - |  2801 | `	}` |
|    34152 |  2802 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    34152 |  2803 | `	pc = nPc;` |
|        - |  2804 | `	/* Execute as much as we can */` |
|  5153673 |  2805 | `	for(;;){` |
|        - |  2806 | `		/* Fetch the instruction to execute */` |
| 10306644 |  2807 | `		pInstr = &aInstr[pc];` |
| 10306644 |  2808 | `		rc = SXRET_OK;` |
|        - |  2809 | `/*` |
|        - |  2810 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2811 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2812 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2813 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2814 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2815 | ` */` |
| 10306644 |  2816 | `		switch(pInstr->iOp){` |
|        - |  2817 | `/*` |
|        - |  2818 | ` * DONE: P1 * *` |
|        - |  2819 | ` *` |
|        - |  2820 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2821 | ` * and return immediately.` |
|        - |  2822 | ` */` |
|    16757 |  2823 | `case PH7_OP_DONE:` |
|    33516 |  2824 | `	if( pInstr->iP1 ){` |
|        - |  2825 | `#ifdef UNTRUST` |
|        - |  2826 | `		if( pTos < pStack ){` |
|        - |  2827 | `			goto Abort;` |
|        - |  2828 | `		}` |
|        - |  2829 | `#endif` |
|    19450 |  2830 | `		if( pLastRef ){` |
|    12622 |  2831 | `			*pLastRef = pTos->nIdx;` |
|     6310 |  2832 | `		}` |
|    19450 |  2833 | `		if( pResult ){` |
|        - |  2834 | `			/* Execution result */` |
|    18464 |  2835 | `			PH7_MemObjStore(pTos,pResult);` |
|     9231 |  2836 | `		}` |
|    19450 |  2837 | `		VmPopOperand(&pTos,1);` |
|    23792 |  2838 | `	}else if( pLastRef ){` |
|        - |  2839 | `		/* Nothing referenced */` |
|     1098 |  2840 | `		*pLastRef = SXU32_HIGH;` |
|      548 |  2841 | `	}` |
|        - |  2842 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2843 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2844 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2845 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2846 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2847 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2848 | `	 * block can override it.` |
|        - |  2849 | `	 */` |
|    33518 |  2850 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2851 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2852 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2853 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2854 | `		pExc->pFrame = 0;` |
|        3 |  2855 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2856 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2857 | `			pExc->iFinallyDone = 1;` |
|        - |  2858 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2859 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2860 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2861 | `				goto Abort;` |
|        - |  2862 | `			}` |
|        1 |  2863 | `		}` |
|        1 |  2864 | `	}` |
|    33516 |  2865 | `	goto Done;` |
|        - |  2866 | `/*` |
|        - |  2867 | ` * HALT: P1 * *` |
|        - |  2868 | ` *` |
|        - |  2869 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2870 | ` * and abort immediately.` |
|        - |  2871 | ` */` |
|        4 |  2872 | `case PH7_OP_HALT:` |
|        9 |  2873 | `	if( pInstr->iP1 ){` |
|        - |  2874 | `#ifdef UNTRUST` |
|        - |  2875 | `		if( pTos < pStack ){` |
|        - |  2876 | `			goto Abort;` |
|        - |  2877 | `		}` |
|        - |  2878 | `#endif` |
|        9 |  2879 | `		if( pLastRef ){` |
|      ! 0 |  2880 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2881 | `		}` |
|        9 |  2882 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2883 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2884 | `				/* Output the exit message */` |
|        7 |  2885 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2886 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2887 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2888 | `			}` |
|        7 |  2889 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2890 | `			/* Record exit status */` |
|        5 |  2891 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2892 | `		}` |
|        9 |  2893 | `		VmPopOperand(&pTos,1);` |
|        4 |  2894 | `	}else if( pLastRef ){` |
|        - |  2895 | `		/* Nothing referenced */` |
|      ! 0 |  2896 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2897 | `	}` |
|        - |  2898 | `	/* Check if we're in an included file context */` |
|        9 |  2899 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2900 | `		/* Terminate the entire process */` |
|        9 |  2901 | `		exit(pVm->iExitStatus);` |
|        - |  2902 | `	}` |
|      ! 0 |  2903 | `	goto Abort;` |
|        - |  2904 | `/*` |
|        - |  2905 | ` * JMP: * P2 *` |
|        - |  2906 | ` *` |
|        - |  2907 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2908 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2909 | ` */` |
|   222093 |  2910 | `case PH7_OP_JMP:` |
|   444232 |  2911 | `	pc = pInstr->iP2 - 1;` |
|   444232 |  2912 | `	break;` |
|        - |  2913 | `/*` |
|        - |  2914 | ` * JZ: P1 P2 *` |
|        - |  2915 | ` *` |
|        - |  2916 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2917 | ` * entry in the stack if P1 is zero.` |
|        - |  2918 | ` */` |
|   520344 |  2919 | `case PH7_OP_JZ:` |
|        - |  2920 | `#ifdef UNTRUST` |
|        - |  2921 | `	if( pTos < pStack ){` |
|        - |  2922 | `		goto Abort;` |
|        - |  2923 | `	}` |
|        - |  2924 | `#endif` |
|        - |  2925 | `	/* Get a boolean value */` |
|  1040778 |  2926 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2927 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2928 | `	}` |
|  1040778 |  2929 | `	if( !pTos->x.iVal ){` |
|        - |  2930 | `		/* Take the jump */` |
|   527738 |  2931 | `		pc = pInstr->iP2 - 1;` |
|   263868 |  2932 | `	}` |
|  1040778 |  2933 | `	if( !pInstr->iP1 ){` |
|   826752 |  2934 | `		VmPopOperand(&pTos,1);` |
|   413397 |  2935 | `	}` |
|  1040778 |  2936 | `	break;` |
|        - |  2937 | `/*` |
|        - |  2938 | ` * JNZ: P1 P2 *` |
|        - |  2939 | ` *` |
|        - |  2940 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2941 | ` * entry in the stack if P1 is zero.` |
|        - |  2942 | ` */` |
|    54959 |  2943 | `case PH7_OP_JNZ:` |
|        - |  2944 | `#ifdef UNTRUST` |
|        - |  2945 | `	if( pTos < pStack ){` |
|        - |  2946 | `		goto Abort;` |
|        - |  2947 | `	}` |
|        - |  2948 | `#endif` |
|        - |  2949 | `	/* Get a boolean value */` |
|   109920 |  2950 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2951 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2952 | `	}` |
|   109920 |  2953 | `	if( pTos->x.iVal ){` |
|        - |  2954 | `		/* Take the jump */` |
|     4710 |  2955 | `		pc = pInstr->iP2 - 1;` |
|     2354 |  2956 | `	}` |
|   109920 |  2957 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2958 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2959 | `	}` |
|   109920 |  2960 | `	break;` |
|        - |  2961 | `/*` |
|        - |  2962 | ` * NOOP: * * *` |
|        - |  2963 | ` *` |
|        - |  2964 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2965 | ` * destination.` |
|        - |  2966 | ` */` |
|      ! 0 |  2967 | `case PH7_OP_NOOP:` |
|      ! 0 |  2968 | `	break;` |
|        - |  2969 | `/*` |
|        - |  2970 | ` * POP: P1 * *` |
|        - |  2971 | ` *` |
|        - |  2972 | ` * Pop P1 elements from the operand stack.` |
|        - |  2973 | ` */` |
|   404622 |  2974 | `case PH7_OP_POP: {` |
|   809290 |  2975 | `	sxi32 n = pInstr->iP1;` |
|   809290 |  2976 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2977 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       11 |  2978 | `		n = (sxi32)(pTos - pStack);` |
|        5 |  2979 | `	}` |
|   809290 |  2980 | `	VmPopOperand(&pTos,n);` |
|   809290 |  2981 | `	break;` |
|        - |  2982 | `				 }` |
|        - |  2983 | `/*` |
|        - |  2984 | ` * DUP: * * *` |
|        - |  2985 | ` *` |
|        - |  2986 | ` * Duplicate the top of the stack.` |
|        - |  2987 | ` */` |
|       41 |  2988 | `case PH7_OP_DUP:` |
|        - |  2989 | `#ifdef UNTRUST` |
|        - |  2990 | `	if( pTos < pStack ){` |
|        - |  2991 | `		goto Abort;` |
|        - |  2992 | `	}` |
|        - |  2993 | `#endif` |
|       84 |  2994 | `	pTos++;` |
|       84 |  2995 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  2996 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  2997 | `	break;` |
|        - |  2998 | `/*` |
|        - |  2999 | ` * NSSWITCH: * * P3` |
|        - |  3000 | ` *` |
|        - |  3001 | ` * Switch the active namespace at runtime.` |
|        - |  3002 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3003 | ` */` |
|     6717 |  3004 | `case PH7_OP_NSSWITCH:` |
|    13436 |  3005 | `	SyBlobReset(&pVm->sNamespace);` |
|    13436 |  3006 | `	if( pInstr->p3 ){` |
|       92 |  3007 | `		const char *zNs = (const char *)pInstr->p3;` |
|       92 |  3008 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       45 |  3009 | `	}` |
|        - |  3010 | `	/* Clear namespace-scoped use-const imports */` |
|    13436 |  3011 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13436 |  3012 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13436 |  3013 | `	break;` |
|        - |  3014 | `/* OP_USECONST P1 * P3` |
|        - |  3015 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3016 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3017 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3018 | ` */` |
|        7 |  3019 | `case PH7_OP_USECONST: {` |
|       16 |  3020 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3021 | `	if( azPair ){` |
|       16 |  3022 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3023 | `	}` |
|       16 |  3024 | `	break;` |
|        - |  3025 | `				}` |
|        - |  3026 | `/*` |
|        - |  3027 | ` * CVT_INT: * * *` |
|        - |  3028 | ` *` |
|        - |  3029 | ` * Force the top of the stack to be an integer.` |
|        - |  3030 | ` */` |
|       77 |  3031 | `case PH7_OP_CVT_INT:` |
|        - |  3032 | `#ifdef UNTRUST` |
|        - |  3033 | `	if( pTos < pStack ){` |
|        - |  3034 | `		goto Abort;` |
|        - |  3035 | `	}` |
|        - |  3036 | `#endif` |
|      156 |  3037 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3038 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3039 | `	}` |
|        - |  3040 | `	/* Invalidate any prior representation */` |
|      156 |  3041 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3042 | `	break;` |
|        - |  3043 | `/*` |
|        - |  3044 | ` * CVT_REAL: * * *` |
|        - |  3045 | ` *` |
|        - |  3046 | ` * Force the top of the stack to be a real.` |
|        - |  3047 | ` */` |
|        4 |  3048 | `case PH7_OP_CVT_REAL:` |
|        - |  3049 | `#ifdef UNTRUST` |
|        - |  3050 | `	if( pTos < pStack ){` |
|        - |  3051 | `		goto Abort;` |
|        - |  3052 | `	}` |
|        - |  3053 | `#endif` |
|        9 |  3054 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3055 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3056 | `	}` |
|        - |  3057 | `	/* Invalidate any prior representation */` |
|        9 |  3058 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3059 | `	break;` |
|        - |  3060 | `/*` |
|        - |  3061 | ` * CVT_STR: * * *` |
|        - |  3062 | ` *` |
|        - |  3063 | ` * Force the top of the stack to be a string.` |
|        - |  3064 | ` */` |
|      146 |  3065 | `case PH7_OP_CVT_STR:` |
|        - |  3066 | `#ifdef UNTRUST` |
|        - |  3067 | `	if( pTos < pStack ){` |
|        - |  3068 | `		goto Abort;` |
|        - |  3069 | `	}` |
|        - |  3070 | `#endif` |
|      294 |  3071 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3072 | `		PH7_MemObjToString(pTos);` |
|      146 |  3073 | `	}` |
|      294 |  3074 | `	break;` |
|        - |  3075 | `/*` |
|        - |  3076 | ` * CVT_BOOL: * * *` |
|        - |  3077 | ` *` |
|        - |  3078 | ` * Force the top of the stack to be a boolean.` |
|        - |  3079 | ` */` |
|        5 |  3080 | `case PH7_OP_CVT_BOOL:` |
|        - |  3081 | `#ifdef UNTRUST` |
|        - |  3082 | `	if( pTos < pStack ){` |
|        - |  3083 | `		goto Abort;` |
|        - |  3084 | `	}` |
|        - |  3085 | `#endif` |
|       11 |  3086 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3087 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3088 | `	}` |
|       11 |  3089 | `	break;` |
|        - |  3090 | `/*` |
|        - |  3091 | ` * CVT_NULL: * * *` |
|        - |  3092 | ` *` |
|        - |  3093 | ` * Nullify the top of the stack.` |
|        - |  3094 | ` */` |
|        3 |  3095 | `case PH7_OP_CVT_NULL:` |
|        - |  3096 | `#ifdef UNTRUST` |
|        - |  3097 | `	if( pTos < pStack ){` |
|        - |  3098 | `		goto Abort;` |
|        - |  3099 | `	}` |
|        - |  3100 | `#endif` |
|        7 |  3101 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3102 | `	break;` |
|        - |  3103 | `/*` |
|        - |  3104 | ` * CVT_NUMC: * * *` |
|        - |  3105 | ` *` |
|        - |  3106 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3107 | ` */` |
|      ! 0 |  3108 | `case PH7_OP_CVT_NUMC:` |
|        - |  3109 | `#ifdef UNTRUST` |
|        - |  3110 | `	if( pTos < pStack ){` |
|        - |  3111 | `		goto Abort;` |
|        - |  3112 | `	}` |
|        - |  3113 | `#endif` |
|        - |  3114 | `	/* Force a numeric cast */` |
|      ! 0 |  3115 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3116 | `	break;` |
|        - |  3117 | `/*` |
|        - |  3118 | ` * CVT_ARRAY: * * *` |
|        - |  3119 | ` *` |
|        - |  3120 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3121 | ` */` |
|       10 |  3122 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3123 | `#ifdef UNTRUST` |
|        - |  3124 | `	if( pTos < pStack ){` |
|        - |  3125 | `		goto Abort;` |
|        - |  3126 | `	}` |
|        - |  3127 | `#endif` |
|        - |  3128 | `	/* Force a hashmap cast */` |
|       21 |  3129 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3130 | `	if( rc != SXRET_OK ){` |
|        - |  3131 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3132 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3133 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3134 | `	}` |
|       21 |  3135 | `	break;` |
|        - |  3136 | `/*` |
|        - |  3137 | ` * CVT_OBJ: * * *` |
|        - |  3138 | ` *` |
|        - |  3139 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3140 | ` */` |
|        8 |  3141 | `case PH7_OP_CVT_OBJ:` |
|        - |  3142 | `#ifdef UNTRUST` |
|        - |  3143 | `	if( pTos < pStack ){` |
|        - |  3144 | `		goto Abort;` |
|        - |  3145 | `	}` |
|        - |  3146 | `#endif` |
|       17 |  3147 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3148 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3149 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3150 | `	}` |
|       17 |  3151 | `	break;` |
|        - |  3152 | `/*` |
|        - |  3153 | ` * ERR_CTRL * * *` |
|        - |  3154 | ` *` |
|        - |  3155 | ` * Error control operator.` |
|        - |  3156 | ` */` |
|    13532 |  3157 | `case PH7_OP_ERR_CTRL:` |
|        - |  3158 | `	/*` |
|        - |  3159 | `	 * TICKET 1433-038:` |
|        - |  3160 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3161 | `	 * use the public API,to control error output.` |
|        - |  3162 | `	 */` |
|    27064 |  3163 | `	break;` |
|        - |  3164 | `/*` |
|        - |  3165 | ` * IS_A * * *` |
|        - |  3166 | ` *` |
|        - |  3167 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3168 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3169 | ` * holding a class name or an object).` |
|        - |  3170 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3171 | ` */` |
|       23 |  3172 | `case PH7_OP_IS_A:{` |
|       48 |  3173 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3174 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3175 | `#ifdef UNTRUST` |
|        - |  3176 | `	if( pNos < pStack ){` |
|        - |  3177 | `		goto Abort;` |
|        - |  3178 | `	}` |
|        - |  3179 | `#endif` |
|       48 |  3180 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3181 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3182 | `		ph7_class *pClass = 0;` |
|        - |  3183 | `		/* Extract the target class */` |
|       46 |  3184 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3185 | `			/* Instance already loaded */` |
|      ! 0 |  3186 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3187 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3188 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3189 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3190 | `			/* Handle self/static/parent keywords */` |
|       46 |  3191 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3192 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3193 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3194 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3195 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3196 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3197 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3198 | `					pClass = pSelf->pBase;` |
|        2 |  3199 | `				}` |
|        3 |  3200 | `			}else{` |
|       36 |  3201 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3202 | `			}` |
|       22 |  3203 | `		}` |
|       46 |  3204 | `		if( pClass ){` |
|        - |  3205 | `			/* Perform the query */` |
|       46 |  3206 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3207 | `		}` |
|       22 |  3208 | `	}` |
|        - |  3209 | `	/* Push result */` |
|       48 |  3210 | `	VmPopOperand(&pTos,1);` |
|       48 |  3211 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3212 | `	pTos->x.iVal = iRes;` |
|       48 |  3213 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3214 | `	break;` |
|        - |  3215 | `				 }` |
|        - |  3216 |  |
|        - |  3217 | `/*` |
|        - |  3218 | ` * LOADC P1 P2 *` |
|        - |  3219 | ` *` |
|        - |  3220 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3221 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3222 | ` */` |
|   869323 |  3223 | `case PH7_OP_LOADC: {` |
|        - |  3224 | `	ph7_value *pObj;` |
|        - |  3225 | `	/* Reserve a room */` |
|  1738692 |  3226 | `	pTos++;` |
|  2599591 |  3227 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1738692 |  3228 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3229 | `			SyHashEntry *pEntry;` |
|        - |  3230 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3231 | `			{` |
|        - |  3232 | `				SyHashEntry *pConstImport;` |
|    25409 |  3233 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    16938 |  3234 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16940 |  3235 | `				if( pConstImport ){` |
|       11 |  3236 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3237 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3238 | `					if( pEntry ){` |
|       11 |  3239 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3240 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3241 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3242 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3243 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3244 | `						break;` |
|        - |  3245 | `					}` |
|        - |  3246 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3247 | `				}` |
|        - |  3248 | `			}` |
|        - |  3249 | `			/* Candidate for expansion via user defined callbacks */` |
|    16930 |  3250 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16930 |  3251 | `			if( pEntry ){` |
|    16926 |  3252 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3253 | `				/* Set a NULL default value */` |
|    16926 |  3254 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16926 |  3255 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3256 | `				/* Invoke the callback and deal with the expanded value */` |
|    16926 |  3257 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3258 | `				/* Mark as constant */` |
|    16926 |  3259 | `				pTos->nIdx = SXU32_HIGH;` |
|    16926 |  3260 | `				break;` |
|        - |  3261 | `			}` |
|        - |  3262 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3263 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3264 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3265 | `			{` |
|        6 |  3266 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3267 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3268 | `				sxu32 j;` |
|        6 |  3269 | `				int isQualified = 0;` |
|       32 |  3270 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3271 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3272 | `				}` |
|        6 |  3273 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3274 | `					/* Try current_namespace\name */` |
|      ! 0 |  3275 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3276 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3277 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3278 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3279 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3280 | `					if( pEntry ){` |
|      ! 0 |  3281 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3282 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3283 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3284 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3285 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3286 | `						break;` |
|        - |  3287 | `					}` |
|        - |  3288 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3289 | `				}` |
|        6 |  3290 | `				if( isQualified ){` |
|        - |  3291 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3292 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3293 | `					SyBlob sErr;` |
|        3 |  3294 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3295 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3296 | `					if( pErrFile ){` |
|        3 |  3297 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3298 | `					}` |
|        3 |  3299 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3300 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3301 | `					SyBlobRelease(&sErr);` |
|        3 |  3302 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3303 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3304 | `					goto LoadC_Done;` |
|        - |  3305 | `				}` |
|        - |  3306 | `			}` |
|        1 |  3307 | `		}` |
|  1721756 |  3308 | `		PH7_MemObjLoad(pObj,pTos);` |
|   860901 |  3309 | `	}else{` |
|        - |  3310 | `		/* Set a NULL value */` |
|      ! 0 |  3311 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3312 | `	}` |
|   860856 |  3313 | `LoadC_Done:` |
|        - |  3314 | `	/* Mark as constant */` |
|  1721758 |  3315 | `	pTos->nIdx = SXU32_HIGH;` |
|  1721758 |  3316 | `	break;` |
|        - |  3317 | `				  }` |
|        - |  3318 | `/*` |
|        - |  3319 | ` * LOAD: P1 * P3` |
|        - |  3320 | ` *` |
|        - |  3321 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3322 | ` * from the P3 operand.` |
|        - |  3323 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3324 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3325 | ` */` |
|  1391578 |  3326 | `case PH7_OP_LOAD:{` |
|        - |  3327 | `	ph7_value *pObj;` |
|        - |  3328 | `	SyString sName;` |
|  2783378 |  3329 | `	if( pInstr->p3 == 0 ){` |
|        - |  3330 | `		/* Take the variable name from the top of the stack */` |
|        - |  3331 | `#ifdef UNTRUST` |
|        - |  3332 | `		if( pTos < pStack ){` |
|        - |  3333 | `			goto Abort;` |
|        - |  3334 | `		}` |
|        - |  3335 | `#endif` |
|        - |  3336 | `		/* Force a string cast */` |
|       19 |  3337 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3338 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3339 | `		}` |
|       19 |  3340 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3341 | `	}else{` |
|  2783360 |  3342 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3343 | `		/* Reserve a room for the target object */` |
|  2783360 |  3344 | `		pTos++;` |
|        - |  3345 | `	}` |
|        - |  3346 | `	/* Extract the requested memory object */` |
|  2783378 |  3347 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2783378 |  3348 | `	if( pObj == 0 ){` |
|       28 |  3349 | `		if( pInstr->iP1 ){` |
|        - |  3350 | `			/* Variable not found,load NULL */` |
|       28 |  3351 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3352 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3353 | `			}else{` |
|       28 |  3354 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3355 | `			}` |
|       28 |  3356 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1391593 |  3357 | `			break;` |
|      ! 0 |  3358 | `		}else{` |
|        - |  3359 | `			/* Fatal error */` |
|      ! 0 |  3360 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3361 | `			goto Abort;` |
|        - |  3362 | `		}` |
|        - |  3363 | `	}` |
|        - |  3364 | `	/* Load variable contents */` |
|  2783352 |  3365 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2783352 |  3366 | `	pTos->nIdx = pObj->nIdx;` |
|  2783352 |  3367 | `	break;` |
|        - |  3368 | `				   }` |
|        - |  3369 | `/*` |
|        - |  3370 | ` * LOAD_MAP P1 * *` |
|        - |  3371 | ` *` |
|        - |  3372 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3373 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3374 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3375 | ` */` |
|    19395 |  3376 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3377 | `	ph7_hashmap *pMap;` |
|        - |  3378 | `	/* Allocate a new hashmap instance */` |
|    38792 |  3379 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    38792 |  3380 | `	if( pMap == 0 ){` |
|      ! 0 |  3381 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3382 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3383 | `		goto Abort;` |
|        - |  3384 | `	}` |
|    38792 |  3385 | `	if( pInstr->iP1 > 0 ){` |
|     2328 |  3386 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3387 | `		/* Perform the insertion */` |
|     7136 |  3388 | `		while( pEntry < pTos ){` |
|     4810 |  3389 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3390 | `				/* Insertion by reference */` |
|      142 |  3391 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3392 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3393 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3394 | `					);` |
|       48 |  3395 | `			}else{` |
|        - |  3396 | `				/* Standard insertion */` |
|     7073 |  3397 | `				PH7_HashmapInsert(pMap,` |
|     4714 |  3398 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2357 |  3399 | `					&pEntry[1]` |
|        - |  3400 | `				);` |
|        - |  3401 | `			}` |
|        - |  3402 | `			/* Next pair on the stack */` |
|     4810 |  3403 | `			pEntry += 2;` |
|        2 |  3404 | `		}` |
|        - |  3405 | `		/* Pop P1 elements */` |
|     2328 |  3406 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1163 |  3407 | `	}` |
|        - |  3408 | `	/* Push the hashmap */` |
|    38792 |  3409 | `	pTos++;` |
|    38792 |  3410 | `	pTos->nIdx = SXU32_HIGH;` |
|    38792 |  3411 | `	pTos->x.pOther = pMap;` |
|    38792 |  3412 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    38792 |  3413 | `	break;` |
|        - |  3414 | `					  }` |
|        - |  3415 | `/*` |
|        - |  3416 | ` * LOAD_LIST: P1 * *` |
|        - |  3417 | ` *` |
|        - |  3418 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3419 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3420 | ` * Caveats:` |
|        - |  3421 | ` *  This implementation support only a single nesting level.` |
|        - |  3422 | ` */` |
|       48 |  3423 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3424 | `	ph7_value *pEntry;` |
|       98 |  3425 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3426 | `		/* Empty list,break immediately */` |
|      ! 0 |  3427 | `		break;` |
|        - |  3428 | `	}` |
|       98 |  3429 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3430 | `#ifdef UNTRUST` |
|        - |  3431 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3432 | `		goto Abort;` |
|        - |  3433 | `	}` |
|        - |  3434 | `#endif` |
|       98 |  3435 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  3436 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3437 | `		ph7_hashmap_node *pNode;` |
|        - |  3438 | `		ph7_value sKey,*pObj;` |
|        - |  3439 | `		/* Start Copying */` |
|       91 |  3440 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  3441 | `		while( pEntry <= pTos ){` |
|      193 |  3442 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  3443 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  3444 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  3445 | `					if( rc == SXRET_OK ){` |
|        - |  3446 | `						/* Store node value */` |
|      165 |  3447 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  3448 | `					}else{` |
|        - |  3449 | `						/* Undefined array key */` |
|        - |  3450 | `						char zMsg[128];` |
|      ! 0 |  3451 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  3452 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3453 | `						PH7_MemObjRelease(pObj);` |
|        - |  3454 | `					}` |
|       82 |  3455 | `				}` |
|       82 |  3456 | `			}` |
|      193 |  3457 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  3458 | `			pEntry++;` |
|        1 |  3459 | `		}` |
|       46 |  3460 | `	}else{` |
|        - |  3461 | `		/* Source is not an array */` |
|        - |  3462 | `		ph7_value *pObj;` |
|       18 |  3463 | `		while( pEntry <= pTos ){` |
|       12 |  3464 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  3465 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  3466 | `					PH7_MemObjRelease(pObj);` |
|        5 |  3467 | `				}` |
|        5 |  3468 | `			}` |
|       12 |  3469 | `			pEntry++;` |
|        2 |  3470 | `		}` |
|        8 |  3471 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  3472 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  3473 | `			const char *zType = "unknown";` |
|        3 |  3474 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  3475 | `			char zMsg[256];` |
|        3 |  3476 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  3477 | `				zType = "string";` |
|        1 |  3478 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  3479 | `				zType = "int";` |
|      ! 0 |  3480 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3481 | `				zType = "float";` |
|      ! 0 |  3482 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3483 | `				zType = "object";` |
|      ! 0 |  3484 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  3485 | `				zType = "resource";` |
|      ! 0 |  3486 | `			}` |
|        3 |  3487 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  3488 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  3489 | `		}` |
|        - |  3490 | `	}` |
|       98 |  3491 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  3492 | `	break;` |
|        - |  3493 | `					   }` |
|        - |  3494 | `/*` |
|        - |  3495 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3496 | ` *` |
|        - |  3497 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3498 | ` * from the stack.` |
|        - |  3499 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3500 | ` * instead.` |
|        - |  3501 | ` */` |
|   223010 |  3502 | `case PH7_OP_LOAD_IDX: {` |
|   446066 |  3503 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   446066 |  3504 | `	ph7_hashmap *pMap = 0;` |
|        - |  3505 | `	ph7_value *pIdx;` |
|   446066 |  3506 | `	pIdx = 0;` |
|   446066 |  3507 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3508 | `		if( !pInstr->iP2){` |
|        - |  3509 | `			/* No available index,load NULL */` |
|      ! 0 |  3510 | `			if( pTos >= pStack ){` |
|      ! 0 |  3511 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3512 | `			}else{` |
|        - |  3513 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3514 | `				pTos++;` |
|      ! 0 |  3515 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3516 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3517 | `			}` |
|        - |  3518 | `			/* Emit a notice */` |
|      ! 0 |  3519 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3520 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3521 | `			break;` |
|        - |  3522 | `		}` |
|      ! 0 |  3523 | `	}else{` |
|   446066 |  3524 | `		pIdx = pTos;` |
|   446066 |  3525 | `		pTos--;` |
|        - |  3526 | `	}` |
|   446066 |  3527 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3528 | `		/* String access */` |
|   349608 |  3529 | `		if( pIdx ){` |
|        - |  3530 | `			sxu32 nOfft;` |
|   349608 |  3531 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3532 | `				/* Force an int cast */` |
|      ! 0 |  3533 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3534 | `			}` |
|   349608 |  3535 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   349608 |  3536 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3537 | `				/* Invalid offset,load null */` |
|      ! 0 |  3538 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3539 | `			}else{` |
|   349608 |  3540 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   349608 |  3541 | `				int c = zData[nOfft];` |
|   349608 |  3542 | `				PH7_MemObjRelease(pTos);` |
|   349608 |  3543 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   349608 |  3544 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3545 | `			}` |
|   174827 |  3546 | `		}else{` |
|        - |  3547 | `			/* No available index,load NULL */` |
|      ! 0 |  3548 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3549 | `		}` |
|   349608 |  3550 | `		break;` |
|        - |  3551 | `	}` |
|    96460 |  3552 | `	if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3553 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3554 | `			ph7_value *pObj;` |
|      ! 0 |  3555 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3556 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3557 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3558 | `			}` |
|      ! 0 |  3559 | `		}` |
|      ! 0 |  3560 | `	}` |
|    96460 |  3561 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    96460 |  3562 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    96460 |  3563 | `		if( pInstr->iP2 == 1 ){` |
|        - |  3564 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3565 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3566 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3567 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3568 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3569 | `		}` |
|        - |  3570 | `		/* Point to the hashmap */` |
|    96460 |  3571 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    96460 |  3572 | `		if( pIdx ){` |
|        - |  3573 | `			/* Load the desired entry */` |
|    96460 |  3574 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    48229 |  3575 | `		}` |
|    96460 |  3576 | `		if( rc != SXRET_OK && pInstr->iP2 == 1 ){` |
|        - |  3577 | `			/* Create a new empty entry */` |
|      265 |  3578 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3579 | `			if( rc == SXRET_OK ){` |
|        - |  3580 | `				/* Point to the last inserted entry */` |
|      265 |  3581 | `				pNode = pMap->pLast;` |
|      132 |  3582 | `			}` |
|      132 |  3583 | `		}` |
|    48229 |  3584 | `	}` |
|    96460 |  3585 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  3586 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  3587 | `		char zMsg[128];` |
|      ! 0 |  3588 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3589 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3590 | `		}` |
|      ! 0 |  3591 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  3592 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3593 | `	}` |
|    96460 |  3594 | `	if( pIdx ){` |
|    96460 |  3595 | `		PH7_MemObjRelease(pIdx);` |
|    48229 |  3596 | `	}` |
|    96460 |  3597 | `	if( rc == SXRET_OK ){` |
|        - |  3598 | `		/* Load entry contents */` |
|    43790 |  3599 | `		if( pMap->iRef < 2 ){` |
|        - |  3600 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3601 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3602 | `			 */` |
|       24 |  3603 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3604 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3605 | `		}else{` |
|    43768 |  3606 | `			pTos->nIdx = pNode->nValIdx;` |
|    43768 |  3607 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    43768 |  3608 | `			PH7_HashmapUnref(pMap);` |
|        - |  3609 | `		}` |
|    21896 |  3610 | `	}else{` |
|        - |  3611 | `		/* No such entry,load NULL */` |
|    52672 |  3612 | `		PH7_MemObjRelease(pTos);` |
|    52672 |  3613 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3614 | `	}` |
|    96460 |  3615 | `	break;` |
|        - |  3616 | `					  }` |
|        - |  3617 | `/*` |
|        - |  3618 | ` * LOAD_CLOSURE * * P3` |
|        - |  3619 | ` *` |
|        - |  3620 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3621 | ` * name in the stack.` |
|        - |  3622 | ` */` |
|        5 |  3623 | `case PH7_OP_LOAD_CLOSURE:{` |
|       11 |  3624 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       11 |  3625 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3626 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3627 | `		ph7_vm_func *pClosure;` |
|        - |  3628 | `		char *zName;` |
|        - |  3629 | `		sxu32 mLen;` |
|        - |  3630 | `		sxu32 n;` |
|        - |  3631 | `		/* Create a new VM function */` |
|       11 |  3632 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3633 | `		/* Generate an unique closure name */` |
|       11 |  3634 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       11 |  3635 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3636 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3637 | `			goto Abort;` |
|        - |  3638 | `		}` |
|       11 |  3639 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       11 |  3640 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3641 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3642 | `		}` |
|        - |  3643 | `		/* Zero the stucture */` |
|       11 |  3644 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3645 | `		/* Perform a structure assignment on read-only items */` |
|       11 |  3646 | `		pClosure->aArgs = pFunc->aArgs;` |
|       11 |  3647 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       11 |  3648 | `		pClosure->aStatic = pFunc->aStatic;` |
|       11 |  3649 | `		pClosure->iFlags = pFunc->iFlags;` |
|       11 |  3650 | `		pClosure->pUserData = pFunc->pUserData;` |
|       11 |  3651 | `		pClosure->sSignature = pFunc->sSignature;` |
|       11 |  3652 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       11 |  3653 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       11 |  3654 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3655 | `		/* Register the closure */` |
|       11 |  3656 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3657 | `		/* Set up closure environment */` |
|       11 |  3658 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       11 |  3659 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       35 |  3660 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3661 | `			ph7_value *pValue;` |
|       25 |  3662 | `			pEnv = &aEnv[n];` |
|       25 |  3663 | `			sEnv.sName  = pEnv->sName;` |
|       25 |  3664 | `			sEnv.iFlags = pEnv->iFlags;` |
|       25 |  3665 | `			sEnv.nIdx = SXU32_HIGH;` |
|       25 |  3666 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       25 |  3667 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3668 | `				/* Pass by reference */` |
|      ! 0 |  3669 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3670 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3671 | `					);` |
|      ! 0 |  3672 | `			}` |
|        - |  3673 | `			/* Standard pass by value */` |
|       25 |  3674 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       25 |  3675 | `			if( pValue ){` |
|        - |  3676 | `				/* Copy imported value */` |
|       15 |  3677 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        7 |  3678 | `			}` |
|        - |  3679 | `			/* Insert the imported variable */` |
|       25 |  3680 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       13 |  3681 | `		}` |
|        - |  3682 | `		/* Finally,load the closure name on the stack */` |
|       11 |  3683 | `		pTos++;` |
|       11 |  3684 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        5 |  3685 | `	}` |
|       11 |  3686 | `	break;` |
|        - |  3687 | `						 }` |
|        - |  3688 | `/*` |
|        - |  3689 | ` * STORE * P2 P3` |
|        - |  3690 | ` *` |
|        - |  3691 | ` * Perform a store (Assignment) operation.` |
|        - |  3692 | ` */` |
|   119387 |  3693 | `case PH7_OP_STORE: {` |
|        - |  3694 | `	ph7_value *pObj;` |
|        - |  3695 | `	SyString sName;` |
|        - |  3696 | `#ifdef UNTRUST` |
|        - |  3697 | `	if( pTos < pStack ){` |
|        - |  3698 | `		goto Abort;` |
|        - |  3699 | `	}` |
|        - |  3700 | `#endif` |
|   238776 |  3701 | `	if( pInstr->iP2 ){` |
|        - |  3702 | `		sxu32 nIdx;` |
|        - |  3703 | `		/* Member store operation */` |
|     3178 |  3704 | `		nIdx = pTos->nIdx;` |
|     3178 |  3705 | `		VmPopOperand(&pTos,1);` |
|     3178 |  3706 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3707 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3708 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3709 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3710 | `		}else{` |
|        - |  3711 | `			/* Point to the desired memory object */` |
|     3174 |  3712 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3174 |  3713 | `			if( pObj ){` |
|        - |  3714 | `				/* Perform the store operation */` |
|     3174 |  3715 | `				PH7_MemObjStore(pTos,pObj);` |
|     1586 |  3716 | `			}` |
|        - |  3717 | `		}` |
|   120977 |  3718 | `		break;` |
|   235600 |  3719 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3720 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3721 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3722 | `			/* Force a string cast */` |
|      ! 0 |  3723 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3724 | `		}` |
|        7 |  3725 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3726 | `		pTos--;` |
|        - |  3727 | `#ifdef UNTRUST` |
|        - |  3728 | `		if( pTos < pStack  ){` |
|        - |  3729 | `			goto Abort;` |
|        - |  3730 | `		}` |
|        - |  3731 | `#endif` |
|        4 |  3732 | `	}else{` |
|   235594 |  3733 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3734 | `	}` |
|        - |  3735 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   235600 |  3736 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   235600 |  3737 | `	if( pObj == 0 ){` |
|      ! 0 |  3738 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3739 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3740 | `		goto Abort;` |
|        - |  3741 | `	}` |
|   235600 |  3742 | `	if( !pInstr->p3 ){` |
|        7 |  3743 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3744 | `	}` |
|        - |  3745 | `	/* Perform the store operation */` |
|   235600 |  3746 | `	PH7_MemObjStore(pTos,pObj);` |
|   235600 |  3747 | `	break;` |
|        - |  3748 | `				   }` |
|        - |  3749 | `/*` |
|        - |  3750 | ` * STORE_IDX:   P1 * P3` |
|        - |  3751 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3752 | ` *` |
|        - |  3753 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3754 | ` */` |
|    85717 |  3755 | `case PH7_OP_STORE_IDX:` |
|        - |  3756 | `case PH7_OP_STORE_IDX_REF: {` |
|   171436 |  3757 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3758 | `	ph7_value *pKey;` |
|        - |  3759 | `	sxu32 nIdx;` |
|   171436 |  3760 | `	if( pInstr->iP1 ){` |
|        - |  3761 | `		/* Key is next on stack */` |
|    58822 |  3762 | `		pKey = pTos;` |
|    58822 |  3763 | `		pTos--;` |
|    29412 |  3764 | `	}else{` |
|   112616 |  3765 | `		pKey = 0;` |
|        - |  3766 | `	}` |
|   171436 |  3767 | `	nIdx = pTos->nIdx;` |
|   171436 |  3768 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3769 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3770 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3771 | `		 * checking true sharing count, then re-add after separation. */` |
|   171384 |  3772 | `		if( nIdx != SXU32_HIGH ){` |
|   171384 |  3773 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   257075 |  3774 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   171384 |  3775 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3776 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3777 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3778 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3779 | `				 * refcounts if the backing array was already separated. */` |
|   171384 |  3780 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   171384 |  3781 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   171384 |  3782 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   171384 |  3783 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   171384 |  3784 | `					pTos->x.pOther = pMap;` |
|    85693 |  3785 | `				}else{` |
|        - |  3786 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3787 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3788 | `					pMap = pCur;` |
|        - |  3789 | `				}` |
|    85693 |  3790 | `			}else{` |
|      ! 0 |  3791 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3792 | `			}` |
|    85693 |  3793 | `		}else{` |
|      ! 0 |  3794 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3795 | `		}` |
|   171384 |  3796 | `		if( pMap->iRef < 2 ){` |
|        - |  3797 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3798 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3799 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3800 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3801 | `			pMap->iRef = 2;` |
|      ! 0 |  3802 | `		}` |
|    85693 |  3803 | `	}else{` |
|        - |  3804 | `		ph7_value *pObj;` |
|       53 |  3805 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3806 | `		if( pObj == 0 ){` |
|      ! 0 |  3807 | `			if( pKey ){` |
|      ! 0 |  3808 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3809 | `			}` |
|      ! 0 |  3810 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3811 | `			break;` |
|        - |  3812 | `		}` |
|        - |  3813 | `		/* Phase#1: Load the array */` |
|       53 |  3814 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3815 | `			VmPopOperand(&pTos,1);` |
|       53 |  3816 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3817 | `				/* Force a string cast */` |
|      ! 0 |  3818 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3819 | `			}` |
|       53 |  3820 | `			if( pKey == 0 ){` |
|        - |  3821 | `				/* Append string */` |
|        3 |  3822 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3823 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3824 | `				}` |
|        2 |  3825 | `			}else{` |
|        - |  3826 | `				sxu32 nOfft;` |
|       51 |  3827 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3828 | `					/* Force an int cast */` |
|       51 |  3829 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3830 | `				}` |
|       51 |  3831 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3832 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3833 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3834 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3835 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3836 | `				}else{` |
|      ! 0 |  3837 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3838 | `						/* Perform an append operation */` |
|      ! 0 |  3839 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3840 | `					}` |
|        - |  3841 | `				}` |
|        - |  3842 | `			}` |
|       53 |  3843 | `			if( pKey ){` |
|       51 |  3844 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3845 | `			}` |
|       53 |  3846 | `			break;` |
|      ! 0 |  3847 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3848 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3849 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3850 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3851 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3852 | `				goto Abort;` |
|        - |  3853 | `			}` |
|      ! 0 |  3854 | `		}` |
|        - |  3855 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3856 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3857 | `	}` |
|   171384 |  3858 | `	VmPopOperand(&pTos,1);` |
|        - |  3859 | `	/* Phase#2: Perform the insertion */` |
|   171384 |  3860 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3861 | `		/* Insertion by reference */` |
|       15 |  3862 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3863 | `	}else{` |
|   171370 |  3864 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3865 | `	}` |
|   171384 |  3866 | `	if( pKey ){` |
|    58772 |  3867 | `		PH7_MemObjRelease(pKey);` |
|    29385 |  3868 | `	}` |
|   171384 |  3869 | `	break;` |
|        - |  3870 | `					   }` |
|        - |  3871 | `/*` |
|        - |  3872 | ` * INCR: P1 * *` |
|        - |  3873 | ` *` |
|        - |  3874 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3875 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3876 | ` * the stack and increment after that.` |
|        - |  3877 | ` */` |
|   154733 |  3878 | `case PH7_OP_INCR:` |
|        - |  3879 | `#ifdef UNTRUST` |
|        - |  3880 | `	if( pTos < pStack ){` |
|        - |  3881 | `		goto Abort;` |
|        - |  3882 | `	}` |
|        - |  3883 | `#endif` |
|   309512 |  3884 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   309512 |  3885 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3886 | `			ph7_value *pObj;` |
|   309512 |  3887 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3888 | `				/* Force a numeric cast */` |
|   309512 |  3889 | `				PH7_MemObjToNumeric(pObj);` |
|   309512 |  3890 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3891 | `					pObj->rVal++;` |
|        - |  3892 | `					/* Try to get an integer representation */` |
|      ! 0 |  3893 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3894 | `				}else{` |
|   309512 |  3895 | `					pObj->x.iVal++;` |
|   309512 |  3896 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3897 | `				}` |
|   309512 |  3898 | `				if( pInstr->iP1 ){` |
|        - |  3899 | `					/* Pre-icrement */` |
|       71 |  3900 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3901 | `				}` |
|   154777 |  3902 | `			}` |
|   154779 |  3903 | `		}else{` |
|      ! 0 |  3904 | `			if( pInstr->iP1 ){` |
|        - |  3905 | `				/* Force a numeric cast */` |
|      ! 0 |  3906 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3907 | `				/* Pre-increment */` |
|      ! 0 |  3908 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3909 | `					pTos->rVal++;` |
|        - |  3910 | `					/* Try to get an integer representation */` |
|      ! 0 |  3911 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3912 | `				}else{` |
|      ! 0 |  3913 | `					pTos->x.iVal++;` |
|      ! 0 |  3914 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3915 | `				}` |
|      ! 0 |  3916 | `			}` |
|        - |  3917 | `		}` |
|   154777 |  3918 | `	}` |
|   309512 |  3919 | `	break;` |
|        - |  3920 | `/*` |
|        - |  3921 | ` * DECR: P1 * *` |
|        - |  3922 | ` *` |
|        - |  3923 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3924 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3925 | ` * and decrement after that.` |
|        - |  3926 | ` */` |
|        2 |  3927 | `case PH7_OP_DECR:` |
|        - |  3928 | `#ifdef UNTRUST` |
|        - |  3929 | `	if( pTos < pStack ){` |
|        - |  3930 | `		goto Abort;` |
|        - |  3931 | `	}` |
|        - |  3932 | `#endif` |
|        5 |  3933 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3934 | `		/* Force a numeric cast */` |
|        5 |  3935 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3936 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3937 | `			ph7_value *pObj;` |
|        5 |  3938 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3939 | `				/* Force a numeric cast */` |
|        5 |  3940 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3941 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3942 | `					pObj->rVal--;` |
|        - |  3943 | `					/* Try to get an integer representation */` |
|      ! 0 |  3944 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3945 | `				}else{` |
|        5 |  3946 | `					pObj->x.iVal--;` |
|        5 |  3947 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3948 | `				}` |
|        5 |  3949 | `				if( pInstr->iP1 ){` |
|        - |  3950 | `					/* Pre-icrement */` |
|      ! 0 |  3951 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3952 | `				}` |
|        2 |  3953 | `			}` |
|        3 |  3954 | `		}else{` |
|      ! 0 |  3955 | `			if( pInstr->iP1 ){` |
|        - |  3956 | `				/* Pre-increment */` |
|      ! 0 |  3957 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3958 | `					pTos->rVal--;` |
|        - |  3959 | `					/* Try to get an integer representation */` |
|      ! 0 |  3960 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3961 | `				}else{` |
|      ! 0 |  3962 | `					pTos->x.iVal--;` |
|      ! 0 |  3963 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3964 | `				}` |
|      ! 0 |  3965 | `			}` |
|        - |  3966 | `		}` |
|        2 |  3967 | `	}` |
|        5 |  3968 | `	break;` |
|        - |  3969 | `/*` |
|        - |  3970 | ` * UMINUS: * * *` |
|        - |  3971 | ` *` |
|        - |  3972 | ` * Perform a unary minus operation.` |
|        - |  3973 | ` */` |
|    25133 |  3974 | `case PH7_OP_UMINUS:` |
|        - |  3975 | `#ifdef UNTRUST` |
|        - |  3976 | `	if( pTos < pStack ){` |
|        - |  3977 | `		goto Abort;` |
|        - |  3978 | `	}` |
|        - |  3979 | `#endif` |
|        - |  3980 | `	/* Force a numeric (integer,real or both) cast */` |
|    50268 |  3981 | `	PH7_MemObjToNumeric(pTos);` |
|    50268 |  3982 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3983 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3984 | `	}` |
|    50268 |  3985 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    50238 |  3986 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    25118 |  3987 | `	}` |
|    50268 |  3988 | `	break;` |
|        - |  3989 | `/*` |
|        - |  3990 | ` * UPLUS: * * *` |
|        - |  3991 | ` *` |
|        - |  3992 | ` * Perform a unary plus operation.` |
|        - |  3993 | ` */` |
|       18 |  3994 | `case PH7_OP_UPLUS:` |
|        - |  3995 | `#ifdef UNTRUST` |
|        - |  3996 | `	if( pTos < pStack ){` |
|        - |  3997 | `		goto Abort;` |
|        - |  3998 | `	}` |
|        - |  3999 | `#endif` |
|        - |  4000 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4001 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4002 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4003 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4004 | `	}` |
|       37 |  4005 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4006 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4007 | `	}` |
|       37 |  4008 | `	break;` |
|        - |  4009 | `/*` |
|        - |  4010 | ` * OP_LNOT: * * *` |
|        - |  4011 | ` *` |
|        - |  4012 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4013 | ` * with its complement.` |
|        - |  4014 | ` */` |
|    40980 |  4015 | `case PH7_OP_LNOT:` |
|        - |  4016 | `#ifdef UNTRUST` |
|        - |  4017 | `	if( pTos < pStack ){` |
|        - |  4018 | `		goto Abort;` |
|        - |  4019 | `	}` |
|        - |  4020 | `#endif` |
|        - |  4021 | `	/* Force a boolean cast */` |
|    82006 |  4022 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4023 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4024 | `	}` |
|    82006 |  4025 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    82006 |  4026 | `	break;` |
|        - |  4027 | `/*` |
|        - |  4028 | ` * OP_BITNOT: * * *` |
|        - |  4029 | ` *` |
|        - |  4030 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4031 | ` * with its ones-complement.` |
|        - |  4032 | ` */` |
|       13 |  4033 | `case PH7_OP_BITNOT:` |
|        - |  4034 | `#ifdef UNTRUST` |
|        - |  4035 | `	if( pTos < pStack ){` |
|        - |  4036 | `		goto Abort;` |
|        - |  4037 | `	}` |
|        - |  4038 | `#endif` |
|        - |  4039 | `	/* Force an integer cast */` |
|       28 |  4040 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4041 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4042 | `	}` |
|       28 |  4043 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4044 | `	break;` |
|        - |  4045 | `/* OP_MUL * * *` |
|        - |  4046 | ` * OP_MUL_STORE * * *` |
|        - |  4047 | ` *` |
|        - |  4048 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4049 | ` * and push the result back onto the stack.` |
|        - |  4050 | ` */` |
|     1255 |  4051 | `case PH7_OP_MUL:` |
|        - |  4052 | `case PH7_OP_MUL_STORE: {` |
|     2512 |  4053 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4054 | `	/* Force the operand to be numeric */` |
|        - |  4055 | `#ifdef UNTRUST` |
|        - |  4056 | `	if( pNos < pStack ){` |
|        - |  4057 | `		goto Abort;` |
|        - |  4058 | `	}` |
|        - |  4059 | `#endif` |
|     2512 |  4060 | `	PH7_MemObjToNumeric(pTos);` |
|     2512 |  4061 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4062 | `	/* Perform the requested operation */` |
|     2512 |  4063 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4064 | `		/* Floating point arithemic */` |
|        - |  4065 | `		ph7_real a,b,r;` |
|       17 |  4066 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4067 | `			PH7_MemObjToReal(pTos);` |
|        3 |  4068 | `		}` |
|       17 |  4069 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4070 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4071 | `		}` |
|       17 |  4072 | `		a = pNos->rVal;` |
|       17 |  4073 | `		b = pTos->rVal;` |
|       17 |  4074 | `		r = a * b;` |
|        - |  4075 | `		/* Push the result */` |
|       17 |  4076 | `		pNos->rVal = r;` |
|       17 |  4077 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4078 | `		/* Try to get an integer representation */` |
|       17 |  4079 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  4080 | `	}else{` |
|        - |  4081 | `		/* Integer arithmetic */` |
|        - |  4082 | `		sxi64 a,b,r;` |
|     2496 |  4083 | `		a = pNos->x.iVal;` |
|     2496 |  4084 | `		b = pTos->x.iVal;` |
|     2496 |  4085 | `		r = a * b;` |
|        - |  4086 | `		/* Push the result */` |
|     2496 |  4087 | `		pNos->x.iVal = r;` |
|     2496 |  4088 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4089 | `	}` |
|     2512 |  4090 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4091 | `		ph7_value *pObj;` |
|       29 |  4092 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4093 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       29 |  4094 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       29 |  4095 | `			PH7_MemObjStore(pNos,pObj);` |
|       14 |  4096 | `		}` |
|       14 |  4097 | `	}` |
|     2512 |  4098 | `	VmPopOperand(&pTos,1);` |
|     2512 |  4099 | `	break;` |
|        - |  4100 | `				 }` |
|        - |  4101 | `/* OP_ADD * * *` |
|        - |  4102 | ` *` |
|        - |  4103 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4104 | ` * and push the result back onto the stack.` |
|        - |  4105 | ` */` |
|      464 |  4106 | `case PH7_OP_ADD:{` |
|      930 |  4107 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4108 | `#ifdef UNTRUST` |
|        - |  4109 | `	if( pNos < pStack ){` |
|        - |  4110 | `		goto Abort;` |
|        - |  4111 | `	}` |
|        - |  4112 | `#endif` |
|        - |  4113 | `	/* Perform the addition */` |
|      930 |  4114 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      930 |  4115 | `	VmPopOperand(&pTos,1);` |
|      930 |  4116 | `	break;` |
|        - |  4117 | `				}` |
|        - |  4118 | `/*` |
|        - |  4119 | ` * OP_ADD_STORE * * *` |
|        - |  4120 | ` *` |
|        - |  4121 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4122 | ` * and push the result back onto the stack.` |
|        - |  4123 | ` */` |
|      496 |  4124 | `case PH7_OP_ADD_STORE:{` |
|      994 |  4125 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4126 | `	ph7_value *pObj;` |
|        - |  4127 | `	sxu32 nIdx;` |
|        - |  4128 | `#ifdef UNTRUST` |
|        - |  4129 | `	if( pNos < pStack ){` |
|        - |  4130 | `		goto Abort;` |
|        - |  4131 | `	}` |
|        - |  4132 | `#endif` |
|        - |  4133 | `	/* Perform the addition */` |
|      994 |  4134 | `	nIdx = pTos->nIdx;` |
|      994 |  4135 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4136 | `	/* Peform the store operation */` |
|      994 |  4137 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4138 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      994 |  4139 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      994 |  4140 | `		PH7_MemObjStore(pTos,pObj);` |
|      496 |  4141 | `	}` |
|        - |  4142 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      994 |  4143 | `	PH7_MemObjStore(pTos,pNos);` |
|      994 |  4144 | `	VmPopOperand(&pTos,1);` |
|      994 |  4145 | `	break;` |
|        - |  4146 | `				}` |
|        - |  4147 | `/* OP_SUB * * *` |
|        - |  4148 | ` *` |
|        - |  4149 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4150 | ` * first (what was next on the stack) from the second (the` |
|        - |  4151 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4152 | ` */` |
|      302 |  4153 | `case PH7_OP_SUB: {` |
|      606 |  4154 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4155 | `#ifdef UNTRUST` |
|        - |  4156 | `	if( pNos < pStack ){` |
|        - |  4157 | `		goto Abort;` |
|        - |  4158 | `	}` |
|        - |  4159 | `#endif` |
|      606 |  4160 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4161 | `		/* Floating point arithemic */` |
|        - |  4162 | `		ph7_real a,b,r;` |
|       95 |  4163 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4164 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4165 | `		}` |
|       95 |  4166 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4167 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4168 | `		}` |
|       95 |  4169 | `		a = pNos->rVal;` |
|       95 |  4170 | `		b = pTos->rVal;` |
|       95 |  4171 | `		r = a - b;` |
|        - |  4172 | `		/* Push the result */` |
|       95 |  4173 | `		pNos->rVal = r;` |
|       95 |  4174 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4175 | `		/* Try to get an integer representation */` |
|       95 |  4176 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4177 | `	}else{` |
|        - |  4178 | `		/* Integer arithmetic */` |
|        - |  4179 | `		sxi64 a,b,r;` |
|      512 |  4180 | `		a = pNos->x.iVal;` |
|      512 |  4181 | `		b = pTos->x.iVal;` |
|      512 |  4182 | `		r = a - b;` |
|        - |  4183 | `		/* Push the result */` |
|      512 |  4184 | `		pNos->x.iVal = r;` |
|      512 |  4185 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4186 | `	}` |
|      606 |  4187 | `	VmPopOperand(&pTos,1);` |
|      606 |  4188 | `	break;` |
|        - |  4189 | `				 }` |
|        - |  4190 | `/* OP_SUB_STORE * * *` |
|        - |  4191 | ` *` |
|        - |  4192 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4193 | ` * first (what was next on the stack) from the second (the` |
|        - |  4194 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4195 | ` */` |
|        3 |  4196 | `case PH7_OP_SUB_STORE: {` |
|        7 |  4197 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4198 | `	ph7_value *pObj;` |
|        - |  4199 | `#ifdef UNTRUST` |
|        - |  4200 | `	if( pNos < pStack ){` |
|        - |  4201 | `		goto Abort;` |
|        - |  4202 | `	}` |
|        - |  4203 | `#endif` |
|        7 |  4204 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4205 | `		/* Floating point arithemic */` |
|        - |  4206 | `		ph7_real a,b,r;` |
|      ! 0 |  4207 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4208 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4209 | `		}` |
|      ! 0 |  4210 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4211 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4212 | `		}` |
|      ! 0 |  4213 | `		a = pTos->rVal;` |
|      ! 0 |  4214 | `		b = pNos->rVal;` |
|      ! 0 |  4215 | `		r = a - b;` |
|        - |  4216 | `		/* Push the result */` |
|      ! 0 |  4217 | `		pNos->rVal = r;` |
|      ! 0 |  4218 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4219 | `		/* Try to get an integer representation */` |
|      ! 0 |  4220 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4221 | `	}else{` |
|        - |  4222 | `		/* Integer arithmetic */` |
|        - |  4223 | `		sxi64 a,b,r;` |
|        7 |  4224 | `		a = pTos->x.iVal;` |
|        7 |  4225 | `		b = pNos->x.iVal;` |
|        7 |  4226 | `		r = a - b;` |
|        - |  4227 | `		/* Push the result */` |
|        7 |  4228 | `		pNos->x.iVal = r;` |
|        7 |  4229 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4230 | `	}` |
|        7 |  4231 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4232 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        7 |  4233 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        7 |  4234 | `		PH7_MemObjStore(pNos,pObj);` |
|        3 |  4235 | `	}` |
|        7 |  4236 | `	VmPopOperand(&pTos,1);` |
|        7 |  4237 | `	break;` |
|        - |  4238 | `				 }` |
|        - |  4239 |  |
|        - |  4240 | `/*` |
|        - |  4241 | ` * OP_MOD * * *` |
|        - |  4242 | ` *` |
|        - |  4243 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4244 | ` * first (what was next on the stack) from the second (the` |
|        - |  4245 | ` * top of the stack) and push the remainder after division` |
|        - |  4246 | ` * onto the stack.` |
|        - |  4247 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4248 | ` */` |
|      307 |  4249 | `case PH7_OP_MOD:{` |
|      616 |  4250 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4251 | `	sxi64 a,b,r;` |
|        - |  4252 | `#ifdef UNTRUST` |
|        - |  4253 | `	if( pNos < pStack ){` |
|        - |  4254 | `		goto Abort;` |
|        - |  4255 | `	}` |
|        - |  4256 | `#endif` |
|        - |  4257 | `	/* Force the operands to be integer */` |
|      616 |  4258 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4259 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4260 | `	}` |
|      616 |  4261 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4262 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4263 | `	}` |
|        - |  4264 | `	/* Perform the requested operation */` |
|      616 |  4265 | `	a = pNos->x.iVal;` |
|      616 |  4266 | `	b = pTos->x.iVal;` |
|      616 |  4267 | `	if( b == 0 ){` |
|        3 |  4268 | `		r = 0;` |
|        3 |  4269 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4270 | `		/* goto Abort; */` |
|        2 |  4271 | `	}else{` |
|      613 |  4272 | `		r = a%b;` |
|        - |  4273 | `	}` |
|        - |  4274 | `	/* Push the result */` |
|      616 |  4275 | `	pNos->x.iVal = r;` |
|      616 |  4276 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4277 | `	VmPopOperand(&pTos,1);` |
|      616 |  4278 | `	break;` |
|        - |  4279 | `				}` |
|        - |  4280 | `/*` |
|        - |  4281 | ` * OP_MOD_STORE * * *` |
|        - |  4282 | ` *` |
|        - |  4283 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4284 | ` * first (what was next on the stack) from the second (the` |
|        - |  4285 | ` * top of the stack) and push the remainder after division` |
|        - |  4286 | ` * onto the stack.` |
|        - |  4287 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4288 | ` */` |
|        1 |  4289 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4290 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4291 | `	ph7_value *pObj;` |
|        - |  4292 | `	sxi64 a,b,r;` |
|        - |  4293 | `#ifdef UNTRUST` |
|        - |  4294 | `	if( pNos < pStack ){` |
|        - |  4295 | `		goto Abort;` |
|        - |  4296 | `	}` |
|        - |  4297 | `#endif` |
|        - |  4298 | `	/* Force the operands to be integer */` |
|        3 |  4299 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4300 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4301 | `	}` |
|        3 |  4302 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4303 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4304 | `	}` |
|        - |  4305 | `	/* Perform the requested operation */` |
|        3 |  4306 | `	a = pTos->x.iVal;` |
|        3 |  4307 | `	b = pNos->x.iVal;` |
|        3 |  4308 | `	if( b == 0 ){` |
|      ! 0 |  4309 | `		r = 0;` |
|      ! 0 |  4310 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4311 | `		/* goto Abort; */` |
|      ! 0 |  4312 | `	}else{` |
|        3 |  4313 | `		r = a%b;` |
|        - |  4314 | `	}` |
|        - |  4315 | `	/* Push the result */` |
|        3 |  4316 | `	pNos->x.iVal = r;` |
|        3 |  4317 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4318 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4319 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4320 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4321 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4322 | `	}` |
|        3 |  4323 | `	VmPopOperand(&pTos,1);` |
|        3 |  4324 | `	break;` |
|        - |  4325 | `				}` |
|        - |  4326 | `/*` |
|        - |  4327 | ` * OP_DIV * * *` |
|        - |  4328 | ` *` |
|        - |  4329 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4330 | ` * first (what was next on the stack) from the second (the` |
|        - |  4331 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4332 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4333 | ` */` |
|       30 |  4334 | `case PH7_OP_DIV:{` |
|       62 |  4335 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4336 | `	ph7_real a,b,r;` |
|        - |  4337 | `#ifdef UNTRUST` |
|        - |  4338 | `	if( pNos < pStack ){` |
|        - |  4339 | `		goto Abort;` |
|        - |  4340 | `	}` |
|        - |  4341 | `#endif` |
|        - |  4342 | `	/* Force the operands to be real */` |
|       62 |  4343 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  4344 | `		PH7_MemObjToReal(pTos);` |
|       28 |  4345 | `	}` |
|       62 |  4346 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  4347 | `		PH7_MemObjToReal(pNos);` |
|       11 |  4348 | `	}` |
|        - |  4349 | `	/* Perform the requested operation */` |
|       62 |  4350 | `	a = pNos->rVal;` |
|       62 |  4351 | `	b = pTos->rVal;` |
|       62 |  4352 | `	if( b == 0 ){` |
|        - |  4353 | `		/* Division by zero */` |
|        3 |  4354 | `		pNos->rVal = 0;` |
|        3 |  4355 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4356 | `		/* goto Abort; */` |
|        2 |  4357 | `	}else{` |
|       59 |  4358 | `		r = a/b;` |
|        - |  4359 | `		/* Push the result */` |
|       59 |  4360 | `		pNos->rVal = r;` |
|       59 |  4361 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4362 | `		/* Try to get an integer representation */` |
|       59 |  4363 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4364 | `	}` |
|       62 |  4365 | `	VmPopOperand(&pTos,1);` |
|       62 |  4366 | `	break;` |
|        - |  4367 | `				}` |
|        - |  4368 | `/*` |
|        - |  4369 | ` * OP_DIV_STORE * * *` |
|        - |  4370 | ` *` |
|        - |  4371 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4372 | ` * first (what was next on the stack) from the second (the` |
|        - |  4373 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4374 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4375 | ` */` |
|        2 |  4376 | `case PH7_OP_DIV_STORE:{` |
|        5 |  4377 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4378 | `	ph7_value *pObj;` |
|        - |  4379 | `	ph7_real a,b,r;` |
|        - |  4380 | `#ifdef UNTRUST` |
|        - |  4381 | `	if( pNos < pStack ){` |
|        - |  4382 | `		goto Abort;` |
|        - |  4383 | `	}` |
|        - |  4384 | `#endif` |
|        - |  4385 | `	/* Force the operands to be real */` |
|        5 |  4386 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4387 | `		PH7_MemObjToReal(pTos);` |
|        2 |  4388 | `	}` |
|        5 |  4389 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4390 | `		PH7_MemObjToReal(pNos);` |
|        2 |  4391 | `	}` |
|        - |  4392 | `	/* Perform the requested operation */` |
|        5 |  4393 | `	a = pTos->rVal;` |
|        5 |  4394 | `	b = pNos->rVal;` |
|        5 |  4395 | `	if( b == 0 ){` |
|        - |  4396 | `		/* Division by zero */` |
|      ! 0 |  4397 | `		r = 0;` |
|      ! 0 |  4398 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4399 | `		/* goto Abort; */` |
|      ! 0 |  4400 | `	}else{` |
|        5 |  4401 | `		r = a/b;` |
|        - |  4402 | `		/* Push the result */` |
|        5 |  4403 | `		pNos->rVal = r;` |
|        5 |  4404 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4405 | `		/* Try to get an integer representation */` |
|        5 |  4406 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4407 | `	}` |
|        5 |  4408 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4409 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4410 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4411 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4412 | `	}` |
|        5 |  4413 | `	VmPopOperand(&pTos,1);` |
|        5 |  4414 | `	break;` |
|        - |  4415 | `				}` |
|        - |  4416 | `/* OP_BAND * * *` |
|        - |  4417 | ` *` |
|        - |  4418 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4419 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4420 | ` * two elements.` |
|        - |  4421 | `*/` |
|        - |  4422 | `/* OP_BOR * * *` |
|        - |  4423 | ` *` |
|        - |  4424 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4425 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4426 | ` * two elements.` |
|        - |  4427 | ` */` |
|        - |  4428 | `/* OP_BXOR * * *` |
|        - |  4429 | ` *` |
|        - |  4430 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4431 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4432 | ` * two elements.` |
|        - |  4433 | ` */` |
|       44 |  4434 | `case PH7_OP_BAND:` |
|        - |  4435 | `case PH7_OP_BOR:` |
|        - |  4436 | `case PH7_OP_BXOR:{` |
|       90 |  4437 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4438 | `	sxi64 a,b,r;` |
|        - |  4439 | `#ifdef UNTRUST` |
|        - |  4440 | `	if( pNos < pStack ){` |
|        - |  4441 | `		goto Abort;` |
|        - |  4442 | `	}` |
|        - |  4443 | `#endif` |
|        - |  4444 | `	/* Force the operands to be integer */` |
|       90 |  4445 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4446 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4447 | `	}` |
|       90 |  4448 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4449 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4450 | `	}` |
|        - |  4451 | `	/* Perform the requested operation */` |
|       90 |  4452 | `	a = pNos->x.iVal;` |
|       90 |  4453 | `	b = pTos->x.iVal;` |
|       90 |  4454 | `	switch(pInstr->iOp){` |
|        7 |  4455 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4456 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4457 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4458 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4459 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4460 | `	case PH7_OP_BAND:` |
|       62 |  4461 | `	default:          r = a&b; break;` |
|        - |  4462 | `	}` |
|        - |  4463 | `	/* Push the result */` |
|       90 |  4464 | `	pNos->x.iVal = r;` |
|       90 |  4465 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4466 | `	VmPopOperand(&pTos,1);` |
|       90 |  4467 | `	break;` |
|        - |  4468 | `				 }` |
|        - |  4469 | `/* OP_BAND_STORE * * *` |
|        - |  4470 | ` *` |
|        - |  4471 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4472 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4473 | ` * two elements.` |
|        - |  4474 | `*/` |
|        - |  4475 | `/* OP_BOR_STORE * * *` |
|        - |  4476 | ` *` |
|        - |  4477 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4478 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4479 | ` * two elements.` |
|        - |  4480 | ` */` |
|        - |  4481 | `/* OP_BXOR_STORE * * *` |
|        - |  4482 | ` *` |
|        - |  4483 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4484 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4485 | ` * two elements.` |
|        - |  4486 | ` */` |
|       10 |  4487 | `case PH7_OP_BAND_STORE:` |
|        - |  4488 | `case PH7_OP_BOR_STORE:` |
|        - |  4489 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4490 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4491 | `	ph7_value *pObj;` |
|        - |  4492 | `	sxi64 a,b,r;` |
|        - |  4493 | `#ifdef UNTRUST` |
|        - |  4494 | `	if( pNos < pStack ){` |
|        - |  4495 | `		goto Abort;` |
|        - |  4496 | `	}` |
|        - |  4497 | `#endif` |
|        - |  4498 | `	/* Force the operands to be integer */` |
|       21 |  4499 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4500 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4501 | `	}` |
|       21 |  4502 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4503 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4504 | `	}` |
|        - |  4505 | `	/* Perform the requested operation */` |
|       21 |  4506 | `	a = pTos->x.iVal;` |
|       21 |  4507 | `	b = pNos->x.iVal;` |
|       21 |  4508 | `	switch(pInstr->iOp){` |
|        3 |  4509 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4510 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4511 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4512 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4513 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4514 | `	case PH7_OP_BAND:` |
|        7 |  4515 | `	default:          r = a&b; break;` |
|        - |  4516 | `	}` |
|        - |  4517 | `	/* Push the result */` |
|       21 |  4518 | `	pNos->x.iVal = r;` |
|       21 |  4519 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4520 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4521 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4522 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4523 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4524 | `	}` |
|       21 |  4525 | `	VmPopOperand(&pTos,1);` |
|       21 |  4526 | `	break;` |
|        - |  4527 | `				 }` |
|        - |  4528 | `/* OP_SHL * * *` |
|        - |  4529 | ` *` |
|        - |  4530 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4531 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4532 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4533 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4534 | ` */` |
|        - |  4535 | `/* OP_SHR * * *` |
|        - |  4536 | ` *` |
|        - |  4537 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4538 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4539 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4540 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4541 | ` */` |
|       12 |  4542 | `case PH7_OP_SHL:` |
|        - |  4543 | `case PH7_OP_SHR: {` |
|       25 |  4544 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4545 | `	sxi64 a,r;` |
|        - |  4546 | `	sxi32 b;` |
|        - |  4547 | `#ifdef UNTRUST` |
|        - |  4548 | `	if( pNos < pStack ){` |
|        - |  4549 | `		goto Abort;` |
|        - |  4550 | `	}` |
|        - |  4551 | `#endif` |
|        - |  4552 | `	/* Force the operands to be integer */` |
|       25 |  4553 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4554 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4555 | `	}` |
|       25 |  4556 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4557 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4558 | `	}` |
|        - |  4559 | `	/* Perform the requested operation */` |
|       25 |  4560 | `	a = pNos->x.iVal;` |
|       25 |  4561 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4562 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4563 | `		r = a << b;` |
|        8 |  4564 | `	}else{` |
|       11 |  4565 | `		r = a >> b;` |
|        - |  4566 | `	}` |
|        - |  4567 | `	/* Push the result */` |
|       25 |  4568 | `	pNos->x.iVal = r;` |
|       25 |  4569 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4570 | `	VmPopOperand(&pTos,1);` |
|       25 |  4571 | `	break;` |
|        - |  4572 | `				 }` |
|        - |  4573 | `/*  OP_SHL_STORE * * *` |
|        - |  4574 | ` *` |
|        - |  4575 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4576 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4577 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4578 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4579 | ` */` |
|        - |  4580 | `/* OP_SHR_STORE * * *` |
|        - |  4581 | ` *` |
|        - |  4582 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4583 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4584 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4585 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4586 | ` */` |
|        9 |  4587 | `case PH7_OP_SHL_STORE:` |
|        - |  4588 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4589 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4590 | `	ph7_value *pObj;` |
|        - |  4591 | `	sxi64 a,r;` |
|        - |  4592 | `	sxi32 b;` |
|        - |  4593 | `#ifdef UNTRUST` |
|        - |  4594 | `	if( pNos < pStack ){` |
|        - |  4595 | `		goto Abort;` |
|        - |  4596 | `	}` |
|        - |  4597 | `#endif` |
|        - |  4598 | `	/* Force the operands to be integer */` |
|       19 |  4599 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4600 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4601 | `	}` |
|       19 |  4602 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4603 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4604 | `	}` |
|        - |  4605 | `	/* Perform the requested operation */` |
|       19 |  4606 | `	a = pTos->x.iVal;` |
|       19 |  4607 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4608 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4609 | `		r = a << b;` |
|        5 |  4610 | `	}else{` |
|       11 |  4611 | `		r = a >> b;` |
|        - |  4612 | `	}` |
|        - |  4613 | `	/* Push the result */` |
|       19 |  4614 | `	pNos->x.iVal = r;` |
|       19 |  4615 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4616 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4617 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4618 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  4619 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  4620 | `	}` |
|       19 |  4621 | `	VmPopOperand(&pTos,1);` |
|       19 |  4622 | `	break;` |
|        - |  4623 | `				 }` |
|        - |  4624 | `/* CAT:  P1 * *` |
|        - |  4625 | ` *` |
|        - |  4626 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4627 | ` * back.` |
|        - |  4628 | ` */` |
|    64876 |  4629 | `case PH7_OP_CAT:{` |
|        - |  4630 | `	ph7_value *pNos,*pCur;` |
|   129754 |  4631 | `	if( pInstr->iP1 < 1 ){` |
|   102668 |  4632 | `		pNos = &pTos[-1];` |
|    51335 |  4633 | `	}else{` |
|    27088 |  4634 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4635 | `	}` |
|        - |  4636 | `#ifdef UNTRUST` |
|        - |  4637 | `	if( pNos < pStack ){` |
|        - |  4638 | `		goto Abort;` |
|        - |  4639 | `	}` |
|        - |  4640 | `#endif` |
|        - |  4641 | `	/* Force a string cast */` |
|   129754 |  4642 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1626 |  4643 | `		PH7_MemObjToString(pNos);` |
|      812 |  4644 | `	}` |
|   129754 |  4645 | `	pCur = &pNos[1];` |
|   261642 |  4646 | `	while( pCur <= pTos ){` |
|   131890 |  4647 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50728 |  4648 | `			PH7_MemObjToString(pCur);` |
|    25363 |  4649 | `		}` |
|        - |  4650 | `		/* Perform the concatenation */` |
|   131890 |  4651 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   131852 |  4652 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    65925 |  4653 | `		}` |
|   131890 |  4654 | `		SyBlobRelease(&pCur->sBlob);` |
|   131890 |  4655 | `		pCur++;` |
|        2 |  4656 | `	}` |
|   129754 |  4657 | `	pTos = pNos;` |
|   129754 |  4658 | `	break;` |
|        - |  4659 | `				}` |
|        - |  4660 | `/*  CAT_STORE: * * *` |
|        - |  4661 | ` *` |
|        - |  4662 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4663 | ` * back.` |
|        - |  4664 | ` */` |
|     3487 |  4665 | `case PH7_OP_CAT_STORE:{` |
|     6976 |  4666 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4667 | `	ph7_value *pObj;` |
|        - |  4668 | `#ifdef UNTRUST` |
|        - |  4669 | `	if( pNos < pStack ){` |
|        - |  4670 | `		goto Abort;` |
|        - |  4671 | `	}` |
|        - |  4672 | `#endif` |
|     6976 |  4673 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4674 | `		/* Force a string cast */` |
|      ! 0 |  4675 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4676 | `	}` |
|     6976 |  4677 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4678 | `		/* Force a string cast */` |
|      ! 0 |  4679 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4680 | `	}` |
|        - |  4681 | `	/* Perform the concatenation (Reverse order) */` |
|     6976 |  4682 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6976 |  4683 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3487 |  4684 | `	}` |
|        - |  4685 | `	/* Perform the store operation */` |
|     6976 |  4686 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4687 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6976 |  4688 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6976 |  4689 | `		PH7_MemObjStore(pTos,pObj);` |
|     3487 |  4690 | `	}` |
|     6976 |  4691 | `	PH7_MemObjStore(pTos,pNos);` |
|     6976 |  4692 | `	VmPopOperand(&pTos,1);` |
|     6976 |  4693 | `	break;` |
|        - |  4694 | `				}` |
|        - |  4695 | `/* OP_AND: * * *` |
|        - |  4696 | ` *` |
|        - |  4697 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4698 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4699 | ` * stack.` |
|        - |  4700 | ` */` |
|        - |  4701 | `/* OP_OR: * * *` |
|        - |  4702 | ` *` |
|        - |  4703 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4704 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4705 | ` * stack.` |
|        - |  4706 | ` */` |
|    97456 |  4707 | `case PH7_OP_LAND:` |
|        - |  4708 | `case PH7_OP_LOR: {` |
|   194958 |  4709 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4710 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4711 | `#ifdef UNTRUST` |
|        - |  4712 | `	if( pNos < pStack ){` |
|        - |  4713 | `		goto Abort;` |
|        - |  4714 | `	}` |
|        - |  4715 | `#endif` |
|        - |  4716 | `	/* Force a boolean cast */` |
|   194958 |  4717 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4718 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4719 | `	}` |
|   194958 |  4720 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4721 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4722 | `	}` |
|   194958 |  4723 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   194958 |  4724 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   194958 |  4725 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4726 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    89748 |  4727 | `		v1 = and_logic[v1*3+v2];` |
|    44897 |  4728 | `	}else{` |
|        - |  4729 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   105212 |  4730 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4731 | `	}` |
|   194958 |  4732 | `	if( v1 == 2 ){` |
|      ! 0 |  4733 | `		v1 = 1;` |
|      ! 0 |  4734 | `	}` |
|   194958 |  4735 | `	VmPopOperand(&pTos,1);` |
|   194958 |  4736 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   194958 |  4737 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   194958 |  4738 | `	break;` |
|        - |  4739 | `				 }` |
|        - |  4740 | `/*` |
|        - |  4741 | ` * OP_NULLC: * * *` |
|        - |  4742 | ` * Null coalescing operator '??'.` |
|        - |  4743 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4744 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4745 | ` */` |
|        - |  4746 | `/*` |
|        - |  4747 | ` * OP_NULLC: * P2 *` |
|        - |  4748 | ` * Short-circuit null coalescing '??'.` |
|        - |  4749 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4750 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4751 | ` */` |
|       19 |  4752 | `case PH7_OP_NULLC: {` |
|        - |  4753 | `#ifdef UNTRUST` |
|        - |  4754 | `	if( pTos < pStack ){` |
|        - |  4755 | `		goto Abort;` |
|        - |  4756 | `	}` |
|        - |  4757 | `#endif` |
|       40 |  4758 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4759 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  4760 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  4761 | `	}else{` |
|        - |  4762 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  4763 | `		VmPopOperand(&pTos, 1);` |
|        - |  4764 | `	}` |
|       40 |  4765 | `	break;` |
|        - |  4766 |  |
|        - |  4767 | `/*` |
|        - |  4768 | ` * OP_SPREAD: * * *` |
|        - |  4769 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4770 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4771 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4772 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4773 | ` */` |
|        7 |  4774 | `case PH7_OP_SPREAD: {` |
|        - |  4775 | `#ifdef UNTRUST` |
|        - |  4776 | `	if( pTos < pStack ){` |
|        - |  4777 | `		goto Abort;` |
|        - |  4778 | `	}` |
|        - |  4779 | `#endif` |
|       15 |  4780 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4781 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4782 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4783 | `		if( nEntry == 0 ){` |
|        - |  4784 | `			/* Empty array — remove from stack */` |
|        3 |  4785 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4786 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4787 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4788 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4789 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4790 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4791 | `				VM_STACK_GUARD);` |
|      ! 0 |  4792 | `		}else{` |
|        - |  4793 | `			ph7_hashmap_node *pNode2;` |
|        - |  4794 | `			ph7_value *pElem;` |
|        - |  4795 | `			sxu32 i;` |
|        - |  4796 | `			/* Overwrite TOS with first element */` |
|       13 |  4797 | `			pNode2 = pMap->pFirst;` |
|       13 |  4798 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4799 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4800 | `			if( pElem ){` |
|       13 |  4801 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4802 | `			}` |
|       13 |  4803 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4804 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4805 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4806 | `			pNode2 = pNode2->pPrev;` |
|        - |  4807 | `			/* Push remaining elements */` |
|       33 |  4808 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4809 | `				pTos++;` |
|       21 |  4810 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4811 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4812 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4813 | `				if( pElem ){` |
|       21 |  4814 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4815 | `				}` |
|       21 |  4816 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4817 | `			}` |
|       13 |  4818 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4819 | `		}` |
|        7 |  4820 | `	}` |
|        - |  4821 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4822 | `	break;` |
|        - |  4823 |  |
|        - |  4824 | `/* OP_LXOR: * * *` |
|        - |  4825 | ` *` |
|        - |  4826 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4827 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4828 | ` * stack.` |
|        - |  4829 | ` * According to the PHP language reference manual:` |
|        - |  4830 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4831 | ` *  TRUE,but not both.` |
|        - |  4832 | ` */` |
|        5 |  4833 | `case PH7_OP_LXOR:{` |
|       11 |  4834 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4835 | `	sxi32 v = 0;` |
|        - |  4836 | `#ifdef UNTRUST` |
|        - |  4837 | `	if( pNos < pStack ){` |
|        - |  4838 | `		goto Abort;` |
|        - |  4839 | `	}` |
|        - |  4840 | `#endif` |
|        - |  4841 | `	/* Force a boolean cast */` |
|       11 |  4842 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4843 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4844 | `	}` |
|       11 |  4845 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4846 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4847 | `	}` |
|       11 |  4848 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4849 | `		v = 1;` |
|        3 |  4850 | `	}` |
|       11 |  4851 | `	VmPopOperand(&pTos,1);` |
|       11 |  4852 | `	pTos->x.iVal = v;` |
|       11 |  4853 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4854 | `	break;` |
|        - |  4855 | `				 }` |
|        - |  4856 | `/* OP_EQ P1 P2 P3` |
|        - |  4857 | ` *` |
|        - |  4858 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4859 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4860 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4861 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4862 | ` */` |
|        - |  4863 | `/* OP_NEQ P1 P2 P3` |
|        - |  4864 | ` *` |
|        - |  4865 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4866 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4867 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4868 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4869 | ` */` |
|     4085 |  4870 | `case PH7_OP_EQ:` |
|        - |  4871 | `case PH7_OP_NEQ: {` |
|     8172 |  4872 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4873 | `	/* Perform the comparison and act accordingly */` |
|        - |  4874 | `#ifdef UNTRUST` |
|        - |  4875 | `	if( pNos < pStack ){` |
|        - |  4876 | `		goto Abort;` |
|        - |  4877 | `	}` |
|        - |  4878 | `#endif` |
|     8172 |  4879 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8172 |  4880 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4881 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8163 |  4882 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8128 |  4883 | `		rc = rc == 0;` |
|     4065 |  4884 | `	}else{` |
|       28 |  4885 | `		rc = rc != 0;` |
|        - |  4886 | `	}` |
|     8172 |  4887 | `	VmPopOperand(&pTos,1);` |
|     8172 |  4888 | `	if( !pInstr->iP2 ){` |
|        - |  4889 | `		/* Push comparison result without taking the jump */` |
|     8172 |  4890 | `		PH7_MemObjRelease(pTos);` |
|     8172 |  4891 | `		pTos->x.iVal = rc;` |
|        - |  4892 | `		/* Invalidate any prior representation */` |
|     8172 |  4893 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4087 |  4894 | `	}else{` |
|      ! 0 |  4895 | `		if( rc ){` |
|        - |  4896 | `			/* Jump to the desired location */` |
|      ! 0 |  4897 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4898 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4899 | `		}` |
|        - |  4900 | `	}` |
|     8172 |  4901 | `	break;` |
|        - |  4902 | `				 }` |
|        - |  4903 | `/* OP_TEQ P1 P2 *` |
|        - |  4904 | ` *` |
|        - |  4905 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4906 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4907 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4908 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4909 | ` */` |
|   138216 |  4910 | `case PH7_OP_TEQ: {` |
|   276434 |  4911 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4912 | `	/* Perform the comparison and act accordingly */` |
|        - |  4913 | `#ifdef UNTRUST` |
|        - |  4914 | `	if( pNos < pStack ){` |
|        - |  4915 | `		goto Abort;` |
|        - |  4916 | `	}` |
|        - |  4917 | `#endif` |
|   276434 |  4918 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   276434 |  4919 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4920 | `		rc = 0;` |
|        2 |  4921 | `	}else{` |
|   276432 |  4922 | `		rc = rc == 0;` |
|        - |  4923 | `	}` |
|   276434 |  4924 | `	VmPopOperand(&pTos,1);` |
|   276434 |  4925 | `	if( !pInstr->iP2 ){` |
|        - |  4926 | `		/* Push comparison result without taking the jump */` |
|   276434 |  4927 | `		PH7_MemObjRelease(pTos);` |
|   276434 |  4928 | `		pTos->x.iVal = rc;` |
|        - |  4929 | `		/* Invalidate any prior representation */` |
|   276434 |  4930 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   138218 |  4931 | `	}else{` |
|      ! 0 |  4932 | `		if( rc ){` |
|        - |  4933 | `			/* Jump to the desired location */` |
|      ! 0 |  4934 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4935 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4936 | `		}` |
|        - |  4937 | `	}` |
|   276434 |  4938 | `	break;` |
|        - |  4939 | `				 }` |
|        - |  4940 | `/* OP_TNE P1 P2 *` |
|        - |  4941 | ` *` |
|        - |  4942 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4943 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4944 | ` * instruction.` |
|        - |  4945 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4946 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4947 | ` *` |
|        - |  4948 | ` */` |
|   107757 |  4949 | `case PH7_OP_TNE: {` |
|   215516 |  4950 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4951 | `	/* Perform the comparison and act accordingly */` |
|        - |  4952 | `#ifdef UNTRUST` |
|        - |  4953 | `	if( pNos < pStack ){` |
|        - |  4954 | `		goto Abort;` |
|        - |  4955 | `	}` |
|        - |  4956 | `#endif` |
|   215516 |  4957 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   215516 |  4958 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4959 | `		rc = 1;` |
|        2 |  4960 | `	}else{` |
|   215514 |  4961 | `		rc = rc != 0;` |
|        - |  4962 | `	}` |
|   215516 |  4963 | `	VmPopOperand(&pTos,1);` |
|   215516 |  4964 | `	if( !pInstr->iP2 ){` |
|        - |  4965 | `		/* Push comparison result without taking the jump */` |
|   215516 |  4966 | `		PH7_MemObjRelease(pTos);` |
|   215516 |  4967 | `		pTos->x.iVal = rc;` |
|        - |  4968 | `		/* Invalidate any prior representation */` |
|   215516 |  4969 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   107759 |  4970 | `	}else{` |
|      ! 0 |  4971 | `		if( rc ){` |
|        - |  4972 | `			/* Jump to the desired location */` |
|      ! 0 |  4973 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4974 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4975 | `		}` |
|        - |  4976 | `	}` |
|   215516 |  4977 | `	break;` |
|        - |  4978 | `				 }` |
|        - |  4979 | `/* OP_LT P1 P2 P3` |
|        - |  4980 | ` *` |
|        - |  4981 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4982 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4983 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4984 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4985 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4986 | ` *` |
|        - |  4987 | ` */` |
|        - |  4988 | `/* OP_LE P1 P2 P3` |
|        - |  4989 | ` *` |
|        - |  4990 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4991 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4992 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4993 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4994 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4995 | ` *` |
|        - |  4996 | ` */` |
|   104409 |  4997 | `case PH7_OP_LT:` |
|        - |  4998 | `case PH7_OP_LE: {` |
|   208864 |  4999 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5000 | `	/* Perform the comparison and act accordingly */` |
|        - |  5001 | `#ifdef UNTRUST` |
|        - |  5002 | `	if( pNos < pStack ){` |
|        - |  5003 | `		goto Abort;` |
|        - |  5004 | `	}` |
|        - |  5005 | `#endif` |
|   208864 |  5006 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   208864 |  5007 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5008 | `		rc = 0;` |
|   208860 |  5009 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      434 |  5010 | `		rc = rc < 1;` |
|      218 |  5011 | `	}else{` |
|   208424 |  5012 | `		rc = rc < 0;` |
|        - |  5013 | `	}` |
|   208864 |  5014 | `	VmPopOperand(&pTos,1);` |
|   208864 |  5015 | `	if( !pInstr->iP2 ){` |
|        - |  5016 | `		/* Push comparison result without taking the jump */` |
|   208864 |  5017 | `		PH7_MemObjRelease(pTos);` |
|   208864 |  5018 | `		pTos->x.iVal = rc;` |
|        - |  5019 | `		/* Invalidate any prior representation */` |
|   208864 |  5020 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   104455 |  5021 | `	}else{` |
|      ! 0 |  5022 | `		if( rc ){` |
|        - |  5023 | `			/* Jump to the desired location */` |
|      ! 0 |  5024 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5025 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5026 | `		}` |
|        - |  5027 | `	}` |
|   208864 |  5028 | `	break;` |
|        - |  5029 | `				}` |
|        - |  5030 | `/* OP_GT P1 P2 P3` |
|        - |  5031 | ` *` |
|        - |  5032 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5033 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5034 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5035 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5036 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5037 | ` *` |
|        - |  5038 | ` */` |
|        - |  5039 | `/* OP_GE P1 P2 P3` |
|        - |  5040 | ` *` |
|        - |  5041 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5042 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5043 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5044 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5045 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5046 | ` *` |
|        - |  5047 | ` */` |
|    50078 |  5048 | `case PH7_OP_GT:` |
|        - |  5049 | `case PH7_OP_GE: {` |
|   100158 |  5050 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5051 | `	/* Perform the comparison and act accordingly */` |
|        - |  5052 | `#ifdef UNTRUST` |
|        - |  5053 | `	if( pNos < pStack ){` |
|        - |  5054 | `		goto Abort;` |
|        - |  5055 | `	}` |
|        - |  5056 | `#endif` |
|   100158 |  5057 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   100158 |  5058 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5059 | `		rc = 0;` |
|   100154 |  5060 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    99996 |  5061 | `		rc = rc >= 0;` |
|    49999 |  5062 | `	}else{` |
|      156 |  5063 | `		rc = rc > 0;` |
|        - |  5064 | `	}` |
|   100158 |  5065 | `	VmPopOperand(&pTos,1);` |
|   100158 |  5066 | `	if( !pInstr->iP2 ){` |
|        - |  5067 | `		/* Push comparison result without taking the jump */` |
|   100158 |  5068 | `		PH7_MemObjRelease(pTos);` |
|   100158 |  5069 | `		pTos->x.iVal = rc;` |
|        - |  5070 | `		/* Invalidate any prior representation */` |
|   100158 |  5071 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    50080 |  5072 | `	}else{` |
|      ! 0 |  5073 | `		if( rc ){` |
|        - |  5074 | `			/* Jump to the desired location */` |
|      ! 0 |  5075 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5076 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5077 | `		}` |
|        - |  5078 | `	}` |
|   100158 |  5079 | `	break;` |
|        - |  5080 | `				}` |
|        - |  5081 | `/* OP_SPACESHIP * * *` |
|        - |  5082 | ` *` |
|        - |  5083 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5084 | ` *   -1 if left < right` |
|        - |  5085 | ` *    0 if left == right` |
|        - |  5086 | ` *    1 if left > right` |
|        - |  5087 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5088 | ` */` |
|       25 |  5089 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5090 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5091 | `#ifdef UNTRUST` |
|        - |  5092 | `	if( pNos < pStack ){` |
|        - |  5093 | `		goto Abort;` |
|        - |  5094 | `	}` |
|        - |  5095 | `#endif` |
|       51 |  5096 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5097 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5098 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5099 | `		rc = 1;` |
|        4 |  5100 | `	}else{` |
|        - |  5101 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5102 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5103 | `	}` |
|       51 |  5104 | `	VmPopOperand(&pTos,1);` |
|       51 |  5105 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5106 | `	pTos->x.iVal = rc;` |
|       51 |  5107 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5108 | `	break;` |
|        - |  5109 | `				}` |
|        - |  5110 | `/* OP_SEQ P1 P2 *` |
|        - |  5111 | ` * Strict string comparison.` |
|        - |  5112 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5113 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5114 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5115 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5116 | ` * use PH7_OP_EQ.` |
|        - |  5117 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5118 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5119 | ` */` |
|        - |  5120 | `/* OP_SNE P1 P2 *` |
|        - |  5121 | ` * Strict string comparison.` |
|        - |  5122 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5123 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5124 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5125 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5126 | ` * use PH7_OP_EQ.` |
|        - |  5127 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5128 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5129 | ` */` |
|       18 |  5130 | `case PH7_OP_SEQ:` |
|        - |  5131 | `case PH7_OP_SNE: {` |
|       38 |  5132 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5133 | `	SyString s1,s2;` |
|        - |  5134 | `	/* Perform the comparison and act accordingly */` |
|        - |  5135 | `#ifdef UNTRUST` |
|        - |  5136 | `	if( pNos < pStack ){` |
|        - |  5137 | `		goto Abort;` |
|        - |  5138 | `	}` |
|        - |  5139 | `#endif` |
|        - |  5140 | `	/* Force a string cast */` |
|       38 |  5141 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5142 | `		PH7_MemObjToString(pTos);` |
|        2 |  5143 | `	}` |
|       38 |  5144 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5145 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5146 | `	}` |
|       38 |  5147 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5148 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5149 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5150 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5151 | `		rc = rc != 0;` |
|      ! 0 |  5152 | `	}else{` |
|       38 |  5153 | `		rc = rc == 0;` |
|        - |  5154 | `	}` |
|       38 |  5155 | `	VmPopOperand(&pTos,1);` |
|       38 |  5156 | `	if( !pInstr->iP2 ){` |
|        - |  5157 | `		/* Push comparison result without taking the jump */` |
|       38 |  5158 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5159 | `		pTos->x.iVal = rc;` |
|        - |  5160 | `		/* Invalidate any prior representation */` |
|       38 |  5161 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5162 | `	}else{` |
|      ! 0 |  5163 | `		if( rc ){` |
|        - |  5164 | `			/* Jump to the desired location */` |
|      ! 0 |  5165 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5166 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5167 | `		}` |
|        - |  5168 | `	}` |
|       38 |  5169 | `	break;` |
|        - |  5170 | `				 }` |
|        - |  5171 | `/*` |
|        - |  5172 | ` * OP_LOAD_REF * * *` |
|        - |  5173 | ` * Push the index of a referenced object on the stack.` |
|        - |  5174 | ` */` |
|       57 |  5175 | `case PH7_OP_LOAD_REF: {` |
|        - |  5176 | `	sxu32 nIdx;` |
|        - |  5177 | `#ifdef UNTRUST` |
|        - |  5178 | `	if( pTos < pStack ){` |
|        - |  5179 | `		goto Abort;` |
|        - |  5180 | `	}` |
|        - |  5181 | `#endif` |
|        - |  5182 | `	/* Extract memory object index */` |
|      115 |  5183 | `	nIdx = pTos->nIdx;` |
|      115 |  5184 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5185 | `		/* Nullify the object */` |
|       95 |  5186 | `		PH7_MemObjRelease(pTos);` |
|        - |  5187 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5188 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5189 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5190 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5191 | `	}` |
|      115 |  5192 | `	break;` |
|        - |  5193 | `					  }` |
|        - |  5194 | `/*` |
|        - |  5195 | ` * OP_STORE_REF * * P3` |
|        - |  5196 | ` * Perform an assignment operation by reference.` |
|        - |  5197 | ` */` |
|       15 |  5198 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5199 | `	 SyString sName = { 0 , 0 };` |
|        - |  5200 | `	 VmFrame *pFrameLocal;` |
|        - |  5201 | `	SyHashEntry *pEntry;` |
|        - |  5202 | `	sxu32 nIdx;` |
|        - |  5203 | `#ifdef UNTRUST` |
|        - |  5204 | `	if( pTos < pStack ){` |
|        - |  5205 | `		goto Abort;` |
|        - |  5206 | `	}` |
|        - |  5207 | `#endif` |
|       32 |  5208 | `	if( pInstr->p3 == 0 ){` |
|        - |  5209 | `		char *zName;` |
|        - |  5210 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5211 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5212 | `			/* Force a string cast */` |
|      ! 0 |  5213 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5214 | `		}` |
|      ! 0 |  5215 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5216 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5217 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5218 | `			if( zName ){` |
|      ! 0 |  5219 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5220 | `			}` |
|      ! 0 |  5221 | `		}` |
|      ! 0 |  5222 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5223 | `		pTos--;` |
|      ! 0 |  5224 | `	}else{` |
|       32 |  5225 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5226 | `	}` |
|       32 |  5227 | `	nIdx = pTos->nIdx;` |
|       32 |  5228 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5229 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5230 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5231 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5232 | `		}else{` |
|        - |  5233 | `			ph7_value *pObj;` |
|        - |  5234 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5235 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5236 | `			if( pObj == 0 ){` |
|      ! 0 |  5237 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5238 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5239 | `				goto Abort;` |
|        - |  5240 | `			}` |
|        - |  5241 | `			/* Perform the store operation */` |
|      ! 0 |  5242 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5243 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5244 | `		}` |
|       32 |  5245 | `	}else if( sName.nByte > 0){` |
|       32 |  5246 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5247 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5248 | `		}else{` |
|       32 |  5249 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5250 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5251 | `			/* Query the local frame */` |
|       32 |  5252 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5253 | `			if( pEntry ){` |
|      ! 0 |  5254 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5255 | `			}else{` |
|       32 |  5256 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5257 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5258 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5259 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5260 | `				}` |
|       32 |  5261 | `				if( rc == SXRET_OK ){` |
|       32 |  5262 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5263 | `				}` |
|        - |  5264 | `			}` |
|        - |  5265 | `		}` |
|       15 |  5266 | `	}` |
|       32 |  5267 | `	break;` |
|        - |  5268 | `				 }` |
|        - |  5269 | `/*` |
|        - |  5270 | ` * OP_UPLINK P1 * *` |
|        - |  5271 | ` * Link a variable to the top active VM frame.` |
|        - |  5272 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5273 | ` */` |
|       25 |  5274 | `case PH7_OP_UPLINK: {` |
|       52 |  5275 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5276 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5277 | `		SyString sName;` |
|        - |  5278 | `		/* Perform the link */` |
|      104 |  5279 | `		while( pLink <= pTos ){` |
|       54 |  5280 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5281 | `				/* Force a string cast */` |
|      ! 0 |  5282 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5283 | `			}` |
|       54 |  5284 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5285 | `			if( sName.nByte > 0 ){` |
|       54 |  5286 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5287 | `			}` |
|       54 |  5288 | `			pLink++;` |
|        2 |  5289 | `		}` |
|       25 |  5290 | `	}` |
|       52 |  5291 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5292 | `	break;` |
|        - |  5293 | `					}` |
|        - |  5294 | `/*` |
|        - |  5295 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5296 | ` * Push an exception in the corresponding container so that` |
|        - |  5297 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5298 | ` */` |
|       49 |  5299 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      100 |  5300 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5301 | `	VmFrame *pFrameLocal;` |
|        - |  5302 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      100 |  5303 | `	pException->iFinallyDone = 0;` |
|      100 |  5304 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5305 | `	/* Create the exception frame */` |
|      100 |  5306 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      100 |  5307 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5308 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5309 | `		goto Abort;` |
|        - |  5310 | `	}` |
|        - |  5311 | `	/* Mark the special frame */` |
|      100 |  5312 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      100 |  5313 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5314 | `	/* Point to the frame that trigger the exception */` |
|      100 |  5315 | `	pFrameLocal = pFrameLocal->pParent;` |
|      100 |  5316 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      100 |  5317 | `	pException->pFrame = pFrameLocal;` |
|      100 |  5318 | `	break;` |
|        - |  5319 | `							}` |
|        - |  5320 | `/*` |
|        - |  5321 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5322 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5323 | ` */` |
|       48 |  5324 | `case PH7_OP_POP_EXCEPTION: {` |
|       98 |  5325 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       98 |  5326 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5327 | `		ph7_exception **apException;` |
|        - |  5328 | `		/* Pop the loaded exception */` |
|       28 |  5329 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5330 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5331 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5332 | `		}` |
|       13 |  5333 | `	}` |
|       98 |  5334 | `	pException->pFrame = 0;` |
|        - |  5335 | `	/* Leave the exception frame */` |
|       98 |  5336 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5337 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       98 |  5338 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5339 | `		sxi32 rcFinally;` |
|       20 |  5340 | `		pException->iFinallyDone = 1;` |
|       20 |  5341 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5342 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5343 | `			goto Abort;` |
|        - |  5344 | `		}` |
|        9 |  5345 | `	}` |
|       98 |  5346 | `	break;` |
|        - |  5347 | `							}` |
|        - |  5348 |  |
|        - |  5349 | `/*` |
|        - |  5350 | ` * OP_THROW * P2 *` |
|        - |  5351 | ` * Throw an user exception.` |
|        - |  5352 | ` */` |
|       30 |  5353 | `case PH7_OP_THROW: {` |
|       62 |  5354 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  5355 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5356 | `#ifdef UNTRUST` |
|        - |  5357 | `	if( pTos < pStack ){` |
|        - |  5358 | `		goto Abort;` |
|        - |  5359 | `	}` |
|        - |  5360 | `#endif` |
|       62 |  5361 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5362 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  5363 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  5364 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  5365 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5366 | `		ph7_class *pException;` |
|        - |  5367 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5368 | `		 */` |
|       62 |  5369 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  5370 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5371 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5372 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5373 | `			if( rc == SXERR_ABORT ){` |
|        - |  5374 | `				/* Abort processing immediately */` |
|      ! 0 |  5375 | `				goto Abort;` |
|        - |  5376 | `			}` |
|      ! 0 |  5377 | `		}else{` |
|        - |  5378 | `			/* Throw the exception */` |
|       62 |  5379 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  5380 | `			if( rc == SXERR_ABORT ){` |
|        - |  5381 | `				/* Abort processing immediately */` |
|        9 |  5382 | `				goto Abort;` |
|        - |  5383 | `			}` |
|        - |  5384 | `		}` |
|       28 |  5385 | `	}else{` |
|        - |  5386 | `		/* Expecting a class instance */` |
|      ! 0 |  5387 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5388 | `		if( rc == SXERR_ABORT ){` |
|        - |  5389 | `			/* Abort processing immediately */` |
|      ! 0 |  5390 | `			goto Abort;` |
|        - |  5391 | `		}` |
|        - |  5392 | `	}` |
|        - |  5393 | `	/* Pop the top entry */` |
|       54 |  5394 | `	VmPopOperand(&pTos,1);` |
|        - |  5395 | `	/* Perform an unconditional jump */` |
|       54 |  5396 | `	pc = nJump - 1;` |
|       54 |  5397 | `	break;` |
|        - |  5398 | `				   }` |
|        - |  5399 | `/*` |
|        - |  5400 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5401 | ` * Prepare a foreach step.` |
|        - |  5402 | ` */` |
|     5240 |  5403 | `case PH7_OP_FOREACH_INIT: {` |
|    10482 |  5404 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5405 | `	void *pName;` |
|        - |  5406 | `#ifdef UNTRUST` |
|        - |  5407 | `	if( pTos < pStack ){` |
|        - |  5408 | `		goto Abort;` |
|        - |  5409 | `	}` |
|        - |  5410 | `#endif` |
|    10482 |  5411 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5412 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5413 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5414 | `			/* Force a string cast */` |
|      ! 0 |  5415 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5416 | `		}` |
|        - |  5417 | `		/* Duplicate name */` |
|      ! 0 |  5418 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5419 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5420 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5421 | `		}` |
|      ! 0 |  5422 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5423 | `	}` |
|    10482 |  5424 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5425 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5426 | `			/* Force a string cast */` |
|      ! 0 |  5427 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5428 | `		}` |
|        - |  5429 | `		/* Duplicate name */` |
|      ! 0 |  5430 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5431 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5432 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5433 | `		}` |
|      ! 0 |  5434 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5435 | `	}` |
|        - |  5436 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10482 |  5437 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5438 | `		/* Jump out of the loop */` |
|      ! 0 |  5439 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5440 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5441 | `		}` |
|      ! 0 |  5442 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5443 | `	}else{` |
|        - |  5444 | `		ph7_foreach_step *pStep;` |
|    10482 |  5445 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10482 |  5446 | `		if( pStep == 0 ){` |
|      ! 0 |  5447 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5448 | `			/* Jump out of the loop */` |
|      ! 0 |  5449 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5450 | `		}else{` |
|        - |  5451 | `			/* Zero the structure */` |
|    10482 |  5452 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5453 | `			/* Prepare the step */` |
|    10482 |  5454 | `			pStep->iFlags = pInfo->iFlags;` |
|    10482 |  5455 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5456 | `				ph7_hashmap *pMap;` |
|        - |  5457 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5458 | `				 * source array so mutations don't affect other sharers. */` |
|    10454 |  5459 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5460 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5461 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5462 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5463 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5464 | `						 * variable still points at the same hashmap as` |
|        - |  5465 | `						 * the stack value. */` |
|        9 |  5466 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5467 | `							pCur->iRef--;` |
|        9 |  5468 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5469 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5470 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5471 | `						}` |
|        4 |  5472 | `					}` |
|        4 |  5473 | `				}` |
|    10454 |  5474 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5475 | `				/* Reset the internal loop cursor */` |
|    10454 |  5476 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5477 | `				/* Mark the step */` |
|    10454 |  5478 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10454 |  5479 | `				pStep->xIter.pMap = pMap;` |
|    10454 |  5480 | `				pMap->iRef++;` |
|     5228 |  5481 | `			}else{` |
|       30 |  5482 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5483 | `				ph7_class *pIteratorClass;` |
|        - |  5484 | `				/* Check if the object implements Iterator */` |
|       30 |  5485 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5486 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5487 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5488 | `					ph7_class_method *pRewind;` |
|       20 |  5489 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5490 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5491 | `					pThis->iRef++;` |
|       20 |  5492 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5493 | `					if( pRewind ){` |
|       20 |  5494 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5495 | `					}` |
|       11 |  5496 | `				}else{` |
|        - |  5497 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5498 | `					ph7_class *pIterAggClass;` |
|       12 |  5499 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5500 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5501 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5502 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5503 | `						ph7_class_method *pGetIter;` |
|        3 |  5504 | `						int iterAggOk = 0;` |
|        3 |  5505 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5506 | `						if( pGetIter ){` |
|        - |  5507 | `							ph7_value sResult;` |
|        3 |  5508 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5509 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5510 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5511 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5512 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5513 | `									ph7_class_method *pRewind;` |
|        3 |  5514 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5515 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5516 | `									pIterObj->iRef++;` |
|        - |  5517 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5518 | `									pStep->pOwner = pThis;` |
|        3 |  5519 | `									pThis->iRef++;` |
|        3 |  5520 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5521 | `									if( pRewind ){` |
|        3 |  5522 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5523 | `									}` |
|        3 |  5524 | `									iterAggOk = 1;` |
|        1 |  5525 | `								}` |
|        1 |  5526 | `							}` |
|        3 |  5527 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5528 | `						}` |
|        3 |  5529 | `						if( !iterAggOk ){` |
|        - |  5530 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5531 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5532 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5533 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5534 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5535 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5536 | `						}` |
|        2 |  5537 | `					}else{` |
|        - |  5538 | `						/* Plain object iteration via hAttr */` |
|        9 |  5539 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5540 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5541 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5542 | `						pThis->iRef++;` |
|        - |  5543 | `					}` |
|        - |  5544 | `				}` |
|        - |  5545 | `			}` |
|        - |  5546 | `		}` |
|    10482 |  5547 | `		if( pStep ){` |
|    10482 |  5548 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5549 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5550 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5551 | `				/* Jump out of the loop */` |
|      ! 0 |  5552 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5553 | `			}` |
|     5240 |  5554 | `		}` |
|        - |  5555 | `	}` |
|    10482 |  5556 | `	VmPopOperand(&pTos,1);` |
|    10482 |  5557 | `	break;` |
|        - |  5558 | `						  }` |
|        - |  5559 | `/*` |
|        - |  5560 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5561 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5562 | ` */` |
|    85133 |  5563 | `case PH7_OP_FOREACH_STEP: {` |
|   170268 |  5564 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5565 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5566 | `	ph7_value *pValue;` |
|        - |  5567 | `	VmFrame *pFrameLocal;` |
|        - |  5568 | `	/* Peek the last step */` |
|   170268 |  5569 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   170268 |  5570 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   170268 |  5571 | `	pFrameLocal = pVm->pFrame;` |
|   170268 |  5572 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   170268 |  5573 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   170156 |  5574 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5575 | `		ph7_hashmap_node *pNode;` |
|        - |  5576 | `		/* Extract the current node value */` |
|   170156 |  5577 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   170156 |  5578 | `		if( pNode == 0 ){` |
|        - |  5579 | `			/* No more entry to process */` |
|    10452 |  5580 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10452 |  5581 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5582 | `				/* Break the reference with the last element */` |
|        7 |  5583 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5584 | `			}` |
|        - |  5585 | `			/* Automatically reset the loop cursor */` |
|    10452 |  5586 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5587 | `			/* Cleanup the mess left behind */` |
|    10452 |  5588 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10452 |  5589 | `			SySetPop(&pInfo->aStep);` |
|    10452 |  5590 | `			PH7_HashmapUnref(pMap);` |
|     5227 |  5591 | `		}else{` |
|   159706 |  5592 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5593 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5594 | `				if( pKey ){` |
|      416 |  5595 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5596 | `				}` |
|      207 |  5597 | `			}` |
|   159706 |  5598 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5599 | `				SyHashEntry *pEntry;` |
|        - |  5600 | `				/* Pass by reference */` |
|       23 |  5601 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5602 | `				if( pEntry ){` |
|       23 |  5603 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5604 | `				}else{` |
|      ! 0 |  5605 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5606 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5607 | `				}` |
|       12 |  5608 | `			}else{` |
|        - |  5609 | `				/* Make a copy of the entry value */` |
|   159684 |  5610 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   159684 |  5611 | `				if( pValue ){` |
|   159684 |  5612 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    79841 |  5613 | `				}` |
|        - |  5614 | `			}` |
|        2 |  5615 | `		}` |
|    85191 |  5616 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5617 | `		/* Iterator-based iteration.` |
|        - |  5618 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5619 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5620 | `		 */` |
|       90 |  5621 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5622 | `		ph7_class_method *pMethod;` |
|        - |  5623 | `		ph7_value sResult;` |
|       90 |  5624 | `		int isValid = 0;` |
|        - |  5625 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5626 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5627 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5628 | `		}else{` |
|       70 |  5629 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5630 | `			if( pMethod ){` |
|       70 |  5631 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5632 | `			}` |
|        - |  5633 | `		}` |
|        - |  5634 | `		/* Call valid() */` |
|       90 |  5635 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5636 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5637 | `		if( pMethod ){` |
|       90 |  5638 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5639 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5640 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5641 | `		}` |
|       90 |  5642 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5643 | `		if( !isValid ){` |
|        - |  5644 | `			/* Iterator exhausted */` |
|       20 |  5645 | `			pc = pInstr->iP2 - 1;` |
|        - |  5646 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5647 | `			if( pStep->pOwner ){` |
|        3 |  5648 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5649 | `			}` |
|       20 |  5650 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5651 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5652 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5653 | `		}else{` |
|        - |  5654 | `			/* Call current() to get value */` |
|       72 |  5655 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5656 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5657 | `			if( pMethod ){` |
|       72 |  5658 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5659 | `			}` |
|       72 |  5660 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5661 | `			if( pValue ){` |
|       72 |  5662 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5663 | `			}` |
|       72 |  5664 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5665 | `			/* Call key() if needed */` |
|       72 |  5666 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5667 | `				ph7_value sKey;` |
|       35 |  5668 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5669 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5670 | `				if( pMethod ){` |
|       35 |  5671 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5672 | `				}` |
|       35 |  5673 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5674 | `				if( pValue ){` |
|       35 |  5675 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5676 | `				}` |
|       35 |  5677 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5678 | `			}` |
|        - |  5679 | `		}` |
|       46 |  5680 | `	}else{` |
|       25 |  5681 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5682 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5683 | `		SyHashEntry *pEntry;` |
|        - |  5684 | `		/* Point to the next attribute */` |
|       29 |  5685 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5686 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5687 | `			/* Check access permission */` |
|       31 |  5688 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5689 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5690 | `					break; /* Access is granted */` |
|        - |  5691 | `			}` |
|        1 |  5692 | `		}` |
|       25 |  5693 | `		if( pEntry == 0 ){` |
|        - |  5694 | `			/* Clean up the mess left behind */` |
|        9 |  5695 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5696 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5697 | `				/* Break the reference with the last element */` |
|        3 |  5698 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5699 | `			}` |
|        9 |  5700 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5701 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5702 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5703 | `		}else{` |
|       17 |  5704 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5705 | `			ph7_value *pAttrValue;` |
|       17 |  5706 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5707 | `				/* Fill with the current attribute name */` |
|       17 |  5708 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5709 | `				if( pKey ){` |
|       17 |  5710 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5711 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5712 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5713 | `				}` |
|        8 |  5714 | `			}` |
|        - |  5715 | `			/* Extract attribute value */` |
|       17 |  5716 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5717 | `			if( pAttrValue ){` |
|       17 |  5718 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5719 | `					/* Pass by reference */` |
|        3 |  5720 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5721 | `					if( pEntry ){` |
|        3 |  5722 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5723 | `					}else{` |
|      ! 0 |  5724 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5725 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5726 | `					}` |
|        2 |  5727 | `				}else{` |
|        - |  5728 | `					/* Make a copy of the attribute value */` |
|       15 |  5729 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5730 | `					if( pValue ){` |
|       15 |  5731 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5732 | `					}` |
|        - |  5733 | `				}` |
|        8 |  5734 | `			}` |
|        - |  5735 | `		}` |
|        - |  5736 | `	}` |
|   170268 |  5737 | `	break;` |
|        - |  5738 | `						  }` |
|        - |  5739 | `/*` |
|        - |  5740 | ` * OP_MEMBER P1 P2` |
|        - |  5741 | ` * Load class attribute/method on the stack.` |
|        - |  5742 | ` */` |
|     2386 |  5743 | `case PH7_OP_MEMBER: {` |
|        - |  5744 | `	ph7_class_instance *pThis;` |
|        - |  5745 | `	ph7_value *pNos;` |
|        - |  5746 | `	SyString sName;` |
|     4774 |  5747 | `	if( !pInstr->iP1 ){` |
|     4584 |  5748 | `		pNos = &pTos[-1];` |
|        - |  5749 | `#ifdef UNTRUST` |
|        - |  5750 | `		if( pNos < pStack ){` |
|        - |  5751 | `			goto Abort;` |
|        - |  5752 | `		}` |
|        - |  5753 | `#endif` |
|     4584 |  5754 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5755 | `			ph7_class *pClass;` |
|        - |  5756 | `			/* Class already instantiated */` |
|     4584 |  5757 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5758 | `			/* Point to the instantiated class */` |
|     4584 |  5759 | `			pClass = pThis->pClass;` |
|        - |  5760 | `			/* Extract attribute name first */` |
|     4584 |  5761 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4584 |  5762 | `			if( pInstr->iP2 ){` |
|        - |  5763 | `				/* Method call */` |
|      488 |  5764 | `				ph7_class_method *pMeth = 0;` |
|      488 |  5765 | `				if( sName.nByte > 0 ){` |
|        - |  5766 | `					/* Extract the target method */` |
|      488 |  5767 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      243 |  5768 | `				}` |
|      488 |  5769 | `				if( pMeth == 0 ){` |
|      ! 0 |  5770 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5771 | `						&pClass->sName,&sName` |
|        - |  5772 | `						);` |
|        - |  5773 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5774 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5775 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5776 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5777 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5778 | `				}else{` |
|        - |  5779 | `					/* Push method name on the stack */` |
|      488 |  5780 | `					PH7_MemObjRelease(pTos);` |
|      488 |  5781 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      488 |  5782 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5783 | `				}` |
|      488 |  5784 | `				pTos->nIdx = SXU32_HIGH;` |
|      245 |  5785 | `			}else{` |
|        - |  5786 | `				/* Attribute access */` |
|     4098 |  5787 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5788 | `				SyHashEntry *pEntry;` |
|        - |  5789 | `				/* Extract the target attribute */` |
|     4098 |  5790 | `				if( sName.nByte > 0 ){` |
|     4098 |  5791 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4098 |  5792 | `					if( pEntry ){` |
|        - |  5793 | `						/* Point to the attribute value */` |
|     4096 |  5794 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2047 |  5795 | `					}` |
|     2048 |  5796 | `				}` |
|     4098 |  5797 | `				if( pObjAttr == 0 ){` |
|        - |  5798 | `					/* No such attribute,load null */` |
|        4 |  5799 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5800 | `						&pClass->sName,&sName);` |
|        - |  5801 | `					/* Call the __get magic method if available */` |
|        3 |  5802 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5803 | `				}` |
|     4098 |  5804 | `				VmPopOperand(&pTos,1);` |
|        - |  5805 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5806 | `				 * This is due to the following case:` |
|        - |  5807 | `				 *     (new TestClass())->foo;` |
|        - |  5808 | `				 */` |
|     4098 |  5809 | `				pThis->iRef++;` |
|     4098 |  5810 | `				PH7_MemObjRelease(pTos);` |
|     4098 |  5811 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4098 |  5812 | `				if( pObjAttr ){` |
|     4096 |  5813 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5814 | `					/* Check attribute access */` |
|     4096 |  5815 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  5816 | `						/* Load attribute */` |
|     4096 |  5817 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4096 |  5818 | `						if( pValue ){` |
|     4096 |  5819 | `							if( pThis->iRef < 2 ){` |
|        - |  5820 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5821 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5822 | `								 */` |
|        3 |  5823 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5824 | `							}else{` |
|        - |  5825 | `								/* Simple load */` |
|     4094 |  5826 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5827 | `							}` |
|     4096 |  5828 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4094 |  5829 | `								if( pThis->iRef > 1 ){` |
|        - |  5830 | `									/* Load attribute index */` |
|     4092 |  5831 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2045 |  5832 | `								}` |
|     2046 |  5833 | `							}` |
|     2047 |  5834 | `						}` |
|     2049 |  5835 | `					}else{` |
|        - |  5836 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  5837 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  5838 | `						char zMsg[256];` |
|      ! 0 |  5839 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  5840 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  5841 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  5842 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  5843 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5844 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  5845 | `						goto Abort;` |
|        - |  5846 | `					}` |
|     2047 |  5847 | `				}` |
|        - |  5848 | `				/* Safely unreference the object */` |
|     4098 |  5849 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5850 | `			}` |
|     2293 |  5851 | `		}else{` |
|      ! 0 |  5852 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5853 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5854 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5855 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5856 | `		}` |
|     2293 |  5857 | `	}else{` |
|        - |  5858 | `		/* Static member access using class name */` |
|      192 |  5859 | `		pNos = pTos;` |
|      192 |  5860 | `		pThis = 0;` |
|      192 |  5861 | `		if( !pInstr->p3 ){` |
|      180 |  5862 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      180 |  5863 | `			pNos--;` |
|        - |  5864 | `#ifdef UNTRUST` |
|        - |  5865 | `			if( pNos < pStack ){` |
|        - |  5866 | `				goto Abort;` |
|        - |  5867 | `			}` |
|        - |  5868 | `#endif` |
|       91 |  5869 | `		}else{` |
|        - |  5870 | `			/* Attribute name already computed */` |
|       14 |  5871 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5872 | `		}` |
|      192 |  5873 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      192 |  5874 | `			ph7_class *pClass = 0;` |
|      192 |  5875 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5876 | `				/* Class already instantiated */` |
|        5 |  5877 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5878 | `				pClass = pThis->pClass;` |
|        5 |  5879 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5880 | `			}else{` |
|        - |  5881 | `				/* Try to extract the target class */` |
|      188 |  5882 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      188 |  5883 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      188 |  5884 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5885 | `					/* Handle self/static/parent keywords */` |
|      188 |  5886 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       56 |  5887 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       56 |  5888 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5889 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5890 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5891 | `						}` |
|      161 |  5892 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  5893 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      133 |  5894 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  5895 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  5896 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  5897 | `							pClass = pSelf->pBase;` |
|       12 |  5898 | `						}` |
|       14 |  5899 | `					}else{` |
|       84 |  5900 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5901 | `					}` |
|       93 |  5902 | `				}` |
|        - |  5903 | `			}` |
|      192 |  5904 | `			if( pClass == 0 ){` |
|        - |  5905 | `				/* Undefined class */` |
|      ! 0 |  5906 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5907 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5908 | `					);` |
|      ! 0 |  5909 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5910 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5911 | `				}` |
|      ! 0 |  5912 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5913 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5914 | `			}else{` |
|      192 |  5915 | `				if( pInstr->iP2 ){` |
|        - |  5916 | `					/* Method call */` |
|       76 |  5917 | `					ph7_class_method *pMeth = 0;` |
|       76 |  5918 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5919 | `						/* Extract the target method */` |
|       76 |  5920 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       37 |  5921 | `					}` |
|       76 |  5922 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5923 | `						if( pMeth ){` |
|      ! 0 |  5924 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5925 | `								&pClass->sName,&sName` |
|        - |  5926 | `								);` |
|      ! 0 |  5927 | `						}else{` |
|      ! 0 |  5928 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5929 | `								&pClass->sName,&sName` |
|        - |  5930 | `								);` |
|        - |  5931 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5932 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5933 | `						}` |
|        - |  5934 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5935 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5936 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5937 | `						}` |
|      ! 0 |  5938 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5939 | `					}else{` |
|        - |  5940 | `						/* Push method name on the stack */` |
|       76 |  5941 | `						PH7_MemObjRelease(pTos);` |
|       76 |  5942 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       76 |  5943 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5944 | `					}` |
|       76 |  5945 | `					pTos->nIdx = SXU32_HIGH;` |
|       39 |  5946 | `				}else{` |
|        - |  5947 | `					/* Attribute access */` |
|      118 |  5948 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5949 | `					/* Check for special ::class pseudo-constant */` |
|      153 |  5950 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5951 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5952 | `						/* ::class returns the fully qualified class name */` |
|        - |  5953 | `						/* Pop the attribute name from the stack */` |
|       60 |  5954 | `						if( !pInstr->p3 ){` |
|       60 |  5955 | `							VmPopOperand(&pTos,1);` |
|       29 |  5956 | `						}` |
|       60 |  5957 | `						PH7_MemObjRelease(pTos);` |
|        - |  5958 | `						/* Load the class name */` |
|       60 |  5959 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5960 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5961 | `					}else{` |
|        - |  5962 | `						/* Extract the target attribute */` |
|       60 |  5963 | `						if( sName.nByte > 0 ){` |
|       60 |  5964 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       29 |  5965 | `						}` |
|       60 |  5966 | `						if( pAttr == 0 ){` |
|        - |  5967 | `							/* No such attribute,load null */` |
|      ! 0 |  5968 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5969 | `								&pClass->sName,&sName);` |
|        - |  5970 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5971 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5972 | `						}` |
|        - |  5973 | `						/* Pop the attribute name from the stack */` |
|       60 |  5974 | `						if( !pInstr->p3 ){` |
|       48 |  5975 | `							VmPopOperand(&pTos,1);` |
|       23 |  5976 | `						}` |
|       60 |  5977 | `						PH7_MemObjRelease(pTos);` |
|       60 |  5978 | `						pTos->nIdx = SXU32_HIGH;` |
|       60 |  5979 | `						if( pAttr ){` |
|       60 |  5980 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5981 | `								/* Access to a non static attribute */` |
|      ! 0 |  5982 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5983 | `									&pClass->sName,&pAttr->sName` |
|        - |  5984 | `									);` |
|      ! 0 |  5985 | `							}else{` |
|        - |  5986 | `								ph7_value *pValue;` |
|        - |  5987 | `								/* Check if the access to the attribute is allowed */` |
|       60 |  5988 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  5989 | `									/* Load the desired attribute */` |
|       56 |  5990 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       56 |  5991 | `									if( pValue ){` |
|       56 |  5992 | `										PH7_MemObjLoad(pValue,pTos);` |
|       56 |  5993 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5994 | `											/* Load index number */` |
|       14 |  5995 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5996 | `										}` |
|       27 |  5997 | `									}` |
|       29 |  5998 | `								}else{` |
|        - |  5999 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6000 | `									char zMsg[256];` |
|        5 |  6001 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6002 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6003 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6004 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6005 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6006 | `									}else{` |
|      ! 0 |  6007 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6008 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6009 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6010 | `									}` |
|        5 |  6011 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6012 | `									goto Abort;` |
|        - |  6013 | `								}` |
|        - |  6014 | `							}` |
|       27 |  6015 | `						}` |
|        - |  6016 | `					}` |
|        - |  6017 | `				}` |
|      188 |  6018 | `				if( pThis ){` |
|        - |  6019 | `					/* Safely unreference the object */` |
|        5 |  6020 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6021 | `				}` |
|        - |  6022 | `			}` |
|       95 |  6023 | `		}else{` |
|        - |  6024 | `			/* Pop operands */` |
|      ! 0 |  6025 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6026 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6027 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6028 | `			}` |
|      ! 0 |  6029 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6030 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6031 | `		}` |
|        - |  6032 | `	}` |
|     4770 |  6033 | `	break;` |
|        - |  6034 | `					}` |
|        - |  6035 | `/*` |
|        - |  6036 | ` * OP_NEW P1 * * *` |
|        - |  6037 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6038 | ` */` |
|      358 |  6039 | `case PH7_OP_NEW: {` |
|      718 |  6040 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      718 |  6041 | `	ph7_class *pClass = 0;` |
|        - |  6042 | `	ph7_class_instance *pNew;` |
|      718 |  6043 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6044 | `		/* Try to extract the desired class */` |
|     1076 |  6045 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      716 |  6046 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      358 |  6047 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6048 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6049 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6050 | `	}` |
|      718 |  6051 | `	if( pClass == 0 ){` |
|        - |  6052 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6053 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6054 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6055 | `			);` |
|        - |  6056 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6057 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6058 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6059 | `			/* Pop given arguments */` |
|      ! 0 |  6060 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6061 | `		}` |
|      ! 0 |  6062 | `		goto Abort;` |
|      ! 0 |  6063 | `	}else{` |
|        - |  6064 | `		ph7_class_method *pCons;` |
|        - |  6065 | `		/* Create a new class instance */` |
|      718 |  6066 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      718 |  6067 | `		if( pNew == 0 ){` |
|      ! 0 |  6068 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6069 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6070 | `				&pClass->sName` |
|        - |  6071 | `			);` |
|      ! 0 |  6072 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6073 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6074 | `				/* Pop given arguments */` |
|      ! 0 |  6075 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6076 | `			}` |
|      ! 0 |  6077 | `			break;` |
|        - |  6078 | `		}` |
|        - |  6079 | `		/* Check if a constructor is available */` |
|      718 |  6080 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      718 |  6081 | `		if( pCons == 0 ){` |
|      580 |  6082 | `			SyString *pName = &pClass->sName;` |
|        - |  6083 | `			/* Check for a constructor with the same base class name */` |
|      580 |  6084 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      289 |  6085 | `		}` |
|      718 |  6086 | `		if( pCons ){` |
|        - |  6087 | `			/* Call the class constructor */` |
|      140 |  6088 | `			SySetReset(&aArg);` |
|      270 |  6089 | `			while( pArg < pTos ){` |
|      132 |  6090 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      132 |  6091 | `				pArg++;` |
|        2 |  6092 | `			}` |
|      140 |  6093 | `			if( pVm->bErrReport ){` |
|        - |  6094 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6095 | `				sxu32 n;` |
|       57 |  6096 | `				n = SySetUsed(&aArg);` |
|        - |  6097 | `				/* Emit a notice for missing arguments */` |
|      101 |  6098 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6099 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6100 | `					if( pFuncArg ){` |
|       45 |  6101 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6102 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6103 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6104 | `						}` |
|       22 |  6105 | `					}` |
|       45 |  6106 | `					n++;` |
|        1 |  6107 | `				}` |
|       28 |  6108 | `			}` |
|      140 |  6109 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6110 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      140 |  6111 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6112 | `				pNew->iRef = 1;` |
|      ! 0 |  6113 | `			}` |
|       69 |  6114 | `		}` |
|      718 |  6115 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6116 | `			/* Pop given arguments */` |
|      122 |  6117 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       60 |  6118 | `		}` |
|      718 |  6119 | `		PH7_MemObjRelease(pTos);` |
|      718 |  6120 | `		pTos->x.pOther = pNew;` |
|      718 |  6121 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6122 | `	}` |
|      718 |  6123 | `	break;` |
|        - |  6124 | `				 }` |
|        - |  6125 | `/*` |
|        - |  6126 | ` * OP_CLONE * * *` |
|        - |  6127 | ` * Perfome a clone operation.` |
|        - |  6128 | ` */` |
|       23 |  6129 | `case PH7_OP_CLONE: {` |
|        - |  6130 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6131 | `#ifdef UNTRUST` |
|        - |  6132 | `	if( pTos < pStack ){` |
|        - |  6133 | `		goto Abort;` |
|        - |  6134 | `	}` |
|        - |  6135 | `#endif` |
|        - |  6136 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6137 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6138 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6139 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6140 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6141 | `		break;` |
|        - |  6142 | `	}` |
|        - |  6143 | `	/* Point to the source */` |
|       44 |  6144 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6145 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6146 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6147 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6148 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6149 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6150 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6151 | `		break;` |
|        - |  6152 | `	}` |
|        - |  6153 | `	/* Perform the clone operation */` |
|       44 |  6154 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6155 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6156 | `	if( pClone == 0 ){` |
|      ! 0 |  6157 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6158 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6159 | `	}else{` |
|        - |  6160 | `		/* Load the cloned object */` |
|       44 |  6161 | `		pTos->x.pOther = pClone;` |
|       44 |  6162 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6163 | `	}` |
|       44 |  6164 | `	break;` |
|        - |  6165 | `				   }` |
|        - |  6166 | `/*` |
|        - |  6167 | ` * OP_SWITCH * * P3` |
|        - |  6168 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6169 | ` */` |
|       26 |  6170 | `case PH7_OP_SWITCH: {` |
|       54 |  6171 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6172 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6173 | `	ph7_value sValue,sCaseValue;` |
|        - |  6174 | `	sxu32 n,nEntry;` |
|        - |  6175 | `#ifdef UNTRUST` |
|        - |  6176 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6177 | `		goto Abort;` |
|        - |  6178 | `	}` |
|        - |  6179 | `#endif` |
|        - |  6180 | `	/* Point to the case table  */` |
|       54 |  6181 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6182 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6183 | `	/* Select the appropriate case block to execute */` |
|       54 |  6184 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6185 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6186 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6187 | `		pCase = &aCase[n];` |
|      130 |  6188 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6189 | `		/* Execute the case expression first */` |
|      130 |  6190 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6191 | `		/* Compare the two expression */` |
|      130 |  6192 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6193 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6194 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6195 | `		if( rc == 0 ){` |
|        - |  6196 | `			/* Value match,jump to this block */` |
|       52 |  6197 | `			pc = pCase->nStart - 1;` |
|       52 |  6198 | `			break;` |
|        - |  6199 | `		}` |
|       41 |  6200 | `	}` |
|       54 |  6201 | `	VmPopOperand(&pTos,1);` |
|       54 |  6202 | `	if( n >= nEntry ){` |
|        - |  6203 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6204 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6205 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6206 | `		}else{` |
|        - |  6207 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6208 | `			pc = pSwitch->nOut - 1;` |
|        - |  6209 | `		}` |
|        1 |  6210 | `	}` |
|       54 |  6211 | `	break;` |
|        - |  6212 | `					}` |
|        - |  6213 | `/*` |
|        - |  6214 | ` * OP_YIELD P1 P2 *` |
|        - |  6215 | ` *  Yield a value from a generator function.` |
|        - |  6216 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6217 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6218 | ` */` |
|       28 |  6219 | `case PH7_OP_YIELD: {` |
|        - |  6220 | `	ph7_generator *pGen;` |
|       58 |  6221 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6222 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6223 | `		goto Abort;` |
|        - |  6224 | `	}` |
|       58 |  6225 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6226 | `	if( pInstr->iP2 ){` |
|        - |  6227 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6228 | `#ifdef UNTRUST` |
|        - |  6229 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6230 | `#endif` |
|        7 |  6231 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6232 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6233 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6234 | `		VmPopOperand(&pTos, 1);` |
|        - |  6235 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6236 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6237 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6238 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6239 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6240 | `			}` |
|        1 |  6241 | `		}` |
|       55 |  6242 | `	}else if( pInstr->iP1 ){` |
|        - |  6243 | `		/* yield $value */` |
|        - |  6244 | `#ifdef UNTRUST` |
|        - |  6245 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6246 | `#endif` |
|       52 |  6247 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6248 | `		VmPopOperand(&pTos, 1);` |
|        - |  6249 | `		/* Auto-increment key */` |
|       52 |  6250 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6251 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6252 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6253 | `	}else{` |
|        - |  6254 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6255 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6256 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6257 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6258 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6259 | `	}` |
|        - |  6260 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6261 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6262 | `	goto Suspend;` |
|        - |  6263 |  |
|        - |  6264 | `/*` |
|        - |  6265 | ` * OP_CALL P1 * *` |
|        - |  6266 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6267 | ` *  function on the stack.` |
|        - |  6268 | ` */` |
|   305266 |  6269 | `case PH7_OP_CALL: {` |
|   610578 |  6270 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6271 | `	ph7_value *pArg;` |
|   610578 |  6272 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   610578 |  6273 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6274 | `	SyHashEntry *pEntry;` |
|        - |  6275 | `	SyString sName;` |
|        - |  6276 | `	/* Extract function name */` |
|   610578 |  6277 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6278 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6279 | `			ph7_value sResult;` |
|      ! 0 |  6280 | `			SySetReset(&aArg);` |
|      ! 0 |  6281 | `			while( pArg < pTos ){` |
|      ! 0 |  6282 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6283 | `				pArg++;` |
|      ! 0 |  6284 | `			}` |
|      ! 0 |  6285 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6286 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6287 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6288 | `			SySetReset(&aArg);` |
|        - |  6289 | `			/* Pop given arguments */` |
|      ! 0 |  6290 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6291 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6292 | `			}` |
|        - |  6293 | `			/* Copy result */` |
|      ! 0 |  6294 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6295 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6296 | `		}else{` |
|        3 |  6297 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6298 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6299 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6300 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6301 | `			}else{` |
|        - |  6302 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6303 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6304 | `			}` |
|        - |  6305 | `			/* Pop given arguments */` |
|        3 |  6306 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6307 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6308 | `			}` |
|        - |  6309 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6310 | `			PH7_MemObjRelease(pTos);` |
|        - |  6311 | `		}` |
|   304988 |  6312 | `		break;` |
|        - |  6313 | `	}` |
|   610576 |  6314 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6315 | `	/* Check for a compiled function first.` |
|        - |  6316 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6317 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   610576 |  6318 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6319 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6320 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6321 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6322 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6323 | `	 * function calls inside namespaces. */` |
|   610576 |  6324 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6325 | `		const char *zFunc;` |
|        - |  6326 | `		const char *zEnd;` |
|        - |  6327 | `		const char *z;` |
|        - |  6328 | `		SyString sGlobal;` |
|       18 |  6329 | `		zFunc = sName.zString;` |
|       18 |  6330 | `		zEnd  = zFunc + sName.nByte;` |
|       18 |  6331 | `		z = zEnd;` |
|        - |  6332 | `		/* Find last namespace separator */` |
|      154 |  6333 | `		while( z > zFunc ){` |
|      154 |  6334 | `			if( z[-1] == '\\' ){` |
|       18 |  6335 | `				break;` |
|        - |  6336 | `			}` |
|      138 |  6337 | `			z--;` |
|        2 |  6338 | `		}` |
|       18 |  6339 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6340 | `			/* Retry lookup using the unqualified/global function name */` |
|       18 |  6341 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       18 |  6342 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        8 |  6343 | `		}` |
|        8 |  6344 | `	}` |
|   610576 |  6345 | `	if( pEntry ){` |
|        - |  6346 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6347 | `		ph7_class_instance *pThis;` |
|        - |  6348 | `		ph7_value *pFrameStack;` |
|        - |  6349 | `		ph7_vm_func *pVmFunc;` |
|        - |  6350 | `		ph7_class *pSelf;` |
|        - |  6351 | `		VmFrame *pFrame;` |
|        - |  6352 | `		ph7_value *pObj;` |
|        - |  6353 | `		VmSlot sArg;` |
|        - |  6354 | `		sxu32 n;` |
|        - |  6355 | `		/* initialize fields */` |
|    13798 |  6356 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13798 |  6357 | `		pThis = 0;` |
|    13798 |  6358 | `		pSelf = 0;` |
|    13798 |  6359 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6360 | `			ph7_class_method *pMeth;` |
|        - |  6361 | `			/* Class method call */` |
|     2104 |  6362 | `			ph7_value *pTarget = &pTos[-1];` |
|     2104 |  6363 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6364 | `				/* Extract the 'this' pointer */` |
|     2104 |  6365 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6366 | `					/* Instance already loaded */` |
|     2024 |  6367 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2024 |  6368 | `					pThis->iRef++;` |
|     2024 |  6369 | `					pSelf = pThis->pClass;` |
|     1011 |  6370 | `				}` |
|     2104 |  6371 | `				if( pSelf == 0 ){` |
|       82 |  6372 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6373 | `						/* "Late Static Binding" class name */` |
|      113 |  6374 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       37 |  6375 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       37 |  6376 | `					}` |
|       82 |  6377 | `					if( pSelf == 0 ){` |
|       19 |  6378 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  6379 | `					}` |
|       40 |  6380 | `				}` |
|     2104 |  6381 | `				if( pThis == 0  ){` |
|       82 |  6382 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       82 |  6383 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       82 |  6384 | `					if( pFrameLocal->pParent ){` |
|        - |  6385 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  6386 | `						pThis = pFrameLocal->pThis;` |
|       64 |  6387 | `						if( pThis ){` |
|       19 |  6388 | `							pThis->iRef++;` |
|        9 |  6389 | `						}` |
|       31 |  6390 | `					}` |
|       40 |  6391 | `				}` |
|     2104 |  6392 | `				VmPopOperand(&pTos,1);` |
|     2104 |  6393 | `				PH7_MemObjRelease(pTos);` |
|        - |  6394 | `				/* Synchronize pointers */` |
|     2104 |  6395 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6396 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6397 | `				 * user have already computed the random generated unique class method name` |
|        - |  6398 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6399 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6400 | `				 */` |
|     2104 |  6401 | `				while( pArg < pStack ){` |
|      ! 0 |  6402 | `					pArg++;` |
|      ! 0 |  6403 | `				}` |
|     2104 |  6404 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6405 | `					/* Check if the call is allowed */` |
|     2104 |  6406 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2104 |  6407 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  6408 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  6409 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  6410 | `							char zMsg[256];` |
|      ! 0 |  6411 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6412 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  6413 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  6414 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  6415 | `							/* Pop given arguments */` |
|      ! 0 |  6416 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6417 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6418 | `							}` |
|      ! 0 |  6419 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6420 | `							goto Abort;` |
|        - |  6421 | `						}` |
|        6 |  6422 | `					}` |
|     1051 |  6423 | `				}` |
|     1051 |  6424 | `			}` |
|     1051 |  6425 | `		}` |
|        - |  6426 | `		/* Check The recursion limit */` |
|    13798 |  6427 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6428 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6429 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6430 | `				&pVmFunc->sName);` |
|        - |  6431 | `			/* Pop given arguments */` |
|        3 |  6432 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6433 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6434 | `			}` |
|        - |  6435 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6436 | `			PH7_MemObjRelease(pTos);` |
|       12 |  6437 | `			break;` |
|        - |  6438 | `		}` |
|    13796 |  6439 | `		if( pVmFunc->pNextName ){` |
|        - |  6440 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  6441 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  6442 | `		}` |
|    13796 |  6443 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6444 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6445 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6446 | `			ph7_generator *pGenerator;` |
|        - |  6447 | `			ph7_class_instance *pGenObj;` |
|        - |  6448 | `			ph7_value *pCtxAttr;` |
|        - |  6449 | `			SyString sAttrName;` |
|        - |  6450 | `			ph7_value **apCallArgs;` |
|        - |  6451 | `			int nGenArgs, iArg;` |
|        - |  6452 | `			/* Collect arguments from the operand stack */` |
|       20 |  6453 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6454 | `			apCallArgs = 0;` |
|       20 |  6455 | `			if( nGenArgs > 0 ){` |
|        8 |  6456 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6457 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6458 | `				if( apCallArgs == 0 ){` |
|        - |  6459 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6460 | `					nGenArgs = 0;` |
|      ! 0 |  6461 | `				}else{` |
|       12 |  6462 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6463 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6464 | `					}` |
|        - |  6465 | `				}` |
|        2 |  6466 | `			}` |
|        - |  6467 | `			/* Create execution context and generator wrapper */` |
|       20 |  6468 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6469 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6470 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6471 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6472 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6473 | `				break;` |
|        - |  6474 | `			}` |
|       20 |  6475 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6476 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6477 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6478 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6479 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6480 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6481 | `				break;` |
|        - |  6482 | `			}` |
|        - |  6483 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6484 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6485 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6486 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6487 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6488 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6489 | `			if( apCallArgs ){` |
|        6 |  6490 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6491 | `			}` |
|       20 |  6492 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6493 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6494 | `				if( pThis ){` |
|      ! 0 |  6495 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6496 | `				}` |
|      ! 0 |  6497 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6498 | `					goto Abort;` |
|        - |  6499 | `				}` |
|      ! 0 |  6500 | `				break;` |
|        - |  6501 | `			}` |
|        - |  6502 | `			/* Create Generator class instance */` |
|       20 |  6503 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6504 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6505 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6506 | `				break;` |
|        - |  6507 | `			}` |
|        - |  6508 | `			/* Store generator in __ctx attribute */` |
|       20 |  6509 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6510 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6511 | `			if( pCtxAttr ){` |
|       20 |  6512 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6513 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6514 | `			}` |
|        - |  6515 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6516 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6517 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6518 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6519 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6520 | `			pGenObj->iRef++;` |
|       20 |  6521 | `			if( pThis ){` |
|      ! 0 |  6522 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6523 | `			}` |
|       20 |  6524 | `			break;` |
|        - |  6525 | `		}` |
|        - |  6526 | `		/* Extract the formal argument set */` |
|    13778 |  6527 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6528 | `		/* Create a new VM frame  */` |
|    13778 |  6529 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13778 |  6530 | `		if( rc != SXRET_OK ){` |
|        - |  6531 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6532 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6533 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6534 | `				&pVmFunc->sName);` |
|        - |  6535 | `			/* Pop given arguments */` |
|      ! 0 |  6536 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6537 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6538 | `			}` |
|        - |  6539 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6540 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6541 | `			break;` |
|        - |  6542 | `		}` |
|    13778 |  6543 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6544 | `			/* Install the '$this' variable */` |
|        - |  6545 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2040 |  6546 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2040 |  6547 | `			if( pObj ){` |
|        - |  6548 | `				/* Reflect the change */` |
|     2040 |  6549 | `				pObj->x.pOther = pThis;` |
|     2040 |  6550 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1019 |  6551 | `			}` |
|     1019 |  6552 | `		}` |
|    13778 |  6553 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6554 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6555 | `			/* Install static variables */` |
|      ! 0 |  6556 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6557 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6558 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6559 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6560 | `					/* Initialize the static variables */` |
|      ! 0 |  6561 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6562 | `					if( pObj ){` |
|        - |  6563 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6564 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6565 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6566 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6567 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6568 | `						}` |
|      ! 0 |  6569 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6570 | `					}else{` |
|      ! 0 |  6571 | `						continue;` |
|        - |  6572 | `					}` |
|      ! 0 |  6573 | `				}` |
|        - |  6574 | `				/* Install in the current frame */` |
|      ! 0 |  6575 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6576 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6577 | `			}` |
|      ! 0 |  6578 | `		}` |
|        - |  6579 | `		/* Push arguments in the local frame */` |
|    13778 |  6580 | `		n = 0;` |
|    37216 |  6581 | `		while( pArg < pTos ){` |
|    23476 |  6582 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6583 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       28 |  6584 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       28 |  6585 | `				if( pObj ){` |
|        - |  6586 | `					/* Initialize as empty array */` |
|       28 |  6587 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6588 | `					{` |
|       28 |  6589 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      104 |  6590 | `						while( pArg < pTos ){` |
|        - |  6591 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  6592 | `							 * Nullable types (?type) allow null through without coercion. */` |
|       92 |  6593 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  6594 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  6595 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6596 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  6597 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  6598 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  6599 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  6600 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  6601 | `										goto Abort;` |
|        - |  6602 | `									}` |
|        - |  6603 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  6604 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  6605 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  6606 | `									pFrameStack = 0;` |
|      ! 0 |  6607 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  6608 | `									goto SkipFuncBody;` |
|      ! 0 |  6609 | `								}else{` |
|       13 |  6610 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6611 | `									if( xCast ){` |
|       13 |  6612 | `										xCast(pArg);` |
|        6 |  6613 | `									}` |
|        - |  6614 | `								}` |
|        6 |  6615 | `							}` |
|       78 |  6616 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       78 |  6617 | `							pArg++;` |
|        2 |  6618 | `						}` |
|        - |  6619 | `					}` |
|       28 |  6620 | `					sArg.nIdx = pObj->nIdx;` |
|       28 |  6621 | `					sArg.pUserData = 0;` |
|       28 |  6622 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6623 | `				}` |
|       28 |  6624 | `				break; /* All remaining args consumed */` |
|        - |  6625 | `			}` |
|    23450 |  6626 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    23294 |  6627 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       11 |  6628 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6629 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6630 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6631 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6632 | `						goto Abort;` |
|        - |  6633 | `					}` |
|      ! 0 |  6634 | `				}` |
|        - |  6635 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6636 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    23308 |  6637 | `				if( aFormalArg[n].nType > 0` |
|    12246 |  6638 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1182 |  6639 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6640 | `						/* Argument must be a class instance [i.e: object] */` |
|       16 |  6641 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6642 | `						ph7_class *pClass;` |
|        - |  6643 | `						/* Try to extract the desired class */` |
|       16 |  6644 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  6645 | `						if( pClass ){` |
|       16 |  6646 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6647 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6648 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6649 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6650 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6651 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6652 | `								}` |
|      ! 0 |  6653 | `							}else{` |
|        - |  6654 | `								/* reuse pThis declared in outer scope */` |
|       16 |  6655 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6656 | `								/* Make sure the object is an instance of the given class */` |
|       16 |  6657 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6658 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6659 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6660 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6661 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6662 | `								}` |
|        - |  6663 | `							}` |
|        9 |  6664 | `						}` |
|     1175 |  6665 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  6666 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  6667 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  6668 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  6669 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  6670 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  6671 | `								goto Abort;` |
|        - |  6672 | `							}` |
|        - |  6673 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  6674 | `							PH7_MemObjRelease(pTos);` |
|       11 |  6675 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  6676 | `							pFrameStack = 0;` |
|       11 |  6677 | `							rc = PH7_EXCEPTION;` |
|       11 |  6678 | `							goto SkipFuncBody;` |
|      ! 0 |  6679 | `						}else{` |
|      ! 0 |  6680 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6681 | `							/* Cast to the desired type */` |
|      ! 0 |  6682 | `							xCast(pArg);` |
|        - |  6683 | `						}` |
|      ! 0 |  6684 | `					}` |
|      585 |  6685 | `				}` |
|    23286 |  6686 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6687 | `					/* Pass by reference */` |
|       54 |  6688 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6689 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6690 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6691 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6692 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6693 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6694 | `						}` |
|        - |  6695 | `						/* Switch to pass by value */` |
|      ! 0 |  6696 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6697 | `					}else{` |
|        - |  6698 | `						SyHashEntry *pRefEntry;` |
|        - |  6699 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6700 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6701 | `						if( pRefEntry == 0 ){` |
|       80 |  6702 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6703 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6704 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6705 | `							sArg.pUserData = 0;` |
|       54 |  6706 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6707 | `						}` |
|       54 |  6708 | `						pObj = 0;` |
|        - |  6709 | `					}` |
|       28 |  6710 | `				}else{` |
|        - |  6711 | `					/* Pass by value,make a copy of the given argument */` |
|    23234 |  6712 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6713 | `				}` |
|    11644 |  6714 | `			}else{` |
|        - |  6715 | `				char zName[32];` |
|        - |  6716 | `				SyString sArgName;` |
|        - |  6717 | `				/* Set a dummy name */` |
|      156 |  6718 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6719 | `				sArgName.zString = zName;` |
|        - |  6720 | `				/* Annonymous argument */` |
|      156 |  6721 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6722 | `			}` |
|    23440 |  6723 | `			if( pObj ){` |
|    23388 |  6724 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6725 | `				/* Insert argument index  */` |
|    23388 |  6726 | `				sArg.nIdx = pObj->nIdx;` |
|    23388 |  6727 | `				sArg.pUserData = 0;` |
|    23388 |  6728 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11693 |  6729 | `			}` |
|    23440 |  6730 | `			PH7_MemObjRelease(pArg);` |
|    23440 |  6731 | `			pArg++;` |
|    23440 |  6732 | `			++n;` |
|        2 |  6733 | `		}` |
|        - |  6734 | `		/* Set up closure environment */` |
|    13768 |  6735 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6736 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6737 | `			ph7_value *pValue;` |
|        - |  6738 | `			sxu32 iEnv;` |
|       13 |  6739 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       39 |  6740 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       27 |  6741 | `				pEnv = &aEnv[iEnv];` |
|       27 |  6742 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6743 | `					/* Do not install null value */` |
|       13 |  6744 | `					continue;` |
|        - |  6745 | `				}` |
|       15 |  6746 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       15 |  6747 | `				if( pValue == 0 ){` |
|      ! 0 |  6748 | `					continue;` |
|        - |  6749 | `				}` |
|        - |  6750 | `				/* Invalidate any prior representation */` |
|       15 |  6751 | `				PH7_MemObjRelease(pValue);` |
|        - |  6752 | `				/* Duplicate bound variable value */` |
|       15 |  6753 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        8 |  6754 | `			}` |
|        6 |  6755 | `		}` |
|        - |  6756 | `		/* Process default values for remaining formal parameters */` |
|    15808 |  6757 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2074 |  6758 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6759 | `				/* Variadic parameter with no extra args — create empty array */` |
|       34 |  6760 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       34 |  6761 | `				if( pObj ){` |
|       34 |  6762 | `					PH7_MemObjToHashmap(pObj);` |
|       34 |  6763 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  6764 | `					sArg.pUserData = 0;` |
|       34 |  6765 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  6766 | `				}` |
|       34 |  6767 | `				n++;` |
|       34 |  6768 | `				break; /* Variadic is always last */` |
|        - |  6769 | `			}` |
|     2042 |  6770 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2036 |  6771 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2036 |  6772 | `				if( pObj ){` |
|        - |  6773 | `					/* Evaluate the default value and extract it's result */` |
|     2036 |  6774 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2036 |  6775 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6776 | `						goto Abort;` |
|        - |  6777 | `					}` |
|        - |  6778 | `					/* Insert argument index */` |
|     2036 |  6779 | `					sArg.nIdx = pObj->nIdx;` |
|     2036 |  6780 | `					sArg.pUserData = 0;` |
|     2036 |  6781 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6782 | `					/* Make sure the default argument is of the correct type */` |
|     2034 |  6783 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1444 |  6784 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6785 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6786 | `						/* Cast to the desired type */` |
|      ! 0 |  6787 | `						xCast(pObj);` |
|      ! 0 |  6788 | `					}` |
|     1017 |  6789 | `				}` |
|     1017 |  6790 | `			}` |
|     2042 |  6791 | `			++n;` |
|        2 |  6792 | `		}` |
|        - |  6793 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6794 | `		 * does not return anything.` |
|        - |  6795 | `		 */` |
|    13768 |  6796 | `		PH7_MemObjRelease(pTos);` |
|    13768 |  6797 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6798 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13768 |  6799 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13768 |  6800 | `		if( pFrameStack == 0 ){` |
|        - |  6801 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6802 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6803 | `				&pVmFunc->sName);` |
|      ! 0 |  6804 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6805 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6806 | `			}` |
|      ! 0 |  6807 | `			break;` |
|        - |  6808 | `		}` |
|     6883 |  6809 | `SkipFuncBody:` |
|    13778 |  6810 | `		if( pSelf ){` |
|        - |  6811 | `			/* Push class name */` |
|     2102 |  6812 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1050 |  6813 | `		}` |
|        - |  6814 | `		/* Increment nesting level */` |
|    13778 |  6815 | `		pVm->nRecursionDepth++;` |
|    13778 |  6816 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  6817 | `			/* Execute function body */` |
|    13768 |  6818 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     6883 |  6819 | `		}` |
|        - |  6820 | `		/* Decrement nesting level */` |
|    13778 |  6821 | `		pVm->nRecursionDepth--;` |
|    13778 |  6822 | `		if( pSelf ){` |
|        - |  6823 | `			/* Pop class name */` |
|     2102 |  6824 | `			(void)SySetPop(&pVm->aSelf);` |
|     1050 |  6825 | `		}` |
|        - |  6826 | `		/* Cleanup the mess left behind */` |
|    13778 |  6827 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6828 | `			/* Return by reference,reflect that */` |
|        9 |  6829 | `			if( n != SXU32_HIGH ){` |
|        9 |  6830 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6831 | `				sxu32 i;` |
|        - |  6832 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6833 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6834 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6835 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6836 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6837 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6838 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6839 | `								&pVmFunc->sName);` |
|      ! 0 |  6840 | `						}` |
|      ! 0 |  6841 | `						n = SXU32_HIGH;` |
|      ! 0 |  6842 | `						break;` |
|        - |  6843 | `					}` |
|        3 |  6844 | `				}` |
|        5 |  6845 | `			}else{` |
|      ! 0 |  6846 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6847 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6848 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6849 | `						&pVmFunc->sName);` |
|      ! 0 |  6850 | `				}` |
|        - |  6851 | `			}` |
|        9 |  6852 | `			pTos->nIdx = n;` |
|        4 |  6853 | `		}` |
|        - |  6854 | `		/* Cleanup the mess left behind */` |
|    13778 |  6855 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6856 | `			/* An exception was throw in this frame */` |
|       22 |  6857 | `			pFrame = pFrame->pParent;` |
|       22 |  6858 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6859 | `				/* Pop the resutlt */` |
|       20 |  6860 | `				VmPopOperand(&pTos,1);` |
|        - |  6861 | `				/* Jump to this destination */` |
|       20 |  6862 | `				pc = pFrame->iExceptionJump - 1;` |
|       20 |  6863 | `				rc = PH7_OK;` |
|       11 |  6864 | `			}else{` |
|        3 |  6865 | `				if( pFrame->pParent ){` |
|        3 |  6866 | `					rc = PH7_EXCEPTION;` |
|        2 |  6867 | `				}else{` |
|        - |  6868 | `					/* Continue normal execution */` |
|      ! 0 |  6869 | `					rc = PH7_OK;` |
|        - |  6870 | `				}` |
|        - |  6871 | `			}` |
|       10 |  6872 | `		}` |
|        - |  6873 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    13778 |  6874 | `		if( pFrameStack ){` |
|    13768 |  6875 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     6883 |  6876 | `		}` |
|        - |  6877 | `		/* Leave the frame */` |
|    13778 |  6878 | `		VmLeaveFrame(&(*pVm));` |
|    13778 |  6879 | `		if( rc == PH7_ABORT ){` |
|        - |  6880 | `			/* Abort processing immeditaley */` |
|        9 |  6881 | `			goto Abort;` |
|    13770 |  6882 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6883 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6884 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6885 | `			 * overwriting the state saved by the inner level.` |
|        - |  6886 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6887 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6888 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6889 | `			goto Suspend;` |
|    13732 |  6890 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6891 | `			goto Exception;` |
|        - |  6892 | `		}` |
|     6866 |  6893 | `	}else{` |
|        - |  6894 | `		ph7_user_func *pFunc;` |
|        - |  6895 | `		ph7_context sCtx;` |
|        - |  6896 | `		ph7_value sRet;` |
|        - |  6897 | `		/* Look for an installed foreign function.` |
|        - |  6898 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6899 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6900 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6901 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6902 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6903 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   596780 |  6904 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   596780 |  6905 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6906 | `			/* Compiler-qualified: try short name as global fallback */` |
|       18 |  6907 | `			const char *zShort = sName.zString;` |
|        - |  6908 | `			sxu32 i;` |
|      262 |  6909 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      246 |  6910 | `				if( sName.zString[i] == '\\' ){` |
|       22 |  6911 | `					zShort = &sName.zString[i + 1];` |
|       10 |  6912 | `				}` |
|      124 |  6913 | `			}` |
|       18 |  6914 | `			if( zShort != sName.zString ){` |
|       18 |  6915 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       18 |  6916 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        8 |  6917 | `			}` |
|        8 |  6918 | `		}` |
|   596780 |  6919 | `		if( pEntry == 0 ){` |
|        - |  6920 | `			/* Call to undefined function */` |
|        5 |  6921 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6922 | `			/* Pop given arguments */` |
|        5 |  6923 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6924 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6925 | `			}` |
|        - |  6926 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6927 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6928 | `			break;` |
|        - |  6929 | `		}` |
|   596776 |  6930 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6931 | `		/* Start collecting function arguments */` |
|   596776 |  6932 | `		SySetReset(&aArg);` |
|  1603584 |  6933 | `		while( pArg < pTos ){` |
|  1006810 |  6934 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1006810 |  6935 | `			pArg++;` |
|        2 |  6936 | `		}` |
|        - |  6937 | `		/* Assume a null return value */` |
|   596776 |  6938 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6939 | `		/* Init the call context */` |
|   596776 |  6940 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6941 | `		/* Call the foreign function */` |
|   596776 |  6942 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6943 | `		/* Release the call context */` |
|   596776 |  6944 | `		VmReleaseCallContext(&sCtx);` |
|   596776 |  6945 | `		if( rc == PH7_ABORT ){` |
|      471 |  6946 | `			goto Abort;` |
|   596306 |  6947 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6948 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6949 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6950 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6951 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6952 | `				goto Exception;` |
|        - |  6953 | `			}` |
|        - |  6954 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6955 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6956 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6957 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6958 | `			}` |
|        - |  6959 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6960 | `			VmPopOperand(&pTos,1);` |
|        - |  6961 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6962 | `			pFrm = pVm->pFrame;` |
|        7 |  6963 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6964 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6965 | `			}` |
|        7 |  6966 | `			break;` |
|        - |  6967 | `		}` |
|   596296 |  6968 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6969 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6970 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6971 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6972 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6973 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6974 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6975 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6976 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6977 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6978 | `			}` |
|        - |  6979 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6980 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6981 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6982 | `			goto Suspend;` |
|        - |  6983 | `		}` |
|   596258 |  6984 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6985 | `			/* Pop function name and arguments */` |
|   577254 |  6986 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   288648 |  6987 | `		}` |
|        - |  6988 | `		/* Save foreign function return value */` |
|   596258 |  6989 | `		PH7_MemObjStore(&sRet,pTos);` |
|   596258 |  6990 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6991 | `	}` |
|   609986 |  6992 | `	break;` |
|        - |  6993 | `				  }` |
|        - |  6994 | `/*` |
|        - |  6995 | ` * OP_CONSUME: P1 * *` |
|        - |  6996 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6997 | ` */` |
|    12218 |  6998 | `case PH7_OP_CONSUME: {` |
|    24438 |  6999 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    24438 |  7000 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  7001 |  |
|    24438 |  7002 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    24438 |  7003 | `	pCur = pOut;` |
|        - |  7004 | `	/* Start the consume process  */` |
|    48874 |  7005 | `	while( pOut <= pTos ){` |
|        - |  7006 | `		/* Force a string cast */` |
|    24438 |  7007 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  7008 | `			PH7_MemObjToString(pOut);` |
|      149 |  7009 | `		}` |
|    24438 |  7010 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  7011 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  7012 | `			/* Invoke the output consumer callback */` |
|    13764 |  7013 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13764 |  7014 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13764 |  7015 | `			SyBlobRelease(&pOut->sBlob);` |
|    13764 |  7016 | `			if( rc == SXERR_ABORT ){` |
|        - |  7017 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  7018 | `				goto Abort;` |
|        - |  7019 | `			}` |
|     6881 |  7020 | `		}` |
|    24438 |  7021 | `		pOut++;` |
|        2 |  7022 | `	}` |
|    24438 |  7023 | `	pTos = &pCur[-1];` |
|    24436 |  7024 | `	break;` |
|        - |  7025 | `					 }` |
|        - |  7026 |  |
|        - |  7027 | `		} /* Switch() */` |
| 10272494 |  7028 | `		pc++; /* Next instruction in the stream */` |
|        2 |  7029 | `	} /* For(;;) */` |
|    16757 |  7030 | `Done:` |
|    33516 |  7031 | `	SySetRelease(&aArg);` |
|    33516 |  7032 | `	return SXRET_OK;` |
|       66 |  7033 | `Suspend:` |
|      134 |  7034 | `	SySetRelease(&aArg);` |
|      134 |  7035 | `	return PH7_SUSPEND;` |
|      245 |  7036 | `Abort:` |
|      491 |  7037 | `	SySetRelease(&aArg);` |
|     1697 |  7038 | `	while( pTos >= pStack ){` |
|     1207 |  7039 | `		PH7_MemObjRelease(pTos);` |
|     1207 |  7040 | `		pTos--;` |
|        1 |  7041 | `	}` |
|      491 |  7042 | `	return PH7_ABORT;` |
|        3 |  7043 | `Exception:` |
|        8 |  7044 | `	SySetRelease(&aArg);` |
|       22 |  7045 | `	while( pTos >= pStack ){` |
|       16 |  7046 | `		PH7_MemObjRelease(pTos);` |
|       16 |  7047 | `		pTos--;` |
|        2 |  7048 | `	}` |
|        8 |  7049 | `	return PH7_EXCEPTION;` |
|    17073 |  7050 |  |
|        - |  7051 | `/*` |
|        - |  7052 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  7053 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7054 | ` * See block-comment on that function for additional information.` |
|        - |  7055 | ` */` |
|    15852 |  7056 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  7057 |  |
|        - |  7058 | `	ph7_value *pStack;` |
|        - |  7059 | `	sxi32 rc;` |
|        - |  7060 | `	/* Allocate a new operand stack */` |
|    15854 |  7061 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15854 |  7062 | `	if( pStack == 0 ){` |
|      ! 0 |  7063 | `		return SXERR_MEM;` |
|        - |  7064 | `	}` |
|        - |  7065 | `	/* Execute the program */` |
|    15854 |  7066 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  7067 | `	/* Free the operand stack */` |
|    15854 |  7068 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  7069 | `	/* Execution result */` |
|    15854 |  7070 | `	return rc;` |
|     7928 |  7071 |  |
|        - |  7072 | `/*` |
|        - |  7073 | ` * Invoke any installed shutdown callbacks.` |
|        - |  7074 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  7075 | ` * or more calls to [register_shutdown_function()].` |
|        - |  7076 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  7077 | ` * execution ends.` |
|        - |  7078 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  7079 | ` * additional information.` |
|        - |  7080 | ` */` |
|     2382 |  7081 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  7082 |  |
|        - |  7083 | `	VmShutdownCB *pEntry;` |
|        - |  7084 | `	ph7_value *apArg[10];` |
|        - |  7085 | `	sxu32 n,nEntry;` |
|        - |  7086 | `	int i;` |
|        - |  7087 | `	/* Point to the stack of registered callbacks */` |
|     2384 |  7088 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    26204 |  7089 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23822 |  7090 | `		apArg[i] = 0;` |
|    11912 |  7091 | `	}` |
|     2386 |  7092 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  7093 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7094 | `		if( pEntry ){` |
|        - |  7095 | `			/* Prepare callback arguments if any */` |
|        3 |  7096 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  7097 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  7098 | `					break;` |
|        - |  7099 | `				}` |
|      ! 0 |  7100 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  7101 | `			}` |
|        - |  7102 | `			/* Invoke the callback */` |
|        3 |  7103 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  7104 | `			/*` |
|        - |  7105 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  7106 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  7107 | `			 */` |
|        3 |  7108 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7109 | `			if( pEntry ){` |
|        3 |  7110 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  7111 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  7112 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  7113 | `				}` |
|        1 |  7114 | `			}` |
|        1 |  7115 | `		}` |
|        2 |  7116 | `	}` |
|     2384 |  7117 | `	SySetReset(&pVm->aShutdown);` |
|     2384 |  7118 |  |
|        - |  7119 | `/*` |
|        - |  7120 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  7121 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7122 | ` * See block-comment on that function for additional information.` |
|        - |  7123 | ` */` |
|     2390 |  7124 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  7125 |  |
|        - |  7126 | `	/* Make sure we are ready to execute this program */` |
|     2392 |  7127 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  7128 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  7129 | `	}` |
|        - |  7130 | `	/* Set the execution magic number  */` |
|     2392 |  7131 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  7132 | `	/* Execute the program */` |
|     2392 |  7133 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  7134 | `	/* Invoke any shutdown callbacks */` |
|     2388 |  7135 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  7136 | `	/*` |
|        - |  7137 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  7138 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  7139 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  7140 | `	 */` |
|     2388 |  7141 | `	return SXRET_OK;` |
|     1197 |  7142 |  |
|        - |  7143 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  7144 | `/*` |
|        - |  7145 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  7146 | ` * The context is in CREATED state and ready to be started.` |
|        - |  7147 | ` */` |
|       42 |  7148 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  7149 |  |
|        - |  7150 | `	ph7_exec_ctx *pCtx;` |
|        - |  7151 | `	ph7_value *pStack;` |
|        - |  7152 | `	VmFrame *pFrame;` |
|       44 |  7153 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  7154 | `	if( pCtx == 0 ){` |
|      ! 0 |  7155 | `		return 0;` |
|        - |  7156 | `	}` |
|       44 |  7157 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  7158 | `	pCtx->pVm = pVm;` |
|       44 |  7159 | `	pCtx->pFunc = pFunc;` |
|       44 |  7160 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7161 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7162 | `	pCtx->pc = 0;` |
|       44 |  7163 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7164 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7165 | `	/* Allocate a private operand stack */` |
|       44 |  7166 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7167 | `	if( pStack == 0 ){` |
|      ! 0 |  7168 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7169 | `		return 0;` |
|        - |  7170 | `	}` |
|       44 |  7171 | `	pCtx->pStack = pStack;` |
|        - |  7172 | `	/* Create a detached frame for the fiber */` |
|       44 |  7173 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7174 | `	if( pFrame == 0 ){` |
|      ! 0 |  7175 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7176 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7177 | `		return 0;` |
|        - |  7178 | `	}` |
|       44 |  7179 | `	pCtx->pFrame = pFrame;` |
|       44 |  7180 | `	return pCtx;` |
|       23 |  7181 |  |
|        - |  7182 | `/*` |
|        - |  7183 | ` * Start executing a fiber context for the first time.` |
|        - |  7184 | ` */` |
|       42 |  7185 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7186 |  |
|        - |  7187 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7188 | `	sxi32 rc;` |
|       44 |  7189 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7190 | `		return SXERR_INVALID;` |
|        - |  7191 | `	}` |
|        - |  7192 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7193 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7194 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7195 | `	/* Save and set the active context */` |
|       44 |  7196 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7197 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7198 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7199 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7200 | `	pVm->nRecursionDepth++;` |
|        - |  7201 | `	/* Execute from the beginning */` |
|       65 |  7202 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7203 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7204 | `	pVm->nRecursionDepth--;` |
|        - |  7205 | `	/* Restore the previous context */` |
|       44 |  7206 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7207 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7208 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7209 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7210 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7211 | `		if( pResult ){` |
|       24 |  7212 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7213 | `		}` |
|       42 |  7214 | `		return SXRET_OK;` |
|        - |  7215 | `	}` |
|        - |  7216 | `	/* Detach frame */` |
|        3 |  7217 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7218 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7219 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7220 | `	}` |
|        3 |  7221 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7222 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7223 | `		return PH7_ABORT;` |
|        - |  7224 | `	}` |
|        3 |  7225 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7226 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7227 | `		return PH7_EXCEPTION;` |
|        - |  7228 | `	}` |
|        - |  7229 | `	/* Normal completion */` |
|        3 |  7230 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7231 | `	if( pResult ){` |
|        3 |  7232 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7233 | `	}` |
|        3 |  7234 | `	return SXRET_OK;` |
|       23 |  7235 |  |
|        - |  7236 | `/*` |
|        - |  7237 | ` * Resume a suspended fiber context.` |
|        - |  7238 | ` */` |
|       86 |  7239 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7240 |  |
|        - |  7241 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7242 | `	sxi32 rc;` |
|       88 |  7243 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7244 | `		return SXERR_INVALID;` |
|        - |  7245 | `	}` |
|        - |  7246 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7247 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7248 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7249 | `	if( pResumeValue ){` |
|       40 |  7250 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7251 | `	}else{` |
|       50 |  7252 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7253 | `	}` |
|       88 |  7254 | `	pCtx->nTos++;` |
|        - |  7255 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7256 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7257 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7258 | `	/* Save and set the active context */` |
|       88 |  7259 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7260 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7261 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7262 | `	pVm->nRecursionDepth++;` |
|        - |  7263 | `	/* Resume execution from saved PC */` |
|      131 |  7264 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7265 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7266 | `	pVm->nRecursionDepth--;` |
|        - |  7267 | `	/* Restore the previous context */` |
|       88 |  7268 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7269 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7270 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7271 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7272 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7273 | `		if( pResult ){` |
|       18 |  7274 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7275 | `		}` |
|       56 |  7276 | `		return SXRET_OK;` |
|        - |  7277 | `	}` |
|        - |  7278 | `	/* Detach frame */` |
|       34 |  7279 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7280 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7281 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7282 | `	}` |
|       34 |  7283 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7284 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7285 | `		return PH7_ABORT;` |
|        - |  7286 | `	}` |
|       34 |  7287 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7288 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7289 | `		return PH7_EXCEPTION;` |
|        - |  7290 | `	}` |
|        - |  7291 | `	/* Normal completion */` |
|       34 |  7292 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7293 | `	if( pResult ){` |
|       20 |  7294 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7295 | `	}` |
|       34 |  7296 | `	return SXRET_OK;` |
|       45 |  7297 |  |
|        - |  7298 | `/*` |
|        - |  7299 | ` * Release an execution context and all its resources.` |
|        - |  7300 | ` */` |
|        4 |  7301 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7302 |  |
|        5 |  7303 | `	if( pCtx == 0 ){` |
|      ! 0 |  7304 | `		return;` |
|        - |  7305 | `	}` |
|        5 |  7306 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7307 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7308 | `		return;` |
|        - |  7309 | `	}` |
|        5 |  7310 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7311 | `	/* Release values */` |
|        5 |  7312 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7313 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7314 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7315 | `	if( pCtx->pFrame ){` |
|        - |  7316 | `		VmSlot *aSlot;` |
|        - |  7317 | `		sxu32 n;` |
|        - |  7318 | `		/* Free local variables */` |
|        5 |  7319 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7320 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7321 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7322 | `		}` |
|        - |  7323 | `		/* Remove local references */` |
|        5 |  7324 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7325 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7326 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7327 | `		}` |
|        5 |  7328 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7329 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7330 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7331 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7332 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7333 | `		pCtx->pFrame = 0;` |
|        2 |  7334 | `	}` |
|        - |  7335 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7336 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7337 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7338 | `	if( pCtx->pStack ){` |
|        5 |  7339 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7340 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7341 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7342 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7343 | `				pTos--;` |
|        1 |  7344 | `			}` |
|        2 |  7345 | `		}` |
|        5 |  7346 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7347 | `		pCtx->pStack = 0;` |
|        2 |  7348 | `	}` |
|        - |  7349 | `	/* Free the context itself */` |
|        5 |  7350 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7351 |  |
|        - |  7352 | `/*` |
|        - |  7353 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7354 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7355 | ` */` |
|       90 |  7356 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7357 |  |
|        - |  7358 | `	ph7_class_instance *pThis;` |
|        - |  7359 | `	SyString sAttr;` |
|        - |  7360 | `	ph7_value *pAttr;` |
|       92 |  7361 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7362 | `		return 0;` |
|        - |  7363 | `	}` |
|       92 |  7364 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7365 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7366 | `		return 0;` |
|        - |  7367 | `	}` |
|       92 |  7368 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7369 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7370 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7371 | `		return 0;` |
|        - |  7372 | `	}` |
|       62 |  7373 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7374 |  |
|        - |  7375 | `/*` |
|        - |  7376 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7377 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7378 | ` */` |
|       38 |  7379 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7380 |  |
|       40 |  7381 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7382 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7383 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7384 | `			"Cannot suspend outside of a fiber");` |
|        - |  7385 | `	}` |
|       40 |  7386 | `	if( nArg > 0 ){` |
|       40 |  7387 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7388 | `	}else{` |
|      ! 0 |  7389 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7390 | `	}` |
|       40 |  7391 | `	return PH7_SUSPEND;` |
|       21 |  7392 |  |
|        - |  7393 | `/*` |
|        - |  7394 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7395 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7396 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7397 | ` */` |
|       24 |  7398 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7399 |  |
|        - |  7400 | `	ph7_class_instance *pThis;` |
|        - |  7401 | `	ph7_value *pAttr;` |
|        - |  7402 | `	SyString sAttrName;` |
|       26 |  7403 | `	if( nArg < 2 ){` |
|      ! 0 |  7404 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7405 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7406 | `	}` |
|       26 |  7407 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7408 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7409 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7410 | `	}` |
|       26 |  7411 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7412 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7413 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7414 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7415 | `	}` |
|        - |  7416 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7417 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7418 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7419 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7420 | `	}` |
|        - |  7421 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7422 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7423 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7424 | `	if( pAttr ){` |
|       26 |  7425 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7426 | `	}` |
|       26 |  7427 | `	return PH7_OK;` |
|       14 |  7428 |  |
|        - |  7429 | `/*` |
|        - |  7430 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7431 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7432 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7433 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7434 | ` */` |
|       24 |  7435 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7436 | `	ph7_class_instance **ppThis)` |
|        2 |  7437 |  |
|       26 |  7438 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7439 | `	ph7_value *pCallable;` |
|        - |  7440 | `	SyString sAttrName;` |
|       26 |  7441 | `	*ppThis = 0;` |
|       26 |  7442 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7443 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7444 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7445 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7446 | `		return 0;` |
|        - |  7447 | `	}` |
|       26 |  7448 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7449 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7450 | `		SyString sName;` |
|        - |  7451 | `		SyHashEntry *pEntry;` |
|        - |  7452 | `		ph7_vm_func *pFunc;` |
|       26 |  7453 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7454 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7455 | `		if( pEntry == 0 ){` |
|      ! 0 |  7456 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7457 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7458 | `			return 0;` |
|        - |  7459 | `		}` |
|       26 |  7460 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7461 | `		return pFunc;` |
|      ! 0 |  7462 | `	}else{` |
|        - |  7463 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7464 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7465 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7466 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7467 | `		if( pMethod == 0 ){` |
|      ! 0 |  7468 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7469 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7470 | `			return 0;` |
|        - |  7471 | `		}` |
|      ! 0 |  7472 | `		*ppThis = pClosure;` |
|      ! 0 |  7473 | `		return &pMethod->sFunc;` |
|        - |  7474 | `	}` |
|       14 |  7475 |  |
|        - |  7476 | `/*` |
|        - |  7477 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7478 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7479 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7480 | ` */` |
|       42 |  7481 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7482 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7483 |  |
|       44 |  7484 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7485 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7486 | `	sxu32 nFormal, n;` |
|        - |  7487 | `	VmSlot sSlot;` |
|        - |  7488 | `	sxi32 rc;` |
|        - |  7489 | `	/* Install $this for closure/method callables */` |
|       44 |  7490 | `	if( pClosureThis ){` |
|        - |  7491 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7492 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7493 | `		if( pObj ){` |
|      ! 0 |  7494 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7495 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7496 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7497 | `		}` |
|      ! 0 |  7498 | `	}` |
|        - |  7499 | `	/* Install static variables */` |
|       44 |  7500 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7501 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7502 | `		ph7_value *pVal;` |
|      ! 0 |  7503 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7504 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7505 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7506 | `			if( pVal ){` |
|      ! 0 |  7507 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7508 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7509 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7510 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7511 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7512 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7513 | `				}` |
|      ! 0 |  7514 | `			}` |
|      ! 0 |  7515 | `		}` |
|      ! 0 |  7516 | `	}` |
|        - |  7517 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7518 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7519 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7520 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7521 | `		ph7_value *pObj;` |
|       12 |  7522 | `		if( n < (sxu32)nArg ){` |
|        - |  7523 | `			/* Argument provided — install with type casting */` |
|       12 |  7524 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7525 | `			if( pObj ){` |
|       12 |  7526 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7527 | `				/* Type casting */` |
|       12 |  7528 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7529 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7530 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7531 | `						if( xCast ){` |
|      ! 0 |  7532 | `							xCast(pObj);` |
|      ! 0 |  7533 | `						}` |
|      ! 0 |  7534 | `					}` |
|      ! 0 |  7535 | `				}` |
|       12 |  7536 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7537 | `				sSlot.pUserData = 0;` |
|       12 |  7538 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7539 | `			}` |
|        5 |  7540 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7541 | `			/* Default value */` |
|      ! 0 |  7542 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7543 | `			if( pObj ){` |
|      ! 0 |  7544 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7545 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7546 | `					return rc;` |
|        - |  7547 | `				}` |
|      ! 0 |  7548 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7549 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7550 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7551 | `						if( xCast ){` |
|      ! 0 |  7552 | `							xCast(pObj);` |
|      ! 0 |  7553 | `						}` |
|      ! 0 |  7554 | `					}` |
|      ! 0 |  7555 | `				}` |
|      ! 0 |  7556 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7557 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7558 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7559 | `			}` |
|      ! 0 |  7560 | `		}` |
|        7 |  7561 | `	}` |
|        - |  7562 | `	/* Install closure environment (captured variables) */` |
|       44 |  7563 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7564 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7565 | `		ph7_value *pValue;` |
|        - |  7566 | `		sxu32 iEnv;` |
|        3 |  7567 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7568 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7569 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7570 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7571 | `				continue;` |
|        - |  7572 | `			}` |
|        5 |  7573 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7574 | `			if( pValue == 0 ){` |
|      ! 0 |  7575 | `				continue;` |
|        - |  7576 | `			}` |
|        5 |  7577 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7578 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7579 | `		}` |
|        1 |  7580 | `	}` |
|       44 |  7581 | `	return SXRET_OK;` |
|       23 |  7582 |  |
|        - |  7583 | `/*` |
|        - |  7584 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7585 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7586 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7587 | ` */` |
|       26 |  7588 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7589 |  |
|       28 |  7590 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7591 | `	ph7_class_instance *pThis;` |
|        - |  7592 | `	ph7_class_instance *pClosureThis;` |
|        - |  7593 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7594 | `	ph7_vm_func *pFunc;` |
|        - |  7595 | `	ph7_value sResult;` |
|        - |  7596 | `	ph7_value *pCtxAttr;` |
|        - |  7597 | `	SyString sAttrName;` |
|        - |  7598 | `	sxi32 rc;` |
|       28 |  7599 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7600 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7601 | `	}` |
|       28 |  7602 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7603 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7604 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7605 | `	if( pExecCtx != 0 ){` |
|        3 |  7606 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7607 | `			"Cannot start a fiber that has already been started");` |
|        - |  7608 | `	}` |
|        - |  7609 | `	/* Resolve callable */` |
|       26 |  7610 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7611 | `	if( pFunc == 0 ){` |
|      ! 0 |  7612 | `		return PH7_EXCEPTION;` |
|        - |  7613 | `	}` |
|        - |  7614 | `	/* Create execution context now that we know the function */` |
|       26 |  7615 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7616 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7617 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7618 | `			"Fiber::start(): out of memory");` |
|        - |  7619 | `	}` |
|        - |  7620 | `	/* Store context in $this->__ctx */` |
|       26 |  7621 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7622 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7623 | `	if( pCtxAttr ){` |
|       26 |  7624 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7625 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7626 | `	}` |
|        - |  7627 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7628 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7629 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7630 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7631 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7632 | `	/* Unpack the args array and install into the frame */` |
|        - |  7633 | `	{` |
|       26 |  7634 | `		ph7_value **apValues = 0;` |
|       26 |  7635 | `		int nActual = 0;` |
|       26 |  7636 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7637 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7638 | `			ph7_hashmap_node *pNode;` |
|       26 |  7639 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7640 | `			if( nCount > 0 ){` |
|        3 |  7641 | `				sxu32 idx = 0;` |
|        4 |  7642 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7643 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7644 | `				if( apValues ){` |
|        3 |  7645 | `					pNode = pMap->pFirst;` |
|        7 |  7646 | `					while( pNode && idx < nCount ){` |
|        5 |  7647 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7648 | `						idx++;` |
|        5 |  7649 | `						pNode = pNode->pPrev;` |
|        1 |  7650 | `					}` |
|        3 |  7651 | `					nActual = (int)idx;` |
|        1 |  7652 | `				}` |
|        1 |  7653 | `			}` |
|       12 |  7654 | `		}` |
|       26 |  7655 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7656 | `		if( apValues ){` |
|        3 |  7657 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7658 | `		}` |
|        - |  7659 | `	}` |
|        - |  7660 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7661 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7662 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7663 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7664 | `		return PH7_ABORT;` |
|        - |  7665 | `	}` |
|       26 |  7666 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7667 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7668 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7669 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7670 | `		return PH7_ABORT;` |
|        - |  7671 | `	}` |
|       26 |  7672 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7673 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7674 | `		return PH7_EXCEPTION;` |
|        - |  7675 | `	}` |
|       26 |  7676 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7677 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7678 | `	return PH7_OK;` |
|       15 |  7679 |  |
|        - |  7680 | `/*` |
|        - |  7681 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7682 | ` */` |
|       36 |  7683 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7684 |  |
|       38 |  7685 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7686 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7687 | `	ph7_value sResult;` |
|        - |  7688 | `	ph7_value *pResumeVal;` |
|        - |  7689 | `	sxi32 rc;` |
|       38 |  7690 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7691 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7692 | `		return PH7_OK;` |
|        - |  7693 | `	}` |
|       38 |  7694 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7695 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7696 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7697 | `		return PH7_OK;` |
|        - |  7698 | `	}` |
|       38 |  7699 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7700 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7701 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7702 | `	}` |
|       36 |  7703 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7704 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7705 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7706 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7707 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7708 | `		return PH7_ABORT;` |
|        - |  7709 | `	}` |
|       36 |  7710 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7711 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7712 | `		return PH7_EXCEPTION;` |
|        - |  7713 | `	}` |
|       36 |  7714 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7715 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7716 | `	return PH7_OK;` |
|       20 |  7717 |  |
|        - |  7718 | `/*` |
|        - |  7719 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7720 | ` */` |
|        6 |  7721 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7722 |  |
|        8 |  7723 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7724 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7725 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7726 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7727 | `		return PH7_OK;` |
|        - |  7728 | `	}` |
|        8 |  7729 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7730 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7731 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7732 | `		return PH7_OK;` |
|        - |  7733 | `	}` |
|        8 |  7734 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7735 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7736 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7737 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7738 | `		}` |
|      ! 0 |  7739 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7740 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7741 | `	}` |
|        8 |  7742 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7743 | `	return PH7_OK;` |
|        5 |  7744 |  |
|        - |  7745 | `/*` |
|        - |  7746 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7747 | ` */` |
|        6 |  7748 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7749 |  |
|        - |  7750 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7751 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7752 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7753 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7754 | `	return PH7_OK;` |
|        4 |  7755 |  |
|      ! 0 |  7756 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7757 |  |
|        - |  7758 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7759 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7760 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7761 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7762 | `	return PH7_OK;` |
|      ! 0 |  7763 |  |
|        6 |  7764 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7765 |  |
|        - |  7766 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7767 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7768 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7769 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7770 | `	return PH7_OK;` |
|        4 |  7771 |  |
|        6 |  7772 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7773 |  |
|        - |  7774 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7775 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7776 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7777 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7778 | `	return PH7_OK;` |
|        4 |  7779 |  |
|        - |  7780 | `/*` |
|        - |  7781 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7782 | ` */` |
|        4 |  7783 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7784 |  |
|        5 |  7785 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7786 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7787 | `	if( nArg < 1 ){` |
|      ! 0 |  7788 | `		return PH7_OK;` |
|        - |  7789 | `	}` |
|        5 |  7790 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7791 | `	if( pExecCtx ){` |
|        5 |  7792 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7793 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7794 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7795 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7796 | `			SyString sAttrName;` |
|        - |  7797 | `			ph7_value *pAttr;` |
|        5 |  7798 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7799 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7800 | `			if( pAttr ){` |
|        5 |  7801 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7802 | `			}` |
|        2 |  7803 | `		}` |
|        2 |  7804 | `	}` |
|        5 |  7805 | `	return PH7_OK;` |
|        3 |  7806 |  |
|        - |  7807 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7808 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7809 |  |
|        - |  7810 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7811 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7812 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7813 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7814 |  |
|      ! 0 |  7815 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7816 |  |
|        - |  7817 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7818 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7819 | `	ph7_exec_ctx *pCtx;` |
|        - |  7820 | `	ph7_vm_func *pFunc;` |
|        - |  7821 | `	ph7_value *pCallable;` |
|        - |  7822 | `	ph7_value *pCtxAttr;` |
|        - |  7823 | `	SyString sAttrName;` |
|        - |  7824 | `	/* Must not already be started */` |
|      ! 0 |  7825 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7826 | `	if( pCtx != 0 ){` |
|      ! 0 |  7827 | `		return SXERR_INVALID;` |
|        - |  7828 | `	}` |
|      ! 0 |  7829 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7830 | `		return SXERR_INVALID;` |
|        - |  7831 | `	}` |
|      ! 0 |  7832 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7833 | `	/* Get the callable */` |
|      ! 0 |  7834 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7835 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7836 | `	if( pCallable == 0 ){` |
|      ! 0 |  7837 | `		return SXERR_INVALID;` |
|        - |  7838 | `	}` |
|        - |  7839 | `	/* Resolve callable */` |
|      ! 0 |  7840 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7841 | `		SyString sName;` |
|        - |  7842 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7843 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7844 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7845 | `		if( pEntry == 0 ){` |
|      ! 0 |  7846 | `			return SXERR_NOTFOUND;` |
|        - |  7847 | `		}` |
|      ! 0 |  7848 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7849 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7850 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7851 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7852 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7853 | `		if( pMethod == 0 ){` |
|      ! 0 |  7854 | `			return SXERR_INVALID;` |
|        - |  7855 | `		}` |
|      ! 0 |  7856 | `		pClosureThis = pClosure;` |
|      ! 0 |  7857 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7858 | `	}else{` |
|      ! 0 |  7859 | `		return SXERR_INVALID;` |
|        - |  7860 | `	}` |
|        - |  7861 | `	/* Create context */` |
|      ! 0 |  7862 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7863 | `	if( pCtx == 0 ){` |
|      ! 0 |  7864 | `		return SXERR_MEM;` |
|        - |  7865 | `	}` |
|        - |  7866 | `	/* Store in __ctx */` |
|      ! 0 |  7867 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7868 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7869 | `	if( pCtxAttr ){` |
|      ! 0 |  7870 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7871 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7872 | `	}` |
|        - |  7873 | `	/* Set up frame with args */` |
|      ! 0 |  7874 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7875 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7876 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7877 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7878 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7879 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7880 |  |
|      ! 0 |  7881 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7882 |  |
|      ! 0 |  7883 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7884 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7885 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7886 |  |
|      ! 0 |  7887 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7888 |  |
|      ! 0 |  7889 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7890 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7891 |  |
|      ! 0 |  7892 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7893 |  |
|      ! 0 |  7894 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7895 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7896 |  |
|      ! 0 |  7897 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7898 |  |
|      ! 0 |  7899 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7900 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7901 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7902 |  |
|        - |  7903 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7904 | `/*` |
|        - |  7905 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7906 | ` */` |
|       18 |  7907 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7908 |  |
|        - |  7909 | `	ph7_generator *pGen;` |
|       20 |  7910 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7911 | `	if( pGen == 0 ){` |
|      ! 0 |  7912 | `		return 0;` |
|        - |  7913 | `	}` |
|       20 |  7914 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7915 | `	pGen->pCtx = pCtx;` |
|       20 |  7916 | `	pGen->iImplicitKey = 0;` |
|       20 |  7917 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7918 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7919 | `	/* Link the generator back to the exec context */` |
|       20 |  7920 | `	pCtx->pPrivate = pGen;` |
|       20 |  7921 | `	return pGen;` |
|       11 |  7922 |  |
|        - |  7923 | `/*` |
|        - |  7924 | ` * Release a generator and its execution context.` |
|        - |  7925 | ` */` |
|      ! 0 |  7926 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7927 |  |
|      ! 0 |  7928 | `	if( pGen == 0 ){` |
|      ! 0 |  7929 | `		return;` |
|        - |  7930 | `	}` |
|      ! 0 |  7931 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7932 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7933 | `	if( pGen->pCtx ){` |
|      ! 0 |  7934 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7935 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7936 | `		pGen->pCtx = 0;` |
|      ! 0 |  7937 | `	}` |
|      ! 0 |  7938 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7939 |  |
|        - |  7940 | `/*` |
|        - |  7941 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7942 | ` */` |
|      192 |  7943 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7944 |  |
|        - |  7945 | `	ph7_class_instance *pThis;` |
|        - |  7946 | `	SyString sAttr;` |
|        - |  7947 | `	ph7_value *pAttr;` |
|      194 |  7948 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7949 | `		return 0;` |
|        - |  7950 | `	}` |
|      194 |  7951 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7952 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7953 | `		return 0;` |
|        - |  7954 | `	}` |
|      194 |  7955 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7956 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7957 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7958 | `		return 0;` |
|        - |  7959 | `	}` |
|      194 |  7960 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7961 |  |
|        - |  7962 | `/*` |
|        - |  7963 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7964 | ` */` |
|       18 |  7965 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7966 |  |
|        - |  7967 | `	ph7_generator *pGen;` |
|        - |  7968 | `	sxi32 rc;` |
|       20 |  7969 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7970 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7971 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7972 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7973 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7974 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7975 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7976 | `	}` |
|       20 |  7977 | `	return PH7_OK;` |
|       11 |  7978 |  |
|        - |  7979 | `/*` |
|        - |  7980 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7981 | ` */` |
|       52 |  7982 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7983 |  |
|        - |  7984 | `	ph7_generator *pGen;` |
|       54 |  7985 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7986 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7987 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7988 | `	return PH7_OK;` |
|       28 |  7989 |  |
|        - |  7990 | `/*` |
|        - |  7991 | ` * Generator::current() — return the last yielded value.` |
|        - |  7992 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7993 | ` */` |
|       56 |  7994 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7995 |  |
|        - |  7996 | `	ph7_generator *pGen;` |
|        - |  7997 | `	sxi32 rc;` |
|       58 |  7998 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7999 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  8000 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8001 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8002 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8003 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8004 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8005 | `	}` |
|       58 |  8006 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  8007 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  8008 | `	}else{` |
|      ! 0 |  8009 | `		ph7_result_null(pCtx);` |
|        - |  8010 | `	}` |
|       58 |  8011 | `	return PH7_OK;` |
|       30 |  8012 |  |
|        - |  8013 | `/*` |
|        - |  8014 | ` * Generator::key() — return the last yielded key.` |
|        - |  8015 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8016 | ` */` |
|       12 |  8017 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8018 |  |
|        - |  8019 | `	ph7_generator *pGen;` |
|        - |  8020 | `	sxi32 rc;` |
|       13 |  8021 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8022 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  8023 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8024 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8025 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8026 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8027 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8028 | `	}` |
|       13 |  8029 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  8030 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  8031 | `	}else{` |
|      ! 0 |  8032 | `		ph7_result_null(pCtx);` |
|        - |  8033 | `	}` |
|       13 |  8034 | `	return PH7_OK;` |
|        7 |  8035 |  |
|        - |  8036 | `/*` |
|        - |  8037 | ` * Generator::next() — advance to the next yield point.` |
|        - |  8038 | ` */` |
|       48 |  8039 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8040 |  |
|        - |  8041 | `	ph7_generator *pGen;` |
|        - |  8042 | `	sxi32 rc;` |
|       50 |  8043 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  8044 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  8045 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  8046 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8047 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  8048 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  8049 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  8050 | `	}else{` |
|      ! 0 |  8051 | `		return PH7_OK;` |
|        - |  8052 | `	}` |
|       50 |  8053 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  8054 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  8055 | `	return PH7_OK;` |
|       26 |  8056 |  |
|        - |  8057 | `/*` |
|        - |  8058 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  8059 | ` */` |
|        4 |  8060 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8061 |  |
|        - |  8062 | `	ph7_generator *pGen;` |
|        - |  8063 | `	ph7_value *pSendVal;` |
|        - |  8064 | `	sxi32 rc;` |
|        5 |  8065 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  8066 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  8067 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  8068 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  8069 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  8070 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  8071 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  8072 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  8073 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  8074 | `	}else{` |
|      ! 0 |  8075 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8076 | `		return PH7_OK;` |
|        - |  8077 | `	}` |
|        5 |  8078 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  8079 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  8080 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8081 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  8082 | `	}else{` |
|        3 |  8083 | `		ph7_result_null(pCtx);` |
|        - |  8084 | `	}` |
|        5 |  8085 | `	return PH7_OK;` |
|        3 |  8086 |  |
|        - |  8087 | `/*` |
|        - |  8088 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  8089 | ` *` |
|        - |  8090 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  8091 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  8092 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  8093 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  8094 | ` * the exception to the caller.` |
|        - |  8095 | ` */` |
|      ! 0 |  8096 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8097 |  |
|        - |  8098 | `	ph7_generator *pGen;` |
|        - |  8099 | `	const char *zMsg;` |
|        - |  8100 | `	int nLen;` |
|      ! 0 |  8101 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  8102 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8103 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  8104 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  8105 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  8106 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8107 | `			"Cannot throw into a closed generator");` |
|        - |  8108 | `	}` |
|        - |  8109 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  8110 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  8111 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  8112 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  8113 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8114 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  8115 | `	nLen = 0;` |
|      ! 0 |  8116 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  8117 | `		/* Try to get the exception's message */` |
|        - |  8118 | `		SyString sAttr;` |
|        - |  8119 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  8120 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  8121 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  8122 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  8123 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  8124 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  8125 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  8126 | `		}` |
|      ! 0 |  8127 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  8128 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  8129 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  8130 | `	}` |
|      ! 0 |  8131 | `	(void)nLen;` |
|      ! 0 |  8132 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  8133 |  |
|        - |  8134 | `/*` |
|        - |  8135 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  8136 | ` */` |
|        2 |  8137 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8138 |  |
|        - |  8139 | `	ph7_generator *pGen;` |
|        3 |  8140 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8141 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  8142 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8143 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8144 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8145 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  8146 | `	}` |
|        3 |  8147 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  8148 | `	return PH7_OK;` |
|        2 |  8149 |  |
|        - |  8150 | `/*` |
|        - |  8151 | ` * Generator::__destruct() — clean up.` |
|        - |  8152 | ` */` |
|      ! 0 |  8153 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8154 |  |
|        - |  8155 | `	ph7_generator *pGen;` |
|      ! 0 |  8156 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  8157 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8158 | `	if( pGen ){` |
|      ! 0 |  8159 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8160 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8161 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8162 | `			SyString sAttrName;` |
|        - |  8163 | `			ph7_value *pAttr;` |
|      ! 0 |  8164 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8165 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8166 | `			if( pAttr ){` |
|      ! 0 |  8167 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8168 | `			}` |
|      ! 0 |  8169 | `		}` |
|      ! 0 |  8170 | `	}` |
|      ! 0 |  8171 | `	return PH7_OK;` |
|      ! 0 |  8172 |  |
|        - |  8173 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8174 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8175 | `/*` |
|        - |  8176 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8177 | ` * the desired message.` |
|        - |  8178 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8179 | ` * in 'api.c' for additional information.` |
|        - |  8180 | ` */` |
|      370 |  8181 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8182 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8183 | `	SyString *pString /* Message to output */` |
|        - |  8184 | `	)` |
|        2 |  8185 |  |
|      372 |  8186 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8187 | `	sxi32 rc = SXRET_OK;` |
|        - |  8188 | `	/* Call the output consumer */` |
|      372 |  8189 | `	if( pString->nByte > 0 ){` |
|      372 |  8190 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8191 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8192 | `	}` |
|      372 |  8193 | `	return rc;` |
|        2 |  8194 |  |
|        - |  8195 | `/*` |
|        - |  8196 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8197 | ` * callback to consume the formatted message.` |
|        - |  8198 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8199 | ` * in 'api.c' for additional information.` |
|        - |  8200 | ` */` |
|        2 |  8201 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8202 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8203 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8204 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8205 | `	)` |
|        1 |  8206 |  |
|        3 |  8207 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8208 | `	sxi32 rc = SXRET_OK;` |
|        - |  8209 | `	SyBlob sWorker;` |
|        - |  8210 | `	/* Format the message and call the output consumer */` |
|        3 |  8211 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8212 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8213 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8214 | `		/* Consume the formatted message */` |
|        3 |  8215 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8216 | `	}` |
|        3 |  8217 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8218 | `	/* Release the working buffer */` |
|        3 |  8219 | `	SyBlobRelease(&sWorker);` |
|        3 |  8220 | `	return rc;` |
|        1 |  8221 |  |
|        - |  8222 | `/*` |
|        - |  8223 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8224 | ` * This function never fail and always return a pointer` |
|        - |  8225 | ` * to a null terminated string.` |
|        - |  8226 | ` */` |
|       12 |  8227 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8228 |  |
|       13 |  8229 | `	const char *zOp = "Unknown     ";` |
|       13 |  8230 | `	switch(nOp){` |
|        3 |  8231 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8232 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8233 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8234 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8235 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8236 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8237 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8238 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8239 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8240 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8241 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8242 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8243 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8244 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8245 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8246 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8247 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8248 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8249 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8250 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8251 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8252 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8253 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8254 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8255 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8256 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8257 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8258 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8259 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8260 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8261 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8262 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8263 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8264 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8265 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8266 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8267 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8268 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8269 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8270 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8271 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8272 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8273 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8274 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8275 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8276 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8277 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8278 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8279 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8280 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8281 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8282 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8283 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8284 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8285 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8286 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8287 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8288 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8289 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8290 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8291 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8292 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8293 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8294 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8295 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8296 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8297 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8298 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8299 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8300 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8301 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8302 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8303 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8304 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8305 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8306 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8307 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8308 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8309 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8310 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8311 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8312 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8313 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8314 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8315 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8316 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8317 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8318 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8319 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8320 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8321 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8322 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8323 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8324 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8325 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8326 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8327 | `	default:` |
|      ! 0 |  8328 | `		break;` |
|        - |  8329 | `	}` |
|       13 |  8330 | `	return zOp;` |
|        1 |  8331 |  |
|        - |  8332 | `/*` |
|        - |  8333 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8334 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8335 | ` * is responsible of consuming the generated dump.` |
|        - |  8336 | ` */` |
|        2 |  8337 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8338 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8339 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8340 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8341 | `	)` |
|        1 |  8342 |  |
|        - |  8343 | `	sxi32 rc;` |
|        3 |  8344 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8345 | `	return rc;` |
|        1 |  8346 |  |
|        - |  8347 | `/*` |
|        - |  8348 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8349 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8350 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8351 | ` * in 'compile.c' for additional information.` |
|        - |  8352 | ` */` |
|       14 |  8353 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8354 |  |
|       15 |  8355 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8356 | `	/* Evaluate and expand constant value */` |
|       15 |  8357 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8358 |  |
|        - |  8359 | `/*` |
|        - |  8360 | ` * Section:` |
|        - |  8361 | ` *  Function handling functions.` |
|        - |  8362 | ` * Status:` |
|        - |  8363 | ` *    Stable.` |
|        - |  8364 | ` */` |
|        - |  8365 | `/*` |
|        - |  8366 | ` * int func_num_args(void)` |
|        - |  8367 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8368 | ` * Parameters` |
|        - |  8369 | ` *   None.` |
|        - |  8370 | ` * Return` |
|        - |  8371 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8372 | ` *  or -1 if called from the globe scope.` |
|        - |  8373 | ` */` |
|      944 |  8374 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8375 |  |
|        - |  8376 | `	VmFrame *pFrame;` |
|        - |  8377 | `	ph7_vm *pVm;` |
|        - |  8378 | `	/* Point to the target VM */` |
|      946 |  8379 | `	pVm = pCtx->pVm;` |
|        - |  8380 | `	/* Current frame */` |
|      946 |  8381 | `	pFrame = pVm->pFrame;` |
|      946 |  8382 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  8383 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8384 | `		SXUNUSED(nArg);` |
|      ! 0 |  8385 | `		SXUNUSED(apArg);` |
|        - |  8386 | `		/* Global frame,return -1 */` |
|      ! 0 |  8387 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8388 | `		return SXRET_OK;` |
|        - |  8389 | `	}` |
|        - |  8390 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  8391 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  8392 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  8393 | `	return SXRET_OK;` |
|      474 |  8394 |  |
|        - |  8395 | `/*` |
|        - |  8396 | ` * value func_get_arg(int $arg_num)` |
|        - |  8397 | ` *   Return an item from the argument list.` |
|        - |  8398 | ` * Parameters` |
|        - |  8399 | ` *  Argument number(index start from zero).` |
|        - |  8400 | ` * Return` |
|        - |  8401 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8402 | ` */` |
|       22 |  8403 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8404 |  |
|       24 |  8405 | `	ph7_value *pObj = 0;` |
|       24 |  8406 | `	VmSlot *pSlot = 0;` |
|        - |  8407 | `	VmFrame *pFrame;` |
|        - |  8408 | `	ph7_vm *pVm;` |
|        - |  8409 | `	/* Point to the target VM */` |
|       24 |  8410 | `	pVm = pCtx->pVm;` |
|        - |  8411 | `	/* Current frame */` |
|       24 |  8412 | `	pFrame = pVm->pFrame;` |
|       24 |  8413 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8414 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8415 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8416 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8417 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8418 | `		return SXRET_OK;` |
|        - |  8419 | `	}` |
|        - |  8420 | `	/* Extract the desired index */` |
|       21 |  8421 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8422 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8423 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8424 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8425 | `		return SXRET_OK;` |
|        - |  8426 | `	}` |
|        - |  8427 | `	/* Extract the desired argument */` |
|       21 |  8428 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8429 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8430 | `			/* Return the desired argument */` |
|       21 |  8431 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8432 | `		}else{` |
|        - |  8433 | `			/* No such argument,return false */` |
|      ! 0 |  8434 | `			ph7_result_bool(pCtx,0);` |
|        - |  8435 | `		}` |
|       11 |  8436 | `	}else{` |
|        - |  8437 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8438 | `		ph7_result_bool(pCtx,0);` |
|        - |  8439 | `	}` |
|       21 |  8440 | `	return SXRET_OK;` |
|       13 |  8441 |  |
|        - |  8442 | `/*` |
|        - |  8443 | ` * array func_get_args_byref(void)` |
|        - |  8444 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8445 | ` * Parameters` |
|        - |  8446 | ` *  None.` |
|        - |  8447 | ` * Return` |
|        - |  8448 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8449 | ` *  member of the current user-defined function's argument list.` |
|        - |  8450 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8451 | ` * NOTE:` |
|        - |  8452 | ` *  Arguments are returned to the array by reference.` |
|        - |  8453 | ` */` |
|        2 |  8454 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8455 |  |
|        - |  8456 | `	ph7_value *pArray;` |
|        - |  8457 | `	VmFrame *pFrame;` |
|        - |  8458 | `	VmSlot *aSlot;` |
|        - |  8459 | `	sxu32 n;` |
|        - |  8460 | `	/* Point to the current frame */` |
|        3 |  8461 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8462 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8463 | `	if( pFrame->pParent == 0 ){` |
|        - |  8464 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8465 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8466 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8467 | `		return SXRET_OK;` |
|        - |  8468 | `	}` |
|        - |  8469 | `	/* Create a new array */` |
|        3 |  8470 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8471 | `	if( pArray == 0 ){` |
|      ! 0 |  8472 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8473 | `		SXUNUSED(apArg);` |
|      ! 0 |  8474 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8475 | `		return SXRET_OK;` |
|        - |  8476 | `	}` |
|        - |  8477 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8478 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8479 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8480 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8481 | `	}` |
|        - |  8482 | `	/* Return the freshly created array */` |
|        3 |  8483 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8484 | `	return SXRET_OK;` |
|        2 |  8485 |  |
|        - |  8486 | `/*` |
|        - |  8487 | ` * array func_get_args(void)` |
|        - |  8488 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8489 | ` * Parameters` |
|        - |  8490 | ` *  None.` |
|        - |  8491 | ` * Return` |
|        - |  8492 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8493 | ` *  member of the current user-defined function's argument list.` |
|        - |  8494 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8495 | ` */` |
|       88 |  8496 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8497 |  |
|       90 |  8498 | `	ph7_value *pObj = 0;` |
|        - |  8499 | `	ph7_value *pArray;` |
|        - |  8500 | `	VmFrame *pFrame;` |
|        - |  8501 | `	VmSlot *aSlot;` |
|        - |  8502 | `	sxu32 n;` |
|        - |  8503 | `	/* Point to the current frame */` |
|       90 |  8504 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8505 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8506 | `	if( pFrame->pParent == 0 ){` |
|        - |  8507 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8508 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8509 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8510 | `		return SXRET_OK;` |
|        - |  8511 | `	}` |
|        - |  8512 | `	/* Create a new array */` |
|       90 |  8513 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8514 | `	if( pArray == 0 ){` |
|      ! 0 |  8515 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8516 | `		SXUNUSED(apArg);` |
|      ! 0 |  8517 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8518 | `		return SXRET_OK;` |
|        - |  8519 | `	}` |
|        - |  8520 | `	/* Start filling the array with the given arguments */` |
|       90 |  8521 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8522 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8523 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8524 | `		if( pObj ){` |
|      134 |  8525 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8526 | `		}` |
|       68 |  8527 | `	}` |
|        - |  8528 | `	/* Return the freshly created array */` |
|       90 |  8529 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8530 | `	return SXRET_OK;` |
|       46 |  8531 |  |
|        - |  8532 | `/*` |
|        - |  8533 | ` * bool function_exists(string $name)` |
|        - |  8534 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8535 | ` * Parameters` |
|        - |  8536 | ` *  The name of the desired function.` |
|        - |  8537 | ` * Return` |
|        - |  8538 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8539 | ` */` |
|     1684 |  8540 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8541 |  |
|        - |  8542 | `	const char *zName;` |
|        - |  8543 | `	ph7_vm *pVm;` |
|        - |  8544 | `	int nLen;` |
|        - |  8545 | `	int res;` |
|     1686 |  8546 | `	if( nArg < 1 ){` |
|        - |  8547 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8548 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8549 | `		return SXRET_OK;` |
|        - |  8550 | `	}` |
|        - |  8551 | `	/* Point to the target VM */` |
|     1686 |  8552 | `	pVm = pCtx->pVm;` |
|        - |  8553 | `	/* Extract the function name */` |
|     1686 |  8554 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8555 | `	/* Assume the function is not defined */` |
|     1686 |  8556 | `	res = 0;` |
|        - |  8557 | `	/* Perform the lookup */` |
|     2526 |  8558 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1680 |  8559 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8560 | `			/* Function is defined */` |
|      206 |  8561 | `			res = 1;` |
|      102 |  8562 | `	}` |
|     1686 |  8563 | `	ph7_result_bool(pCtx,res);` |
|     1686 |  8564 | `	return SXRET_OK;` |
|      844 |  8565 |  |
|        - |  8566 | `/*` |
|        - |  8567 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8568 | ` * [i.e: Whether it is callable or not].` |
|        - |  8569 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8570 | ` */` |
|    18260 |  8571 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8572 |  |
|    18262 |  8573 | `	int res = 0;` |
|    18262 |  8574 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8575 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8576 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8577 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8578 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8579 | `		if( pMethod && CallInvoke ){` |
|        - |  8580 | `			ph7_value sResult;` |
|        - |  8581 | `			sxi32 rc;` |
|        - |  8582 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8583 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8584 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8585 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8586 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8587 | `			}` |
|      ! 0 |  8588 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8589 | `		}` |
|    18262 |  8590 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8591 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8592 | `		if( pMap->nEntry == 2 ){` |
|        - |  8593 | `			ph7_class *pClass;` |
|        - |  8594 | `			ph7_value *pV;` |
|        - |  8595 | `			/* Extract the target class */` |
|       12 |  8596 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8597 | `			if( pV ){` |
|       12 |  8598 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8599 | `				if( pClass ){` |
|        - |  8600 | `					ph7_class_method *pMethod;` |
|        - |  8601 | `					/* Extract the target method */` |
|       10 |  8602 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8603 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8604 | `						/* Perform the lookup */` |
|       10 |  8605 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8606 | `						if( pMethod ){` |
|        - |  8607 | `							/* Method is callable */` |
|        5 |  8608 | `							res = 1;` |
|        2 |  8609 | `						}` |
|        4 |  8610 | `					}` |
|        4 |  8611 | `				}` |
|        5 |  8612 | `			}` |
|        7 |  8613 | `		}` |
|    18249 |  8614 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8615 | `		const char *zName;` |
|        - |  8616 | `		int nLen;` |
|        - |  8617 | `		/* Extract the name */` |
|     5104 |  8618 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8619 | `		/* Perform the lookup */` |
|     5119 |  8620 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8621 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8622 | `				/* Function is callable */` |
|     5086 |  8623 | `				res = 1;` |
|     2542 |  8624 | `		}` |
|     2551 |  8625 | `	}` |
|    18262 |  8626 | `	return res;` |
|        2 |  8627 |  |
|        - |  8628 | `/*` |
|        - |  8629 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8630 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8631 | ` * Parameters` |
|        - |  8632 | ` * $name` |
|        - |  8633 | ` *    The callback function to check` |
|        - |  8634 | ` * $syntax_only` |
|        - |  8635 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8636 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8637 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8638 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8639 | ` *    a string.` |
|        - |  8640 | ` * Return` |
|        - |  8641 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8642 | ` */` |
|       14 |  8643 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8644 |  |
|        - |  8645 | `	ph7_vm *pVm;` |
|        - |  8646 | `	int res;` |
|       15 |  8647 | `	if( nArg < 1 ){` |
|        - |  8648 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8649 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8650 | `		return SXRET_OK;` |
|        - |  8651 | `	}` |
|        - |  8652 | `	/* Point to the target VM */` |
|       15 |  8653 | `	pVm = pCtx->pVm;` |
|        - |  8654 | `	/* Perform the requested operation */` |
|       15 |  8655 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8656 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8657 | `	return SXRET_OK;` |
|        8 |  8658 |  |
|        - |  8659 | `/*` |
|        - |  8660 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8661 | ` * defined below.` |
|        - |  8662 | ` */` |
|     1200 |  8663 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8664 |  |
|     1201 |  8665 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8666 | `	ph7_value sName;` |
|        - |  8667 | `	sxi32 rc;` |
|        - |  8668 | `	/* Prepare the function name for insertion */` |
|     1201 |  8669 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  8670 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8671 | `	/* Perform the insertion */` |
|     1201 |  8672 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  8673 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  8674 | `	return rc;` |
|        1 |  8675 |  |
|        - |  8676 | `/*` |
|        - |  8677 | ` * array get_defined_functions(void)` |
|        - |  8678 | ` *  Returns an array of all defined functions.` |
|        - |  8679 | ` * Parameter` |
|        - |  8680 | ` *  None.` |
|        - |  8681 | ` * Return` |
|        - |  8682 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8683 | ` *  both built-in (internal) and user-defined.` |
|        - |  8684 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8685 | ` *  defined ones using $arr["user"].` |
|        - |  8686 | ` * Note:` |
|        - |  8687 | ` *  NULL is returned on failure.` |
|        - |  8688 | ` */` |
|        2 |  8689 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8690 |  |
|        - |  8691 | `	ph7_value *pArray,*pEntry;` |
|        - |  8692 | `	/* NOTE:` |
|        - |  8693 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8694 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8695 | `	 */` |
|        3 |  8696 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8697 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8698 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8699 | `		SXUNUSED(apArg);` |
|        - |  8700 | `		/* Return NULL */` |
|      ! 0 |  8701 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8702 | `		return SXRET_OK;` |
|        - |  8703 | `	}` |
|        3 |  8704 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8705 | `	if( pEntry == 0 ){` |
|        - |  8706 | `		/* Return NULL */` |
|      ! 0 |  8707 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8708 | `		return SXRET_OK;` |
|        - |  8709 | `	}` |
|        - |  8710 | `	/* Fill with the appropriate information */` |
|        3 |  8711 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8712 | `	/* Create the 'internal' index */` |
|        3 |  8713 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8714 | `	/* Create the user-func array */` |
|        3 |  8715 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8716 | `	if( pEntry == 0 ){` |
|        - |  8717 | `		/* Return NULL */` |
|      ! 0 |  8718 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8719 | `		return SXRET_OK;` |
|        - |  8720 | `	}` |
|        - |  8721 | `	/* Fill with the appropriate information */` |
|        3 |  8722 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8723 | `	/* Create the 'user' index */` |
|        3 |  8724 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8725 | `	/* Return the multi-dimensional array */` |
|        3 |  8726 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8727 | `	return SXRET_OK;` |
|        2 |  8728 |  |
|        - |  8729 | `/*` |
|        - |  8730 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8731 | ` *  Register a function for execution on shutdown.` |
|        - |  8732 | ` * Note` |
|        - |  8733 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8734 | ` *  be called in the same order as they were registered.` |
|        - |  8735 | ` * Parameters` |
|        - |  8736 | ` *  $callback` |
|        - |  8737 | ` *   The shutdown callback to register.` |
|        - |  8738 | ` * $param` |
|        - |  8739 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8740 | ` * Return` |
|        - |  8741 | ` *  Nothing.` |
|        - |  8742 | ` */` |
|        2 |  8743 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8744 |  |
|        - |  8745 | `	VmShutdownCB sEntry;` |
|        - |  8746 | `	int i,j;` |
|        3 |  8747 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8748 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8749 | `		return PH7_OK;` |
|        - |  8750 | `	}` |
|        - |  8751 | `	/* Zero the Entry */` |
|        3 |  8752 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8753 | `	/* Initialize fields */` |
|        3 |  8754 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8755 | `	/* Save the callback name for later invocation name */` |
|        3 |  8756 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8757 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8758 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8759 | `	}` |
|        - |  8760 | `	/* Copy arguments */` |
|        3 |  8761 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8762 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8763 | `			/* Limit reached */` |
|      ! 0 |  8764 | `			break;` |
|        - |  8765 | `		}` |
|      ! 0 |  8766 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8767 | `	}` |
|        3 |  8768 | `	sEntry.nArg = j;` |
|        - |  8769 | `	/* Install the callback */` |
|        3 |  8770 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8771 | `	return PH7_OK;` |
|        2 |  8772 |  |
|        - |  8773 | `/*` |
|        - |  8774 | ` * Section:` |
|        - |  8775 | ` *  Class handling functions.` |
|        - |  8776 | ` * Status:` |
|        - |  8777 | ` *    Stable.` |
|        - |  8778 | ` */` |
|        - |  8779 | `/*` |
|        - |  8780 | ` * Extract the top active class. NULL is returned` |
|        - |  8781 | ` * if the class stack is empty.` |
|        - |  8782 | ` */` |
|      612 |  8783 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8784 |  |
|      614 |  8785 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8786 | `	ph7_class **apClass;` |
|      614 |  8787 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8788 | `		/* Empty stack,return NULL */` |
|       15 |  8789 | `		return 0;` |
|        - |  8790 | `	}` |
|        - |  8791 | `	/* Peek the last entry */` |
|      600 |  8792 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      600 |  8793 | `	return apClass[pSet->nUsed - 1];` |
|      308 |  8794 |  |
|        - |  8795 | `/*` |
|        - |  8796 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8797 | ` *   Get the class that declared the currently executing method.` |
|        - |  8798 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8799 | ` *` |
|        - |  8800 | ` * Parameters` |
|        - |  8801 | ` *   pVm: Target VM` |
|        - |  8802 | ` *` |
|        - |  8803 | ` * Return` |
|        - |  8804 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8805 | ` *   - Not executing within a class method` |
|        - |  8806 | ` *` |
|        - |  8807 | ` * Note` |
|        - |  8808 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8809 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8810 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8811 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8812 | ` *   declaring class.` |
|        - |  8813 | ` */` |
|       90 |  8814 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8815 |  |
|       92 |  8816 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8817 | `	ph7_vm_func *pVmFunc;` |
|        - |  8818 |  |
|        - |  8819 | `	/* Skip exception frames to find the actual method frame */` |
|       92 |  8820 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8821 |  |
|        - |  8822 | `	/* Check if we're in a method context */` |
|       92 |  8823 | `	if( pFrame->pParent ){` |
|       88 |  8824 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       88 |  8825 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8826 | `			/* Return the declaring class */` |
|       88 |  8827 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8828 | `		}` |
|      ! 0 |  8829 | `	}` |
|        - |  8830 |  |
|        5 |  8831 | `	return 0;` |
|       47 |  8832 |  |
|        - |  8833 |  |
|        - |  8834 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8835 | `/*` |
|        - |  8836 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8837 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8838 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8839 | ` * return value indicates failure.` |
|        - |  8840 | ` */` |
|     1542 |  8841 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8842 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8843 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8844 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8845 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8846 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8847 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8848 | `	)` |
|        2 |  8849 |  |
|        - |  8850 | `	ph7_value *aStack;` |
|        - |  8851 | `	VmInstr aInstr[2];` |
|        - |  8852 | `	int iCursor;` |
|        - |  8853 | `	int i;` |
|        - |  8854 | `	/* Create a new operand stack */` |
|     1544 |  8855 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1544 |  8856 | `	if( aStack == 0 ){` |
|      ! 0 |  8857 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8858 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8859 | `		return SXERR_MEM;` |
|        - |  8860 | `	}` |
|        - |  8861 | `	/* Fill the operand stack with the given arguments */` |
|     2194 |  8862 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      652 |  8863 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8864 | `		/*` |
|        - |  8865 | `		 * Symisc eXtension:` |
|        - |  8866 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8867 | `		 */` |
|      652 |  8868 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      327 |  8869 | `	}` |
|     1544 |  8870 | `	iCursor = nArg + 1;` |
|     1544 |  8871 | `	if( pThis ){` |
|        - |  8872 | `		/*` |
|        - |  8873 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8874 | `		 */` |
|     1538 |  8875 | `		pThis->iRef++; /* Increment reference count */` |
|     1538 |  8876 | `		aStack[i].x.pOther = pThis;` |
|     1538 |  8877 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      768 |  8878 | `	}` |
|     1544 |  8879 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1544 |  8880 | `	i++;` |
|        - |  8881 | `	/* Push method name */` |
|     1544 |  8882 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1544 |  8883 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1544 |  8884 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1544 |  8885 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8886 | `	/* Emit the CALL istruction */` |
|     1544 |  8887 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1544 |  8888 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1544 |  8889 | `	aInstr[0].iP2 = 0;` |
|     1544 |  8890 | `	aInstr[0].p3  = 0;` |
|        - |  8891 | `	/* Emit the DONE instruction */` |
|     1544 |  8892 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1544 |  8893 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1544 |  8894 | `	aInstr[1].iP2 = 0;` |
|     1544 |  8895 | `	aInstr[1].p3  = 0;` |
|        - |  8896 | `	/* Execute the method body (if available) */` |
|     1544 |  8897 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8898 | `	/* Clean up the mess left behind */` |
|     1544 |  8899 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1544 |  8900 | `	return PH7_OK;` |
|      773 |  8901 |  |
|        - |  8902 | `/*` |
|        - |  8903 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8904 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8905 | ` * in the apArg[] array.` |
|        - |  8906 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8907 | ` * return value indicates failure.` |
|        - |  8908 | ` */` |
|      960 |  8909 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8910 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8911 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8912 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8913 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8914 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8915 | `	)` |
|        2 |  8916 |  |
|        - |  8917 | `	ph7_value *aStack;` |
|        - |  8918 | `	VmInstr aInstr[2];` |
|        - |  8919 | `	int i;` |
|      962 |  8920 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8921 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  8922 | `		if( pResult ){` |
|        - |  8923 | `			/* Assume a null return value */` |
|      ! 0 |  8924 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8925 | `		}` |
|      479 |  8926 | `		return SXERR_INVALID;` |
|        - |  8927 | `	}` |
|      484 |  8928 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8929 | `		/* Class method */` |
|       11 |  8930 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8931 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8932 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8933 | `		ph7_class *pClass = 0;` |
|        - |  8934 | `		ph7_value *pValue;` |
|        - |  8935 | `		sxi32 rc;` |
|       11 |  8936 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8937 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8938 | `			if( pResult ){` |
|        - |  8939 | `				/* Assume a null return value */` |
|      ! 0 |  8940 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8941 | `			}` |
|      ! 0 |  8942 | `			return SXRET_OK;` |
|        - |  8943 | `		}` |
|        - |  8944 | `		/* Extract the class name or an instance of it */` |
|       11 |  8945 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8946 | `		if( pValue ){` |
|       11 |  8947 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8948 | `		}` |
|       11 |  8949 | `		if( pClass == 0 ){` |
|        - |  8950 | `			/* No such class,return NULL */` |
|      ! 0 |  8951 | `			if( pResult ){` |
|      ! 0 |  8952 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8953 | `			}` |
|      ! 0 |  8954 | `			return SXRET_OK;` |
|        - |  8955 | `		}` |
|       11 |  8956 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8957 | `			/* Point to the class instance */` |
|        5 |  8958 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8959 | `		}` |
|        - |  8960 | `		/* Try to extract the method */` |
|       11 |  8961 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8962 | `		if( pValue ){` |
|       11 |  8963 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8964 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8965 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8966 | `			}` |
|        5 |  8967 | `		}` |
|       11 |  8968 | `		if( pMethod == 0 ){` |
|        - |  8969 | `			/* No such method,return NULL */` |
|      ! 0 |  8970 | `			if( pResult ){` |
|      ! 0 |  8971 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8972 | `			}` |
|      ! 0 |  8973 | `			return SXRET_OK;` |
|        - |  8974 | `		}` |
|        - |  8975 | `		/* Call the class method */` |
|       11 |  8976 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8977 | `		return rc;` |
|        - |  8978 | `	}` |
|        - |  8979 | `	/* Create a new operand stack */` |
|      474 |  8980 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8981 | `	if( aStack == 0 ){` |
|      ! 0 |  8982 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8983 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8984 | `		if( pResult ){` |
|        - |  8985 | `			/* Assume a null return value */` |
|      ! 0 |  8986 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8987 | `		}` |
|      ! 0 |  8988 | `		return SXERR_MEM;` |
|        - |  8989 | `	}` |
|        - |  8990 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8991 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8992 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8993 | `		/*` |
|        - |  8994 | `		 * Symisc eXtension:` |
|        - |  8995 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8996 | `		 */` |
|     1050 |  8997 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8998 | `	}` |
|        - |  8999 | `	/* Push the function name */` |
|      474 |  9000 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  9001 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  9002 | `	/* Emit the CALL istruction */` |
|      474 |  9003 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  9004 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  9005 | `	aInstr[0].iP2 = 0;` |
|      474 |  9006 | `	aInstr[0].p3  = 0;` |
|        - |  9007 | `	/* Emit the DONE instruction */` |
|      474 |  9008 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  9009 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  9010 | `	aInstr[1].iP2 = 0;` |
|      474 |  9011 | `	aInstr[1].p3  = 0;` |
|        - |  9012 | `	/* Execute the function body (if available) */` |
|      474 |  9013 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  9014 | `	/* Clean up the mess left behind */` |
|      474 |  9015 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  9016 | `	return PH7_OK;` |
|      482 |  9017 |  |
|        - |  9018 | `/*` |
|        - |  9019 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  9020 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  9021 | ` * parameter.` |
|        - |  9022 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  9023 | ` * return value indicates failure.` |
|        - |  9024 | ` */` |
|      236 |  9025 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  9026 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  9027 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  9028 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  9029 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  9030 | `	)` |
|        1 |  9031 |  |
|        - |  9032 | `	ph7_value *pArg;` |
|        - |  9033 | `	SySet aArg;` |
|        - |  9034 | `	va_list ap;` |
|        - |  9035 | `	sxi32 rc;` |
|      237 |  9036 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  9037 | `	/* Copy arguments one after one */` |
|      237 |  9038 | `	va_start(ap,pResult);` |
|      393 |  9039 | `	for(;;){` |
|      787 |  9040 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  9041 | `		if( pArg == 0 ){` |
|      237 |  9042 | `			break;` |
|        - |  9043 | `		}` |
|      551 |  9044 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  9045 | `	}` |
|        - |  9046 | `	/* Call the core routine */` |
|      237 |  9047 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  9048 | `	/* Cleanup */` |
|      237 |  9049 | `	SySetRelease(&aArg);` |
|      237 |  9050 | `	return rc;` |
|        1 |  9051 |  |
|        - |  9052 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  9053 | `/*` |
|        - |  9054 | ` * bool defined(string $name)` |
|        - |  9055 | ` *  Checks whether a given named constant exists.` |
|        - |  9056 | ` * Parameter:` |
|        - |  9057 | ` *  Name of the desired constant.` |
|        - |  9058 | ` * Return` |
|        - |  9059 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  9060 | ` */` |
|       14 |  9061 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9062 |  |
|        - |  9063 | `	const char *zName;` |
|       16 |  9064 | `	int nLen = 0;` |
|       16 |  9065 | `	int res = 0;` |
|       16 |  9066 | `	if( nArg < 1 ){` |
|        - |  9067 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  9068 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  9069 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9070 | `		return SXRET_OK;` |
|        - |  9071 | `	}` |
|        - |  9072 | `	/* Extract constant name */` |
|       16 |  9073 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9074 | `	/* Perform the lookup */` |
|       16 |  9075 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9076 | `		/* Already defined */` |
|       10 |  9077 | `		res = 1;` |
|        4 |  9078 | `	}` |
|       16 |  9079 | `	ph7_result_bool(pCtx,res);` |
|       16 |  9080 | `	return SXRET_OK;` |
|        9 |  9081 |  |
|        - |  9082 | `/*` |
|        - |  9083 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  9084 | ` * below.` |
|        - |  9085 | ` */` |
|       10 |  9086 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  9087 |  |
|       12 |  9088 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  9089 | `	/* Expand constant value */` |
|       12 |  9090 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  9091 |  |
|        - |  9092 | `/*` |
|        - |  9093 | ` * bool define(string $constant_name,expression value)` |
|        - |  9094 | ` *  Defines a named constant at runtime.` |
|        - |  9095 | ` * Parameter:` |
|        - |  9096 | ` *  $constant_name` |
|        - |  9097 | ` *   The name of the constant` |
|        - |  9098 | ` *  $value` |
|        - |  9099 | ` *   Constant value` |
|        - |  9100 | ` * Return:` |
|        - |  9101 | ` *   TRUE on success,FALSE on failure.` |
|        - |  9102 | ` */` |
|       12 |  9103 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9104 |  |
|        - |  9105 | `	const char *zName;  /* Constant name */` |
|        - |  9106 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  9107 | `	int nLen = 0;       /* Name length */` |
|        - |  9108 | `	sxi32 rc;` |
|       14 |  9109 | `	if( nArg < 2 ){` |
|        - |  9110 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  9111 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  9112 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9113 | `		return SXRET_OK;` |
|        - |  9114 | `	}` |
|       14 |  9115 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  9116 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  9117 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9118 | `		return SXRET_OK;` |
|        - |  9119 | `	}` |
|        - |  9120 | `	/* Extract constant name */` |
|       14 |  9121 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  9122 | `	if( nLen < 1 ){` |
|      ! 0 |  9123 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  9124 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9125 | `		return SXRET_OK;` |
|        - |  9126 | `	}` |
|        - |  9127 | `	/* Duplicate constant value */` |
|       14 |  9128 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  9129 | `	if( pValue == 0 ){` |
|      ! 0 |  9130 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9131 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9132 | `		return SXRET_OK;` |
|        - |  9133 | `	}` |
|        - |  9134 | `	/* Initialize the memory object */` |
|       14 |  9135 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  9136 | `	/* Register the constant */` |
|       14 |  9137 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  9138 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9139 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  9140 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9141 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9142 | `		return SXRET_OK;` |
|        - |  9143 | `	}` |
|        - |  9144 | `	/* Duplicate constant value */` |
|       14 |  9145 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  9146 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  9147 | `		/* Lower case the constant name */` |
|      ! 0 |  9148 | `		char *zCur = (char *)zName;` |
|      ! 0 |  9149 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  9150 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  9151 | `				/* UTF-8 stream */` |
|      ! 0 |  9152 | `				zCur++;` |
|      ! 0 |  9153 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  9154 | `					zCur++;` |
|      ! 0 |  9155 | `				}` |
|      ! 0 |  9156 | `				continue;` |
|        - |  9157 | `			}` |
|      ! 0 |  9158 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  9159 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9160 | `				zCur[0] = (char)c;` |
|      ! 0 |  9161 | `			}` |
|      ! 0 |  9162 | `			zCur++;` |
|      ! 0 |  9163 | `		}` |
|        - |  9164 | `		/* Finally,register the constant */` |
|      ! 0 |  9165 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9166 | `	}` |
|        - |  9167 | `	/* All done,return TRUE */` |
|       14 |  9168 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9169 | `	return SXRET_OK;` |
|        8 |  9170 |  |
|        - |  9171 | `/*` |
|        - |  9172 | ` * value constant(string $name)` |
|        - |  9173 | ` *  Returns the value of a constant` |
|        - |  9174 | ` * Parameter` |
|        - |  9175 | ` *  $name` |
|        - |  9176 | ` *    Name of the constant.` |
|        - |  9177 | ` * Return` |
|        - |  9178 | ` *  Constant value or NULL if not defined.` |
|        - |  9179 | ` */` |
|        8 |  9180 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9181 |  |
|        - |  9182 | `	SyHashEntry *pEntry;` |
|        - |  9183 | `	ph7_constant *pCons;` |
|        - |  9184 | `	const char *zName; /* Constant name */` |
|        - |  9185 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9186 | `	int nLen;` |
|       10 |  9187 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9188 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9189 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9190 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9191 | `		return SXRET_OK;` |
|        - |  9192 | `	}` |
|        - |  9193 | `	/* Extract the constant name */` |
|       10 |  9194 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9195 | `	/* Perform the query */` |
|       10 |  9196 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9197 | `	if( pEntry == 0 ){` |
|        3 |  9198 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9199 | `		ph7_result_null(pCtx);` |
|        3 |  9200 | `		return SXRET_OK;` |
|        - |  9201 | `	}` |
|        8 |  9202 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9203 | `	/* Point to the structure that describe the constant */` |
|        8 |  9204 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9205 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9206 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9207 | `	/* Return that value */` |
|        8 |  9208 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9209 | `	/* Cleanup */` |
|        8 |  9210 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9211 | `	return SXRET_OK;` |
|        6 |  9212 |  |
|        - |  9213 | `/*` |
|        - |  9214 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9215 | ` * defined below.` |
|        - |  9216 | ` */` |
|      452 |  9217 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9218 |  |
|      453 |  9219 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9220 | `	ph7_value sName;` |
|        - |  9221 | `	sxi32 rc;` |
|        - |  9222 | `	/* Prepare the constant name for insertion */` |
|      453 |  9223 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9224 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9225 | `	/* Perform the insertion */` |
|      453 |  9226 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9227 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9228 | `	return rc;` |
|        1 |  9229 |  |
|        - |  9230 | `/*` |
|        - |  9231 | ` * array get_defined_constants(void)` |
|        - |  9232 | ` *  Returns an associative array with the names of all defined` |
|        - |  9233 | ` *  constants.` |
|        - |  9234 | ` * Parameters` |
|        - |  9235 | ` *  NONE.` |
|        - |  9236 | ` * Returns` |
|        - |  9237 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9238 | ` */` |
|        2 |  9239 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9240 |  |
|        - |  9241 | `	ph7_value *pArray;` |
|        - |  9242 | `	/* Create the array first*/` |
|        3 |  9243 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9244 | `	if( pArray == 0 ){` |
|      ! 0 |  9245 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9246 | `		SXUNUSED(apArg);` |
|        - |  9247 | `		/* Return NULL */` |
|      ! 0 |  9248 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9249 | `		return SXRET_OK;` |
|        - |  9250 | `	}` |
|        - |  9251 | `	/* Fill the array with the defined constants */` |
|        3 |  9252 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9253 | `	/* Return the created array */` |
|        3 |  9254 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9255 | `	return SXRET_OK;` |
|        2 |  9256 |  |
|        - |  9257 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9258 | `/*` |
|        - |  9259 | ` * Section:` |
|        - |  9260 | ` *  Random numbers/string generators.` |
|        - |  9261 | ` * Status:` |
|        - |  9262 | ` *    Stable.` |
|        - |  9263 | ` */` |
|        - |  9264 | `/*` |
|        - |  9265 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9266 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9267 | ` * used by te SQLite3 library.` |
|        - |  9268 | ` */` |
|     2465 |  9269 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9270 |  |
|        - |  9271 | `	sxu32 iNum;` |
|     2467 |  9272 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2467 |  9273 | `	return iNum;` |
|        2 |  9274 |  |
|        - |  9275 | `/*` |
|        - |  9276 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9277 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9278 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9279 | ` * by te SQLite3 library.` |
|        - |  9280 | ` */` |
|   128114 |  9281 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9282 |  |
|        - |  9283 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9284 | `	int i;` |
|        - |  9285 | `	/* Generate a binary string first */` |
|   128116 |  9286 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9287 | `	/* Turn the binary string into english based alphabet */` |
|  1409424 |  9288 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1281310 |  9289 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   640656 |  9290 | `	 }` |
|   128116 |  9291 |  |
|        - |  9292 | `/*` |
|        - |  9293 | ` * int rand()` |
|        - |  9294 | ` * int mt_rand()` |
|        - |  9295 | ` * int rand(int $min,int $max)` |
|        - |  9296 | ` * int mt_rand(int $min,int $max)` |
|        - |  9297 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9298 | ` * Parameter` |
|        - |  9299 | ` *  $min` |
|        - |  9300 | ` *    The lowest value to return (default: 0)` |
|        - |  9301 | ` *  $max` |
|        - |  9302 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9303 | ` * Return` |
|        - |  9304 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9305 | ` * Note:` |
|        - |  9306 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9307 | ` *  by te SQLite3 library.` |
|        - |  9308 | ` */` |
|       20 |  9309 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9310 |  |
|        - |  9311 | `	sxu32 iNum;` |
|        - |  9312 | `	/* Generate the random number */` |
|       21 |  9313 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9314 | `	if( nArg > 1 ){` |
|        - |  9315 | `		sxu32 iMin,iMax;` |
|        3 |  9316 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9317 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9318 | `		if( iMin < iMax ){` |
|        3 |  9319 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9320 | `			if( iDiv > 0 ){` |
|        3 |  9321 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9322 | `			}` |
|        1 |  9323 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9324 | `			iNum %= iMax;` |
|      ! 0 |  9325 | `		}` |
|        1 |  9326 | `	}` |
|        - |  9327 | `	/* Return the number */` |
|       21 |  9328 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9329 | `	return SXRET_OK;` |
|        1 |  9330 |  |
|        - |  9331 | `/*` |
|        - |  9332 | ` * int getrandmax(void)` |
|        - |  9333 | ` * int mt_getrandmax(void)` |
|        - |  9334 | ` * int rc4_getrandmax(void)` |
|        - |  9335 | ` *   Show largest possible random value` |
|        - |  9336 | ` * Return` |
|        - |  9337 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9338 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9339 | ` * Note:` |
|        - |  9340 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9341 | ` *  by te SQLite3 library.` |
|        - |  9342 | ` */` |
|        4 |  9343 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9344 |  |
|        2 |  9345 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9346 | `	SXUNUSED(apArg);` |
|        5 |  9347 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9348 | `	return SXRET_OK;` |
|        1 |  9349 |  |
|        - |  9350 | `/*` |
|        - |  9351 | ` * string rand_str()` |
|        - |  9352 | ` * string rand_str(int $len)` |
|        - |  9353 | ` *  Generate a random string (English alphabet).` |
|        - |  9354 | ` * Parameter` |
|        - |  9355 | ` *  $len` |
|        - |  9356 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9357 | ` * Return` |
|        - |  9358 | ` *   A pseudo random string.` |
|        - |  9359 | ` * Note:` |
|        - |  9360 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9361 | ` *  by te SQLite3 library.` |
|        - |  9362 | ` *  This function is a symisc extension.` |
|        - |  9363 | ` */` |
|      120 |  9364 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9365 |  |
|        - |  9366 | `	char zString[1024];` |
|      122 |  9367 | `	int iLen = 0x10;` |
|      122 |  9368 | `	if( nArg > 0 ){` |
|        - |  9369 | `		/* Get the desired length */` |
|      122 |  9370 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9371 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9372 | `			/* Default length */` |
|        3 |  9373 | `			iLen = 0x10;` |
|        1 |  9374 | `		}` |
|       60 |  9375 | `	}` |
|        - |  9376 | `	/* Generate the random string */` |
|      122 |  9377 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9378 | `	/* Return the generated string */` |
|      122 |  9379 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9380 | `	return SXRET_OK;` |
|        2 |  9381 |  |
|        - |  9382 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9383 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9384 | `/* Unique ID private data */` |
|        - |  9385 | `struct unique_id_data` |
|        - |  9386 |  |
|        - |  9387 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9388 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9389 | `};` |
|        - |  9390 | `/*` |
|        - |  9391 | ` * Binary to hex consumer callback.` |
|        - |  9392 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9393 | ` * defined below.` |
|        - |  9394 | ` */` |
|      192 |  9395 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9396 |  |
|      193 |  9397 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9398 | `	sxu32 nBuflen;` |
|        - |  9399 | `	/* Extract result buffer length */` |
|      193 |  9400 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9401 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9402 | `			/*` |
|        - |  9403 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9404 | `			 * string will be 13 characters long` |
|        - |  9405 | `			 */` |
|       25 |  9406 | `		return SXERR_ABORT;` |
|        - |  9407 | `	}` |
|      169 |  9408 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9409 | `		return SXERR_ABORT;` |
|        - |  9410 | `	}` |
|        - |  9411 | `	/* Safely Consume the hex stream */` |
|      169 |  9412 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9413 | `	return SXRET_OK;` |
|       97 |  9414 |  |
|        - |  9415 | `/*` |
|        - |  9416 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9417 | ` *  Generate a unique ID` |
|        - |  9418 | ` * Parameter` |
|        - |  9419 | ` * $prefix` |
|        - |  9420 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9421 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9422 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9423 | ` * $more_entropy` |
|        - |  9424 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9425 | ` *  that the result will be unique.` |
|        - |  9426 | ` * Return` |
|        - |  9427 | ` *  Returns the unique identifier, as a string.` |
|        - |  9428 | ` */` |
|       24 |  9429 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9430 |  |
|        - |  9431 | `	struct unique_id_data sUniq;` |
|        - |  9432 | `	unsigned char zDigest[20];` |
|       25 |  9433 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9434 | `	const char *zPrefix;` |
|        - |  9435 | `	SHA1Context sCtx;` |
|        - |  9436 | `	char zRandom[7];` |
|        - |  9437 | `	int nPrefix;` |
|        - |  9438 | `	int entropy;` |
|        - |  9439 | `	/* Generate a random string first */` |
|       25 |  9440 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9441 | `	/* Initialize fields */` |
|       25 |  9442 | `	zPrefix = 0;` |
|       25 |  9443 | `	nPrefix = 0;` |
|       25 |  9444 | `	entropy = 0;` |
|       25 |  9445 | `	if( nArg > 0 ){` |
|        - |  9446 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9447 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9448 | `		if( nArg > 1 ){` |
|      ! 0 |  9449 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9450 | `		}` |
|      ! 0 |  9451 | `	}` |
|       25 |  9452 | `	SHA1Init(&sCtx);` |
|        - |  9453 | `	/* Generate the random ID */` |
|       25 |  9454 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9455 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9456 | `	}` |
|        - |  9457 | `	/* Append the random ID */` |
|       25 |  9458 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9459 | `	/* Append the random string */` |
|       25 |  9460 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9461 | `	/* Increment the number */` |
|       25 |  9462 | `	pVm->unique_id++;` |
|       25 |  9463 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9464 | `	/* Hexify the digest */` |
|       25 |  9465 | `	sUniq.pCtx = pCtx;` |
|       25 |  9466 | `	sUniq.entropy = entropy;` |
|       25 |  9467 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9468 | `	/* All done */` |
|       25 |  9469 | `	return PH7_OK;` |
|        1 |  9470 |  |
|        - |  9471 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9472 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9473 | `/*` |
|        - |  9474 | ` * Section:` |
|        - |  9475 | ` *  Language construct implementation as foreign functions.` |
|        - |  9476 | ` * Status:` |
|        - |  9477 | ` *    Stable.` |
|        - |  9478 | ` */` |
|        - |  9479 | `/*` |
|        - |  9480 | ` * void echo($string...)` |
|        - |  9481 | ` *  Output one or more messages.` |
|        - |  9482 | ` * Parameters` |
|        - |  9483 | ` *  $string` |
|        - |  9484 | ` *   Message to output.` |
|        - |  9485 | ` * Return` |
|        - |  9486 | ` *  NULL.` |
|        - |  9487 | ` */` |
|      ! 0 |  9488 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9489 |  |
|        - |  9490 | `	const char *zData;` |
|      ! 0 |  9491 | `	int nDataLen = 0;` |
|        - |  9492 | `	ph7_vm *pVm;` |
|        - |  9493 | `	int i,rc;` |
|        - |  9494 | `	/* Point to the target VM */` |
|      ! 0 |  9495 | `	pVm = pCtx->pVm;` |
|        - |  9496 | `	/* Output */` |
|      ! 0 |  9497 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9498 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9499 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9500 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9501 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9502 | `			if( rc == SXERR_ABORT ){` |
|        - |  9503 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9504 | `				return PH7_ABORT;` |
|        - |  9505 | `			}` |
|      ! 0 |  9506 | `		}` |
|      ! 0 |  9507 | `	}` |
|      ! 0 |  9508 | `	return SXRET_OK;` |
|      ! 0 |  9509 |  |
|        - |  9510 | `/*` |
|        - |  9511 | ` * int print($string...)` |
|        - |  9512 | ` *  Output one or more messages.` |
|        - |  9513 | ` * Parameters` |
|        - |  9514 | ` *  $string` |
|        - |  9515 | ` *   Message to output.` |
|        - |  9516 | ` * Return` |
|        - |  9517 | ` *  1 always.` |
|        - |  9518 | ` */` |
|        2 |  9519 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9520 |  |
|        - |  9521 | `	const char *zData;` |
|        3 |  9522 | `	int nDataLen = 0;` |
|        - |  9523 | `	ph7_vm *pVm;` |
|        - |  9524 | `	int i,rc;` |
|        - |  9525 | `	/* Point to the target VM */` |
|        3 |  9526 | `	pVm = pCtx->pVm;` |
|        - |  9527 | `	/* Output */` |
|        5 |  9528 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9529 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9530 | `		if( nDataLen > 0 ){` |
|        3 |  9531 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9532 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9533 | `			if( rc == SXERR_ABORT ){` |
|        - |  9534 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9535 | `				return PH7_ABORT;` |
|        - |  9536 | `			}` |
|        1 |  9537 | `		}` |
|        2 |  9538 | `	}` |
|        - |  9539 | `	/* Return 1 */` |
|        3 |  9540 | `	ph7_result_int(pCtx,1);` |
|        3 |  9541 | `	return SXRET_OK;` |
|        2 |  9542 |  |
|        - |  9543 | `/*` |
|        - |  9544 | ` * void exit(string $msg)` |
|        - |  9545 | ` * void exit(int $status)` |
|        - |  9546 | ` * void die(string $ms)` |
|        - |  9547 | ` * void die(int $status)` |
|        - |  9548 | ` *   Output a message and terminate program execution.` |
|        - |  9549 | ` * Parameter` |
|        - |  9550 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9551 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9552 | ` *  and not printed` |
|        - |  9553 | ` * Return` |
|        - |  9554 | ` *  NULL` |
|        - |  9555 | ` */` |
|      ! 0 |  9556 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9557 |  |
|      ! 0 |  9558 | `	if( nArg > 0 ){` |
|      ! 0 |  9559 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9560 | `			const char *zData;` |
|      ! 0 |  9561 | `			int iLen = 0;` |
|        - |  9562 | `			/* Print exit message */` |
|      ! 0 |  9563 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9564 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9565 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9566 | `			sxi32 iExitStatus;` |
|        - |  9567 | `			/* Record exit status code */` |
|      ! 0 |  9568 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9569 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9570 | `		}` |
|      ! 0 |  9571 | `	}` |
|        - |  9572 | `	/* Check if we are in an included file */` |
|      ! 0 |  9573 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9574 | `		/* Exit the entire process */` |
|      ! 0 |  9575 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9576 | `	}` |
|        - |  9577 | `	/* Abort processing immediately */` |
|      ! 0 |  9578 | `	return PH7_ABORT;` |
|      ! 0 |  9579 |  |
|        - |  9580 | `/*` |
|        - |  9581 | ` * bool isset($var,...)` |
|        - |  9582 | ` *  Finds out whether a variable is set.` |
|        - |  9583 | ` * Parameters` |
|        - |  9584 | ` *  One or more variable to check.` |
|        - |  9585 | ` * Return` |
|        - |  9586 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9587 | ` */` |
|    77796 |  9588 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9589 |  |
|        - |  9590 | `	ph7_value *pObj;` |
|    77798 |  9591 | `	int res = 0;` |
|        - |  9592 | `	int i;` |
|    77798 |  9593 | `	if( nArg < 1 ){` |
|        - |  9594 | `		/* Missing arguments,return false */` |
|      ! 0 |  9595 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9596 | `		return SXRET_OK;` |
|        - |  9597 | `	}` |
|        - |  9598 | `	/* Iterate over available arguments */` |
|   102370 |  9599 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    77798 |  9600 | `		pObj = apArg[i];` |
|    77798 |  9601 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    52666 |  9602 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9603 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9604 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9605 | `			}` |
|    26332 |  9606 | `		}` |
|    77798 |  9607 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    77798 |  9608 | `		if( !res ){` |
|        - |  9609 | `			/* Variable not set,return FALSE */` |
|    53226 |  9610 | `			ph7_result_bool(pCtx,0);` |
|    53226 |  9611 | `			return SXRET_OK;` |
|        - |  9612 | `		}` |
|    12288 |  9613 | `	}` |
|        - |  9614 | `	/* All given variable are set,return TRUE */` |
|    24574 |  9615 | `	ph7_result_bool(pCtx,1);` |
|    24574 |  9616 | `	return SXRET_OK;` |
|    38900 |  9617 |  |
|        - |  9618 | `/*` |
|        - |  9619 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9620 | ` * frame,the reference table and discard it's contents.` |
|        - |  9621 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9622 | ` */` |
|  3042930 |  9623 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9624 |  |
|        - |  9625 | `	ph7_value *pObj;` |
|        - |  9626 | `	VmRefObj *pRef;` |
|  3042932 |  9627 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3042932 |  9628 | `	if( pObj ){` |
|        - |  9629 | `		/* Release the object */` |
|  3042932 |  9630 | `		PH7_MemObjRelease(pObj);` |
|  1521465 |  9631 | `	}` |
|        - |  9632 | `	/* Remove old reference links */` |
|  3042932 |  9633 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3042932 |  9634 | `	if( pRef ){` |
|  3042926 |  9635 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9636 | `		/* Unlink from the reference table */` |
|  3042926 |  9637 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3042926 |  9638 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9639 | `			VmSlot sFree;` |
|        - |  9640 | `			/* Restore to the free list */` |
|  3042920 |  9641 | `			sFree.nIdx = nObjIdx;` |
|  3042920 |  9642 | `			sFree.pUserData = 0;` |
|  3042920 |  9643 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1521459 |  9644 | `		}` |
|  1521462 |  9645 | `	}` |
|  3042932 |  9646 | `	return SXRET_OK;` |
|        2 |  9647 |  |
|        - |  9648 | `/*` |
|        - |  9649 | ` * void unset($var,...)` |
|        - |  9650 | ` *   Unset one or more given variable.` |
|        - |  9651 | ` * Parameters` |
|        - |  9652 | ` *  One or more variable to unset.` |
|        - |  9653 | ` * Return` |
|        - |  9654 | ` *  Nothing.` |
|        - |  9655 | ` */` |
|     6848 |  9656 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9657 |  |
|        - |  9658 | `	ph7_value *pObj;` |
|        - |  9659 | `	ph7_vm *pVm;` |
|        - |  9660 | `	int i;` |
|        - |  9661 | `	/* Point to the target VM */` |
|     6850 |  9662 | `	pVm = pCtx->pVm;` |
|        - |  9663 | `	/* Iterate and unset */` |
|    13698 |  9664 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6850 |  9665 | `		pObj = apArg[i];` |
|     6850 |  9666 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9667 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9668 | `				/* Throw an error */` |
|      ! 0 |  9669 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9670 | `			}` |
|      ! 0 |  9671 | `		}else{` |
|     6850 |  9672 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9673 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6850 |  9674 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6844 |  9675 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3421 |  9676 | `			}` |
|        - |  9677 | `		}` |
|     3426 |  9678 | `	}` |
|     6850 |  9679 | `	return SXRET_OK;` |
|        2 |  9680 |  |
|        - |  9681 | `/*` |
|        - |  9682 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9683 | ` */` |
|      110 |  9684 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9685 |  |
|      111 |  9686 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9687 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9688 | `	ph7_value *pObj;` |
|        - |  9689 | `	sxu32 nIdx;` |
|        - |  9690 | `	/* Extract the memory object */` |
|      111 |  9691 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9692 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9693 | `	if( pObj ){` |
|      111 |  9694 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9695 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9696 | `				SyString sName;` |
|        - |  9697 | `				ph7_value sKey;` |
|        - |  9698 | `				/* Perform the insertion */` |
|      109 |  9699 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9700 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9701 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9702 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9703 | `			}` |
|       54 |  9704 | `		}` |
|       55 |  9705 | `	}` |
|      111 |  9706 | `	return SXRET_OK;` |
|        1 |  9707 |  |
|        - |  9708 | `/*` |
|        - |  9709 | ` * array get_defined_vars(void)` |
|        - |  9710 | ` *  Returns an array of all defined variables.` |
|        - |  9711 | ` * Parameter` |
|        - |  9712 | ` *  None` |
|        - |  9713 | ` * Return` |
|        - |  9714 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9715 | ` */` |
|        2 |  9716 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9717 |  |
|        3 |  9718 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9719 | `	ph7_value *pArray;` |
|        - |  9720 | `	/* Create a new array */` |
|        3 |  9721 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9722 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9723 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9724 | `		SXUNUSED(apArg);` |
|        - |  9725 | `		/* Return NULL */` |
|      ! 0 |  9726 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9727 | `		return SXRET_OK;` |
|        - |  9728 | `	}` |
|        - |  9729 | `	/* Superglobals first */` |
|        3 |  9730 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9731 | `	/* Then variable defined in the current frame */` |
|        3 |  9732 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9733 | `	/* Finally,return the created array */` |
|        3 |  9734 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9735 | `	return SXRET_OK;` |
|        2 |  9736 |  |
|        - |  9737 | `/*` |
|        - |  9738 | ` * bool gettype($var)` |
|        - |  9739 | ` *  Get the type of a variable` |
|        - |  9740 | ` * Parameters` |
|        - |  9741 | ` *   $var` |
|        - |  9742 | ` *    The variable being type checked.` |
|        - |  9743 | ` * Return` |
|        - |  9744 | ` *   String representation of the given variable type.` |
|        - |  9745 | ` */` |
|       32 |  9746 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9747 |  |
|       34 |  9748 | `	const char *zType = "Empty";` |
|       34 |  9749 | `	if( nArg > 0 ){` |
|       34 |  9750 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9751 | `	}` |
|        - |  9752 | `	/* Return the variable type */` |
|       34 |  9753 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9754 | `	return SXRET_OK;` |
|        2 |  9755 |  |
|        - |  9756 | `/*` |
|        - |  9757 | ` * string get_resource_type(resource $handle)` |
|        - |  9758 | ` *  This function gets the type of the given resource.` |
|        - |  9759 | ` * Parameters` |
|        - |  9760 | ` *  $handle` |
|        - |  9761 | ` *  The evaluated resource handle.` |
|        - |  9762 | ` * Return` |
|        - |  9763 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9764 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9765 | ` *  the return value will be the string Unknown.` |
|        - |  9766 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9767 | ` *  is not a resource.` |
|        - |  9768 | ` */` |
|        2 |  9769 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9770 |  |
|        3 |  9771 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9772 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9773 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9774 | `		return PH7_OK;` |
|        - |  9775 | `	}` |
|        3 |  9776 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9777 | `	return SXRET_OK;` |
|        2 |  9778 |  |
|        - |  9779 | `/*` |
|        - |  9780 | ` * void var_dump(expression,....)` |
|        - |  9781 | ` *   var_dump � Dumps information about a variable` |
|        - |  9782 | ` * Parameters` |
|        - |  9783 | ` *   One or more expression to dump.` |
|        - |  9784 | ` * Returns` |
|        - |  9785 | ` *  Nothing.` |
|        - |  9786 | ` */` |
|      218 |  9787 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9788 |  |
|        - |  9789 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9790 | `	int i;` |
|      220 |  9791 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9792 | `	/* Dump one or more expressions */` |
|      444 |  9793 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9794 | `		ph7_value *pObj = apArg[i];` |
|        - |  9795 | `		/* Reset the working buffer */` |
|      226 |  9796 | `		SyBlobReset(&sDump);` |
|        - |  9797 | `		/* Dump the given expression */` |
|      226 |  9798 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9799 | `		/* Output */` |
|      226 |  9800 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9801 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9802 | `		}` |
|      114 |  9803 | `	}` |
|        - |  9804 | `	/* Release the working buffer */` |
|      220 |  9805 | `	SyBlobRelease(&sDump);` |
|      220 |  9806 | `	return SXRET_OK;` |
|        2 |  9807 |  |
|        - |  9808 | `/*` |
|        - |  9809 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9810 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9811 | ` * Parameters` |
|        - |  9812 | ` *   expression: Expression to dump` |
|        - |  9813 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9814 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9815 | ` *            print_r() will return the information rather than print it.` |
|        - |  9816 | ` * Return` |
|        - |  9817 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9818 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9819 | ` */` |
|       16 |  9820 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9821 |  |
|       17 |  9822 | `	int ret_string = 0;` |
|        - |  9823 | `	SyBlob sDump;` |
|       17 |  9824 | `	if( nArg < 1 ){` |
|        - |  9825 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9826 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9827 | `		return SXRET_OK;` |
|        - |  9828 | `	}` |
|       17 |  9829 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9830 | `	if ( nArg > 1 ){` |
|        - |  9831 | `		/* Where to redirect output */` |
|       11 |  9832 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9833 | `	}` |
|        - |  9834 | `	/* Generate dump */` |
|       17 |  9835 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9836 | `	if( !ret_string ){` |
|        - |  9837 | `		/* Output dump */` |
|        7 |  9838 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9839 | `		/* Return true */` |
|        7 |  9840 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9841 | `	}else{` |
|        - |  9842 | `		/* Generated dump as return value */` |
|       11 |  9843 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9844 | `	}` |
|        - |  9845 | `	/* Release the working buffer */` |
|       17 |  9846 | `	SyBlobRelease(&sDump);` |
|       17 |  9847 | `	return SXRET_OK;` |
|        9 |  9848 |  |
|        - |  9849 | `/*` |
|        - |  9850 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9851 | ` * Same job as print_r. (see coment above)` |
|        - |  9852 | ` */` |
|        2 |  9853 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9854 |  |
|        3 |  9855 | `	int ret_string = 0;` |
|        - |  9856 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9857 | `	if( nArg < 1 ){` |
|        - |  9858 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9859 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9860 | `		return SXRET_OK;` |
|        - |  9861 | `	}` |
|        3 |  9862 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9863 | `	if ( nArg > 1 ){` |
|        - |  9864 | `		/* Where to redirect output */` |
|        3 |  9865 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9866 | `	}` |
|        - |  9867 | `	/* Generate dump */` |
|        3 |  9868 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9869 | `	if( !ret_string ){` |
|        - |  9870 | `		/* Output dump */` |
|      ! 0 |  9871 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9872 | `		/* Return NULL */` |
|      ! 0 |  9873 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9874 | `	}else{` |
|        - |  9875 | `		/* Generated dump as return value */` |
|        3 |  9876 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9877 | `	}` |
|        - |  9878 | `	/* Release the working buffer */` |
|        3 |  9879 | `	SyBlobRelease(&sDump);` |
|        3 |  9880 | `	return SXRET_OK;` |
|        2 |  9881 |  |
|        - |  9882 | `/*` |
|        - |  9883 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9884 | ` *  Set/get the various assert flags.` |
|        - |  9885 | ` * Parameter` |
|        - |  9886 | ` * $what` |
|        - |  9887 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9888 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9889 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9890 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9891 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9892 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9893 | ` * $value` |
|        - |  9894 | ` *   An optional new value for the option.` |
|        - |  9895 | ` * Return` |
|        - |  9896 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9897 | ` */` |
|       28 |  9898 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9899 |  |
|       30 |  9900 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9901 | `	int iOption;` |
|        - |  9902 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9903 | `	if( nArg < 1 ){` |
|        3 |  9904 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9905 | `			"ArgumentCountError",` |
|        - |  9906 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9907 | `			);` |
|        - |  9908 | `	}` |
|        - |  9909 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9910 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9911 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9912 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9913 | `			"TypeError",` |
|        - |  9914 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9915 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9916 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9917 | `			);` |
|        - |  9918 | `	}` |
|       28 |  9919 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9920 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9921 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9922 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9923 | `	switch( iOption ){` |
|        5 |  9924 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9925 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9926 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9927 | `		if( nArg > 1 ){` |
|        5 |  9928 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9929 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9930 | `			}else{` |
|        3 |  9931 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9932 | `			}` |
|        2 |  9933 | `		}` |
|       12 |  9934 | `		break;` |
|        1 |  9935 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9936 | `		/* Return old callback or null */` |
|        3 |  9937 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9938 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9939 | `		}else{` |
|        3 |  9940 | `			ph7_result_null(pCtx);` |
|        - |  9941 | `		}` |
|        3 |  9942 | `		if( nArg > 1 ){` |
|      ! 0 |  9943 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9944 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9945 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9946 | `			}else{` |
|      ! 0 |  9947 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9948 | `			}` |
|      ! 0 |  9949 | `		}` |
|        3 |  9950 | `		break;` |
|        5 |  9951 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9952 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9953 | `		if( nArg > 1 ){` |
|        5 |  9954 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9955 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9956 | `			}else{` |
|        3 |  9957 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9958 | `			}` |
|        2 |  9959 | `		}` |
|       11 |  9960 | `		break;` |
|      ! 0 |  9961 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9962 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9963 | `		break;` |
|        1 |  9964 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9965 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9966 | `		break;` |
|      ! 0 |  9967 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9968 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9969 | `		break;` |
|        1 |  9970 | `	default:` |
|        - |  9971 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9972 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9973 | `			"ValueError",` |
|        - |  9974 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9975 | `			);` |
|        - |  9976 | `	}` |
|       26 |  9977 | `	return PH7_OK;` |
|       16 |  9978 |  |
|        - |  9979 | `/*` |
|        - |  9980 | ` * bool assert(mixed $assertion)` |
|        - |  9981 | ` *  Checks if assertion is FALSE.` |
|        - |  9982 | ` * Parameter` |
|        - |  9983 | ` *  $assertion` |
|        - |  9984 | ` *    The assertion to test.` |
|        - |  9985 | ` * Return` |
|        - |  9986 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9987 | ` */` |
|       24 |  9988 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9989 |  |
|       26 |  9990 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9991 | `	int iFlags,iResult;` |
|        - |  9992 | `	const char *zDesc;` |
|        - |  9993 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9994 | `	if( nArg < 1 ){` |
|        3 |  9995 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9996 | `			"ArgumentCountError",` |
|        - |  9997 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9998 | `			);` |
|        - |  9999 | `	}` |
|       24 | 10000 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 10001 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 10002 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 10003 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 10004 | `		return PH7_OK;` |
|        - | 10005 | `	}` |
|        - | 10006 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 10007 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 10008 | `	if( !iResult ){` |
|        - | 10009 | `		/* Assertion failed */` |
|        - | 10010 | `		/* Extract optional description */` |
|       13 | 10011 | `		zDesc = 0;` |
|       13 | 10012 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10013 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 10014 | `		}` |
|       13 | 10015 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 10016 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 10017 | `			ph7_value sFile,sLine;` |
|        - | 10018 | `			ph7_value *apCbArg[3];` |
|        - | 10019 | `			SyString *pFile;` |
|        - | 10020 | `			/* Extract the processed script */` |
|      ! 0 | 10021 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 10022 | `			if( pFile == 0 ){` |
|      ! 0 | 10023 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 10024 | `			}` |
|        - | 10025 | `			/* Invoke the callback */` |
|      ! 0 | 10026 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 10027 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 10028 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 10029 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 10030 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 10031 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 10032 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 10033 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 10034 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 10035 | `		}` |
|       13 | 10036 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 10037 | `			/* Abort VM execution immediately */` |
|      ! 0 | 10038 | `			return PH7_ABORT;` |
|        - | 10039 | `		}` |
|        - | 10040 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 10041 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 10042 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10043 | `				"AssertionError",` |
|        - | 10044 | `				"%s",` |
|        1 | 10045 | `				zDesc` |
|        - | 10046 | `				);` |
|      ! 0 | 10047 | `		}else{` |
|       11 | 10048 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10049 | `				"AssertionError",` |
|        - | 10050 | `				"assert(false)"` |
|        - | 10051 | `				);` |
|        - | 10052 | `		}` |
|        - | 10053 | `	}` |
|        - | 10054 | `	/* Assertion passed */` |
|       11 | 10055 | `	ph7_result_bool(pCtx,1);` |
|       11 | 10056 | `	return PH7_OK;` |
|       14 | 10057 |  |
|        - | 10058 | `/*` |
|        - | 10059 | ` * Section:` |
|        - | 10060 | ` *  Error reporting functions.` |
|        - | 10061 | ` * Status:` |
|        - | 10062 | ` *    Stable.` |
|        - | 10063 | ` */` |
|        - | 10064 | `/*` |
|        - | 10065 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 10066 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 10067 | ` * Parameters` |
|        - | 10068 | ` *  $error_msg` |
|        - | 10069 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 10070 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 10071 | ` * $error_type` |
|        - | 10072 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 10073 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 10074 | ` * Return` |
|        - | 10075 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 10076 | ` */` |
|       12 | 10077 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10078 |  |
|       14 | 10079 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 10080 | `	int rc = PH7_OK;` |
|       14 | 10081 | `	if( nArg > 0 ){` |
|        - | 10082 | `		const char *zErr;` |
|        - | 10083 | `		int nLen;` |
|        - | 10084 | `		/* Extract the error message */` |
|       12 | 10085 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 10086 | `		if( nArg > 1 ){` |
|        - | 10087 | `			/* Extract the error type */` |
|       12 | 10088 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 10089 | `			switch( nErr ){` |
|        1 | 10090 | `			case 1:   /* E_ERROR */` |
|        - | 10091 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 10092 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 10093 | `			case 256: /* E_USER_ERROR */` |
|        3 | 10094 | `				nErr = PH7_CTX_ERR;` |
|        3 | 10095 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 10096 | `				break;` |
|        1 | 10097 | `			case 2:   /* E_WARNING */` |
|        - | 10098 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 10099 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 10100 | `			case 512: /* E_USER_WARNING */` |
|        3 | 10101 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 10102 | `				break;` |
|        3 | 10103 | `			default:` |
|        8 | 10104 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 10105 | `				break;` |
|        - | 10106 | `			}` |
|        5 | 10107 | `		}` |
|        - | 10108 | `		/* Report error */` |
|       12 | 10109 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 10110 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 10111 | `			return rc;` |
|        - | 10112 | `		}` |
|        - | 10113 | `		/* Return true */` |
|       12 | 10114 | `		ph7_result_bool(pCtx,1);` |
|        7 | 10115 | `	}else{` |
|        - | 10116 | `		/* Missing arguments,return FALSE */` |
|        3 | 10117 | `		ph7_result_bool(pCtx,0);` |
|        - | 10118 | `	}` |
|       14 | 10119 | `	return rc;` |
|        8 | 10120 |  |
|        - | 10121 | `/*` |
|        - | 10122 | ` * int error_reporting([int $level])` |
|        - | 10123 | ` *  Sets which PHP errors are reported.` |
|        - | 10124 | ` * Parameters` |
|        - | 10125 | ` *  $level` |
|        - | 10126 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 10127 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 10128 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 10129 | ` *   levels will not always behave as expected.` |
|        - | 10130 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 10131 | ` *   in the predefined constants.` |
|        - | 10132 | ` * Return` |
|        - | 10133 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 10134 | ` *   parameter is given.` |
|        - | 10135 | ` */` |
|       38 | 10136 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10137 |  |
|       40 | 10138 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10139 | `	int nOld;` |
|        - | 10140 | `	/* Extract the old reporting level */` |
|       40 | 10141 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 10142 | `	if( nArg > 0 ){` |
|        - | 10143 | `		int nNew;` |
|        - | 10144 | `		/* Extract the desired error reporting level */` |
|       32 | 10145 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 10146 | `		if( !nNew ){` |
|        - | 10147 | `			/* Do not report errors at all */` |
|        5 | 10148 | `			pVm->bErrReport = 0;` |
|        3 | 10149 | `		}else{` |
|        - | 10150 | `			/* Report all errors */` |
|       28 | 10151 | `			pVm->bErrReport = 1;` |
|        - | 10152 | `		}` |
|       15 | 10153 | `	}` |
|        - | 10154 | `	/* Return the old level */` |
|       40 | 10155 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 10156 | `	return PH7_OK;` |
|        2 | 10157 |  |
|        - | 10158 | `/*` |
|        - | 10159 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10160 | ` *  Send an error message somewhere.` |
|        - | 10161 | ` * Parameter` |
|        - | 10162 | ` *  $message` |
|        - | 10163 | ` *   The error message that should be logged.` |
|        - | 10164 | ` *  $message_type` |
|        - | 10165 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10166 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10167 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10168 | ` *       This is the default option.` |
|        - | 10169 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10170 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10171 | ` *    2  No longer an option.` |
|        - | 10172 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10173 | ` *       to the end of the message string.` |
|        - | 10174 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10175 | ` *  $destination` |
|        - | 10176 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10177 | ` *  $extra_headers` |
|        - | 10178 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10179 | ` * Return` |
|        - | 10180 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10181 | ` * NOTE:` |
|        - | 10182 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10183 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10184 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10185 | ` *  Otherwise this function is no-op.` |
|        - | 10186 | ` */` |
|        4 | 10187 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10188 |  |
|        - | 10189 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10190 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10191 | `	int iType = 0;` |
|        5 | 10192 | `	if( nArg < 1 ){` |
|        - | 10193 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10194 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10195 | `		return PH7_OK;` |
|        - | 10196 | `	}` |
|        5 | 10197 | `	if( pVm->xErrLog  ){` |
|        - | 10198 | `		/* Invoke the user callback */` |
|      ! 0 | 10199 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10200 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10201 | `		if( nArg > 1 ){` |
|      ! 0 | 10202 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10203 | `			if( nArg > 2 ){` |
|      ! 0 | 10204 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10205 | `				if( nArg > 3 ){` |
|      ! 0 | 10206 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10207 | `				}` |
|      ! 0 | 10208 | `			}` |
|      ! 0 | 10209 | `		}` |
|      ! 0 | 10210 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10211 | `	}` |
|        - | 10212 | `	/* Retun TRUE */` |
|        5 | 10213 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10214 | `	return PH7_OK;` |
|        3 | 10215 |  |
|        - | 10216 | `/*` |
|        - | 10217 | ` * bool restore_exception_handler(void)` |
|        - | 10218 | ` *  Restores the previously defined exception handler function.` |
|        - | 10219 | ` * Parameter` |
|        - | 10220 | ` *  None` |
|        - | 10221 | ` * Return` |
|        - | 10222 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10223 | ` */` |
|        4 | 10224 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10225 |  |
|        5 | 10226 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10227 | `	ph7_value *pOld,*pNew;` |
|        - | 10228 | `	/* Point to the old and the new handler */` |
|        5 | 10229 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10230 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10231 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10232 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10233 | `		SXUNUSED(apArg);` |
|        - | 10234 | `		/* No installed handler,return FALSE */` |
|        5 | 10235 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10236 | `		return PH7_OK;` |
|        - | 10237 | `	}` |
|        - | 10238 | `	/* Copy the old handler */` |
|      ! 0 | 10239 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10240 | `	PH7_MemObjRelease(pOld);` |
|        - | 10241 | `	/* Return TRUE */` |
|      ! 0 | 10242 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10243 | `	return PH7_OK;` |
|        3 | 10244 |  |
|        - | 10245 | `/*` |
|        - | 10246 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10247 | ` *  Sets a user-defined exception handler function.` |
|        - | 10248 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10249 | ` * NOTE` |
|        - | 10250 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10251 | ` *  the satndard PHP engine.` |
|        - | 10252 | ` * Parameters` |
|        - | 10253 | ` *  $exception_handler` |
|        - | 10254 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10255 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10256 | ` *   that was thrown.` |
|        - | 10257 | ` *  Note:` |
|        - | 10258 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10259 | ` * Return` |
|        - | 10260 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10261 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10262 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10263 | ` */` |
|        4 | 10264 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10265 |  |
|        6 | 10266 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10267 | `	ph7_value *pOld,*pNew;` |
|        - | 10268 | `	/* Point to the old and the new handler */` |
|        6 | 10269 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10270 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10271 | `	/* Return the old handler */` |
|        6 | 10272 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10273 | `	if( nArg > 0 ){` |
|        6 | 10274 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10275 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10276 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10277 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10278 | `		}else{` |
|        6 | 10279 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10280 | `			/* Install the new handler */` |
|        6 | 10281 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10282 | `		}` |
|        2 | 10283 | `	}` |
|        6 | 10284 | `	return PH7_OK;` |
|        2 | 10285 |  |
|        - | 10286 | `/*` |
|        - | 10287 | ` * bool restore_error_handler(void)` |
|        - | 10288 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10289 | ` * Parameters:` |
|        - | 10290 | ` *  None.` |
|        - | 10291 | ` * Return` |
|        - | 10292 | ` *  Always TRUE.` |
|        - | 10293 | ` */` |
|        4 | 10294 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10295 |  |
|        5 | 10296 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10297 | `	ph7_value *pOld,*pNew;` |
|        - | 10298 | `	/* Point to the old and the new handler */` |
|        5 | 10299 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10300 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10301 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10302 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10303 | `		SXUNUSED(apArg);` |
|        - | 10304 | `		/* No installed callback,return FALSE */` |
|        5 | 10305 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10306 | `		return PH7_OK;` |
|        - | 10307 | `	}` |
|        - | 10308 | `	/* Copy the old callback */` |
|      ! 0 | 10309 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10310 | `	PH7_MemObjRelease(pOld);` |
|        - | 10311 | `	/* Return TRUE */` |
|      ! 0 | 10312 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10313 | `	return PH7_OK;` |
|        3 | 10314 |  |
|        - | 10315 | `/*` |
|        - | 10316 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10317 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10318 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10319 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10320 | ` *  Sets a user-defined error handler function.` |
|        - | 10321 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10322 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10323 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10324 | ` *  conditions (using trigger_error()).` |
|        - | 10325 | ` * Parameters` |
|        - | 10326 | ` *  $error_handler` |
|        - | 10327 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10328 | ` *   describing the error.` |
|        - | 10329 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10330 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10331 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10332 | ` *   The function can be shown as:` |
|        - | 10333 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10334 | ` *     errno` |
|        - | 10335 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10336 | ` *   errstr` |
|        - | 10337 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10338 | ` *   errfile` |
|        - | 10339 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10340 | ` *     was raised in, as a string.` |
|        - | 10341 | ` *  Note:` |
|        - | 10342 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10343 | ` * Return` |
|        - | 10344 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10345 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10346 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10347 | ` */` |
|     9474 | 10348 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10349 |  |
|     9476 | 10350 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10351 | `	ph7_value *pOld,*pNew;` |
|        - | 10352 | `	/* Point to the old and the new handler */` |
|     9476 | 10353 | `	pOld = &pVm->aErrCB[0];` |
|     9476 | 10354 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10355 | `	/* Return the old handler */` |
|     9476 | 10356 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9476 | 10357 | `	if( nArg > 0 ){` |
|     9476 | 10358 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10359 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4737 | 10360 | `			PH7_MemObjRelease(pNew);` |
|     4737 | 10361 | `			ph7_result_bool(pCtx,1);` |
|     2369 | 10362 | `		}else{` |
|     4740 | 10363 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10364 | `			/* Install the new handler */` |
|     4740 | 10365 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10366 | `		}` |
|     4737 | 10367 | `	}` |
|     9476 | 10368 | `	return PH7_OK;` |
|        2 | 10369 |  |
|        - | 10370 | `/*` |
|        - | 10371 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10372 | ` *  Generates a backtrace.` |
|        - | 10373 | ` * Paramaeter` |
|        - | 10374 | ` *  $options` |
|        - | 10375 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10376 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10377 | ` *   all the function/method arguments, to save memory.` |
|        - | 10378 | ` * $limit` |
|        - | 10379 | ` *   (Not Used)` |
|        - | 10380 | ` * Return` |
|        - | 10381 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10382 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10383 | ` *          Name        Type      Description` |
|        - | 10384 | ` *          ------      ------     -----------` |
|        - | 10385 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10386 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10387 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10388 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10389 | ` *          object      object    The current object.` |
|        - | 10390 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10391 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10392 | ` */` |
|      554 | 10393 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10394 |  |
|      556 | 10395 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10396 | `	ph7_value *pArray;` |
|        - | 10397 | `	ph7_class *pClass;` |
|        - | 10398 | `	ph7_value *pValue;` |
|        - | 10399 | `	SyString *pFile;` |
|        - | 10400 | `	/* Create a new array */` |
|      556 | 10401 | `	pArray = ph7_context_new_array(pCtx);` |
|      556 | 10402 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      556 | 10403 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10404 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10405 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10406 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10407 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10408 | `		SXUNUSED(apArg);` |
|      ! 0 | 10409 | `		return PH7_OK;` |
|        - | 10410 | `	}` |
|        - | 10411 | `	/* Dump running function name and it's arguments  */` |
|      556 | 10412 | `	if( pVm->pFrame->pParent ){` |
|      556 | 10413 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10414 | `		ph7_vm_func *pFunc;` |
|        - | 10415 | `		ph7_value *pArg;` |
|      556 | 10416 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      556 | 10417 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      556 | 10418 | `		if( pFrame->pParent && pFunc ){` |
|      556 | 10419 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      556 | 10420 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      556 | 10421 | `			ph7_value_reset_string_cursor(pValue);` |
|      277 | 10422 | `		}` |
|        - | 10423 | `		/* Function arguments */` |
|      556 | 10424 | `		pArg = ph7_context_new_array(pCtx);` |
|      556 | 10425 | `		if( pArg  ){` |
|        - | 10426 | `			ph7_value *pObj;` |
|        - | 10427 | `			VmSlot *aSlot;` |
|        - | 10428 | `			sxu32 n;` |
|        - | 10429 | `			/* Start filling the array with the given arguments */` |
|      556 | 10430 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2210 | 10431 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1656 | 10432 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1656 | 10433 | `				if( pObj ){` |
|     1656 | 10434 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      827 | 10435 | `				}` |
|      829 | 10436 | `			}` |
|        - | 10437 | `			/* Save the array */` |
|      556 | 10438 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      277 | 10439 | `		}` |
|      277 | 10440 | `	}` |
|      556 | 10441 | `	ph7_value_int(pValue,1);` |
|        - | 10442 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10443 | `	 * line numbers at run-time. )` |
|        - | 10444 | `	 */` |
|      556 | 10445 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10446 | `	/* Current processed script */` |
|      556 | 10447 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      556 | 10448 | `	if( pFile ){` |
|      556 | 10449 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      556 | 10450 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      556 | 10451 | `		ph7_value_reset_string_cursor(pValue);` |
|      277 | 10452 | `	}` |
|        - | 10453 | `	/* Top class */` |
|      556 | 10454 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      556 | 10455 | `	if( pClass ){` |
|      552 | 10456 | `		ph7_value_reset_string_cursor(pValue);` |
|      552 | 10457 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      552 | 10458 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      275 | 10459 | `	}` |
|        - | 10460 | `	/* Return the freshly created array */` |
|      556 | 10461 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10462 | `	/*` |
|        - | 10463 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10464 | `	 * as soon we return from this function.` |
|        - | 10465 | `	 */` |
|      556 | 10466 | `	return PH7_OK;` |
|      279 | 10467 |  |
|        - | 10468 | `/*` |
|        - | 10469 | ` * Generate a small backtrace.` |
|        - | 10470 | ` * Store the generated dump in the given BLOB` |
|        - | 10471 | ` */` |
|        4 | 10472 | `static int VmMiniBacktrace(` |
|        - | 10473 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10474 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10475 | `	)` |
|        1 | 10476 |  |
|        5 | 10477 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10478 | `	ph7_vm_func *pFunc;` |
|        - | 10479 | `	ph7_class *pClass;` |
|        - | 10480 | `	SyString *pFile;` |
|        - | 10481 | `	/* Called function */` |
|        5 | 10482 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10483 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10484 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10485 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10486 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10487 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10488 | `	}else{` |
|      ! 0 | 10489 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10490 | `	}` |
|        5 | 10491 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10492 | `	/* Current processed script */` |
|        5 | 10493 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10494 | `	if( pFile ){` |
|        5 | 10495 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10496 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10497 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10498 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10499 | `	}` |
|        - | 10500 | `	/* Top class */` |
|        5 | 10501 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10502 | `	if( pClass ){` |
|      ! 0 | 10503 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10504 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10505 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10506 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10507 | `	}` |
|        5 | 10508 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10509 | `	/* All done */` |
|        5 | 10510 | `	return SXRET_OK;` |
|        1 | 10511 |  |
|        - | 10512 | `/*` |
|        - | 10513 | ` * void debug_print_backtrace()` |
|        - | 10514 | ` *  Prints a backtrace` |
|        - | 10515 | ` * Parameters` |
|        - | 10516 | ` * None` |
|        - | 10517 | ` * Return` |
|        - | 10518 | ` * NULL` |
|        - | 10519 | ` */` |
|        2 | 10520 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10521 |  |
|        3 | 10522 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10523 | `	SyBlob sDump;` |
|        3 | 10524 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10525 | `	/* Generate the backtrace */` |
|        3 | 10526 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10527 | `	/* Output backtrace */` |
|        3 | 10528 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10529 | `	/* All done,cleanup */` |
|        3 | 10530 | `	SyBlobRelease(&sDump);` |
|        1 | 10531 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10532 | `	SXUNUSED(apArg);` |
|        3 | 10533 | `	return PH7_OK;` |
|        1 | 10534 |  |
|        - | 10535 | `/*` |
|        - | 10536 | ` * string debug_string_backtrace()` |
|        - | 10537 | ` *  Generate a backtrace` |
|        - | 10538 | ` * Parameters` |
|        - | 10539 | ` * None` |
|        - | 10540 | ` * Return` |
|        - | 10541 | ` *  A mini backtrace().` |
|        - | 10542 | ` * Note that this is a symisc extension.` |
|        - | 10543 | ` */` |
|        2 | 10544 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10545 |  |
|        3 | 10546 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10547 | `	SyBlob sDump;` |
|        3 | 10548 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10549 | `	/* Generate the backtrace */` |
|        3 | 10550 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10551 | `	/* Return the backtrace */` |
|        3 | 10552 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10553 | `	/* All done,cleanup */` |
|        3 | 10554 | `	SyBlobRelease(&sDump);` |
|        1 | 10555 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10556 | `	SXUNUSED(apArg);` |
|        3 | 10557 | `	return PH7_OK;` |
|        1 | 10558 |  |
|        - | 10559 | `/*` |
|        - | 10560 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10561 | ` * exception is triggered.` |
|        - | 10562 | ` */` |
|      480 | 10563 | `static sxi32 VmUncaughtException(` |
|        - | 10564 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10565 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10566 | `	)` |
|        1 | 10567 |  |
|        - | 10568 | `	ph7_value *apArg[2],sArg;` |
|      481 | 10569 | `	int nArg = 1;` |
|        - | 10570 | `	sxi32 rc;` |
|      481 | 10571 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10572 | `		/* Nesting limit reached */` |
|      ! 0 | 10573 | `		return SXRET_OK;` |
|        - | 10574 | `	}` |
|        - | 10575 | `	/* Call any exception handler if available */` |
|      481 | 10576 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 10577 | `	if( pThis ){` |
|        - | 10578 | `		/* Load the exception instance */` |
|      481 | 10579 | `		sArg.x.pOther = pThis;` |
|      481 | 10580 | `		pThis->iRef++;` |
|      481 | 10581 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 10582 | `	}else{` |
|      ! 0 | 10583 | `		nArg = 0;` |
|        - | 10584 | `	}` |
|      481 | 10585 | `	apArg[0] = &sArg;` |
|        - | 10586 | `	/* Call the exception handler if available */` |
|      481 | 10587 | `	pVm->nExceptDepth++;` |
|      481 | 10588 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 10589 | `	pVm->nExceptDepth--;` |
|      481 | 10590 | `	if( rc != SXRET_OK ){` |
|        - | 10591 | `		SyBlob sMsgBuf;` |
|      479 | 10592 | `		const char *zClass = "Exception";` |
|      479 | 10593 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10594 | `		const char *zMsg;` |
|        - | 10595 | `		sxu32 nMsg;` |
|        - | 10596 | `		const char *zFuncName;` |
|        - | 10597 | `		int nFuncLen;` |
|      479 | 10598 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 10599 | `		if( pThis ){` |
|        - | 10600 | `			ph7_class_method *pGetMessage;` |
|        - | 10601 | `			ph7_value sMsg;` |
|        - | 10602 | `			const char *zTmp;` |
|        - | 10603 | `			int nTmp;` |
|      479 | 10604 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 10605 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 10606 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 10607 | `			if( pGetMessage ){` |
|      479 | 10608 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 10609 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 10610 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 10611 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 10612 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 10613 | `					}` |
|      239 | 10614 | `				}` |
|      479 | 10615 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 10616 | `			}` |
|      239 | 10617 | `		}` |
|      479 | 10618 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10619 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10620 | `		}` |
|      479 | 10621 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 10622 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 10623 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 10624 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 10625 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10626 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 10627 | `		rc = SXERR_ABORT;` |
|      239 | 10628 | `	}` |
|      481 | 10629 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 10630 | `	return rc;` |
|      241 | 10631 |  |
|        - | 10632 | `/*` |
|        - | 10633 | ` * Throw a user exception.` |
|        - | 10634 | ` *` |
|        - | 10635 | ` * Exception dispatch follows this sequence:` |
|        - | 10636 | ` *` |
|        - | 10637 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10638 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10639 | ` *` |
|        - | 10640 | ` * 2. If NO catch matches:` |
|        - | 10641 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10642 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10643 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10644 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10645 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10646 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10647 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10648 | ` *` |
|        - | 10649 | ` * 3. If a catch DOES match:` |
|        - | 10650 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10651 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10652 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10653 | ` *       finally block.` |
|        - | 10654 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10655 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10656 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10657 | ` *       in pPendingException (step 2c).` |
|        - | 10658 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10659 | ` *    d. Run finally (if present).` |
|        - | 10660 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10661 | ` *       that handlers are restored and finally has run.` |
|        - | 10662 | ` */` |
|      558 | 10663 | `static sxi32 VmThrowException(` |
|        - | 10664 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10665 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10666 | `	)` |
|        2 | 10667 |  |
|        - | 10668 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10669 | `	ph7_exception **apException;` |
|        - | 10670 | `	ph7_exception *pException;` |
|        - | 10671 | `	/* Point to the stack of loaded exceptions */` |
|      560 | 10672 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      560 | 10673 | `	pException = 0;` |
|      560 | 10674 | `	pCatch = 0;` |
|      560 | 10675 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10676 | `		ph7_exception_block *aCatch;` |
|        - | 10677 | `		ph7_class *pClass;` |
|        - | 10678 | `		SyString *aNames;` |
|        - | 10679 | `		sxu32 nNames;` |
|        - | 10680 | `		int matched;` |
|        - | 10681 | `		sxu32 j,k;` |
|        - | 10682 | `		/* Locate the appropriate block to execute */` |
|       74 | 10683 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       74 | 10684 | `		(void)SySetPop(&pVm->aException);` |
|       74 | 10685 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       76 | 10686 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 10687 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|       74 | 10688 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|       74 | 10689 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|       74 | 10690 | `			matched = 0;` |
|       88 | 10691 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 10692 | `				/* Extract the target class */` |
|       86 | 10693 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|       86 | 10694 | `				if( pClass == 0 ){` |
|        - | 10695 | `					/* No such class */` |
|      ! 0 | 10696 | `					continue;` |
|        - | 10697 | `				}` |
|       86 | 10698 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|       72 | 10699 | `					matched = 1;` |
|       72 | 10700 | `					break;` |
|        - | 10701 | `				}` |
|        8 | 10702 | `			}` |
|       74 | 10703 | `			if( matched ){` |
|        - | 10704 | `				/* Catch block found,break immediately */` |
|       72 | 10705 | `				pCatch = &aCatch[j];` |
|       72 | 10706 | `				break;` |
|        - | 10707 | `			}` |
|        2 | 10708 | `		}` |
|       36 | 10709 | `	}` |
|        - | 10710 | `	/* Execute the cached block if available */` |
|      560 | 10711 | `	if( pCatch == 0 ){` |
|        - | 10712 | `		sxi32 rc;` |
|        - | 10713 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 10714 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10715 | `			pException->iFinallyDone = 1;` |
|        3 | 10716 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10717 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10718 | `				return SXERR_ABORT;` |
|        - | 10719 | `			}` |
|        1 | 10720 | `		}` |
|        - | 10721 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 10722 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10723 | `			/* Re-throw to the outer handler */` |
|        3 | 10724 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10725 | `		}` |
|        - | 10726 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10727 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10728 | `		 * exception instead of reporting it uncaught.` |
|        - | 10729 | `		 */` |
|      488 | 10730 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10731 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10732 | `			 * by looking for a catch frame on the stack.` |
|        - | 10733 | `			 */` |
|      488 | 10734 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 10735 | `			int inCatch = 0;` |
|      974 | 10736 | `			while( pF ){` |
|      494 | 10737 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 10738 | `					inCatch = 1;` |
|        7 | 10739 | `					break;` |
|        - | 10740 | `				}` |
|      487 | 10741 | `				pF = pF->pParent;` |
|        1 | 10742 | `			}` |
|      488 | 10743 | `			if( inCatch ){` |
|        - | 10744 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 10745 | `				pThis->iRef++;` |
|        7 | 10746 | `				pVm->pPendingException = pThis;` |
|        7 | 10747 | `				return SXRET_OK;` |
|        - | 10748 | `			}` |
|      240 | 10749 | `		}` |
|        - | 10750 | `		/* Truly uncaught */` |
|      481 | 10751 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 10752 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10753 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10754 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10755 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10756 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10757 | `			}` |
|      ! 0 | 10758 | `		}` |
|      481 | 10759 | `		return rc;` |
|      ! 0 | 10760 | `	}else{` |
|       72 | 10761 | `		VmFrame *pFrame = pVm->pFrame;` |
|       72 | 10762 | `		ph7_exception **apSaved = 0;` |
|        - | 10763 | `		sxu32 nSavedCount;` |
|        - | 10764 | `		sxi32 rc;` |
|       72 | 10765 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       72 | 10766 | `		if( pException->pFrame == pFrame ){` |
|       48 | 10767 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       23 | 10768 | `		}` |
|        - | 10769 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10770 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10771 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10772 | `		 */` |
|       72 | 10773 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       72 | 10774 | `		if( nSavedCount > 0 ){` |
|       13 | 10775 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 10776 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 10777 | `			if( apSaved ){` |
|       13 | 10778 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 10779 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 10780 | `				SySetReset(&pVm->aException);` |
|        4 | 10781 | `			}` |
|        4 | 10782 | `		}` |
|        - | 10783 | `		/* Create a private frame first */` |
|       72 | 10784 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       72 | 10785 | `		if( rc == SXRET_OK ){` |
|       72 | 10786 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       72 | 10787 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       72 | 10788 | `			if( pObj ){` |
|       72 | 10789 | `				pThis->iRef++;` |
|       72 | 10790 | `				pObj->x.pOther = pThis;` |
|       72 | 10791 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       35 | 10792 | `			}` |
|        - | 10793 | `			/* Execute the catch block */` |
|       72 | 10794 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10795 | `			/* Leave the frame */` |
|       72 | 10796 | `			VmLeaveFrame(&(*pVm));` |
|       35 | 10797 | `		}` |
|        - | 10798 | `		/* Restore the outer exception handlers */` |
|       72 | 10799 | `		if( apSaved ){` |
|        - | 10800 | `			sxu32 k;` |
|        - | 10801 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10802 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10803 | `			 * Restore the original outer entries.` |
|        - | 10804 | `			 */` |
|        9 | 10805 | `			SySetReset(&pVm->aException);` |
|       17 | 10806 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 10807 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10808 | `			}` |
|        9 | 10809 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 10810 | `		}` |
|        - | 10811 | `		/* Execute the finally block after catch */` |
|       72 | 10812 | `		if( pException->iHasFinally ){` |
|       16 | 10813 | `			pException->iFinallyDone = 1;` |
|        - | 10814 | `			{` |
|       16 | 10815 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 10816 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10817 | `					return SXERR_ABORT;` |
|        - | 10818 | `				}` |
|        - | 10819 | `			}` |
|        7 | 10820 | `		}` |
|       72 | 10821 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10822 | `			return SXERR_ABORT;` |
|        - | 10823 | `		}` |
|        - | 10824 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10825 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10826 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10827 | `		 */` |
|       72 | 10828 | `		if( pVm->pPendingException ){` |
|        7 | 10829 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 10830 | `			pVm->pPendingException = 0;` |
|        7 | 10831 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10832 | `		}` |
|        - | 10833 | `	}` |
|        - | 10834 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10835 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10836 | `	 */` |
|       66 | 10837 | `	return SXRET_OK;` |
|      281 | 10838 |  |
|        - | 10839 | `/*` |
|        - | 10840 | ` * Section:` |
|        - | 10841 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10842 | ` * Status:` |
|        - | 10843 | ` *    Stable.` |
|        - | 10844 | ` */` |
|        - | 10845 | `/*` |
|        - | 10846 | ` * string ph7version(void)` |
|        - | 10847 | ` *  Returns the running version of the PH7 version.` |
|        - | 10848 | ` * Parameters` |
|        - | 10849 | ` *  None` |
|        - | 10850 | ` * Return` |
|        - | 10851 | ` * Current PH7 version.` |
|        - | 10852 | ` */` |
|        2 | 10853 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10854 |  |
|        1 | 10855 | `	SXUNUSED(nArg);` |
|        1 | 10856 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10857 | `	/* Current engine version */` |
|        3 | 10858 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10859 | `	return PH7_OK;` |
|        1 | 10860 |  |
|        - | 10861 | `/*` |
|        - | 10862 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10863 | ` */` |
|        - | 10864 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10865 | ` "<html><head>"\` |
|        - | 10866 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10867 | ` "<style type=\"text/css\">"\` |
|        - | 10868 | ` "div {"\` |
|        - | 10869 | `     "border: 1px solid #cccccc;"\` |
|        - | 10870 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10871 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10872 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10873 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10874 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10875 | `     "-o-border-radius: 10px;"\` |
|        - | 10876 | `     "border-radius: 10px;"\` |
|        - | 10877 | `     "padding-left: 2em;"\` |
|        - | 10878 | `     "background-color: white;"\` |
|        - | 10879 | `     "margin-left: auto;"\` |
|        - | 10880 | `     "font-family: verdana;"\` |
|        - | 10881 | `     "padding-right: 2em;"\` |
|        - | 10882 | `     "margin-right: auto;"\` |
|        - | 10883 | `     "}"\` |
|        - | 10884 | `     "body {"\` |
|        - | 10885 | `     "padding: 0.2em;"\` |
|        - | 10886 | `     "font-style: normal;"\` |
|        - | 10887 | `     "font-size: medium;"\` |
|        - | 10888 | `     "background-color: #f2f2f2;"\` |
|        - | 10889 | `     "}"\` |
|        - | 10890 | `     "hr {"\` |
|        - | 10891 | `     "border-style: solid none none;"\` |
|        - | 10892 | `     "border-width: 1px medium medium;"\` |
|        - | 10893 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10894 | `     "height: 1px;"\` |
|        - | 10895 | `     "}"\` |
|        - | 10896 | `     "a {"\` |
|        - | 10897 | `     "color: #3366cc;"\` |
|        - | 10898 | `     "text-decoration: none;"\` |
|        - | 10899 | `     "}"\` |
|        - | 10900 | `     "a:hover {"\` |
|        - | 10901 | `     "color: #999999;"\` |
|        - | 10902 | `     "}"\` |
|        - | 10903 | `     "a:active {"\` |
|        - | 10904 | `     "color: #663399;"\` |
|        - | 10905 | `     "}"\` |
|        - | 10906 | `     "h1 {"\` |
|        - | 10907 | `     "margin: 0;"\` |
|        - | 10908 | `     "padding: 0;"\` |
|        - | 10909 | `     "font-family: Verdana;"\` |
|        - | 10910 | `     "font-weight: bold;"\` |
|        - | 10911 | `     "font-style: normal;"\` |
|        - | 10912 | `     "font-size: medium;"\` |
|        - | 10913 | `     "text-transform: capitalize;"\` |
|        - | 10914 | `     "color: #0a328c;"\` |
|        - | 10915 | `     "}"\` |
|        - | 10916 | `     "p {"\` |
|        - | 10917 | `     "margin: 0 auto;"\` |
|        - | 10918 | `     "font-size: medium;"\` |
|        - | 10919 | `     "font-style: normal;"\` |
|        - | 10920 | `     "font-family: verdana;"\` |
|        - | 10921 | `     "}"\` |
|        - | 10922 | `"</style></head><body>"\` |
|        - | 10923 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10924 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10925 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10926 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10927 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10928 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10929 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10930 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10931 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10932 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10933 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10934 |  |
|        - | 10935 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10936 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10937 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10938 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10939 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10940 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10941 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10942 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10943 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10944 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10945 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10946 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10947 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10948 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10949 |  |
|        - | 10950 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10951 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10952 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10953 | `"&nbsp;*<br>"\` |
|        - | 10954 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10955 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10956 | `"&nbsp;* are met:<br>"\` |
|        - | 10957 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10958 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10959 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10960 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10961 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10962 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10963 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10964 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10965 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10966 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10967 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10968 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10969 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10970 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10971 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10972 | `"&nbsp;*<br>"\` |
|        - | 10973 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10974 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10975 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10976 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10977 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10978 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10979 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10980 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10981 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10982 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10983 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10984 | `"&nbsp;*/<br>"\` |
|        - | 10985 | `"</span></small></small></p>"\` |
|        - | 10986 | `"</div></body></html>"` |
|        - | 10987 | `/*` |
|        - | 10988 | ` * bool ph7credits(void)` |
|        - | 10989 | ` * bool ph7info(void)` |
|        - | 10990 | ` * bool ph7copyright(void)` |
|        - | 10991 | ` *  Prints out the credits for PH7 engine` |
|        - | 10992 | ` * Parameters` |
|        - | 10993 | ` *  None` |
|        - | 10994 | ` * Return` |
|        - | 10995 | ` *  Always TRUE` |
|        - | 10996 | ` */` |
|        2 | 10997 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10998 |  |
|        3 | 10999 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 11000 | `	/* Expand the HTML page above*/` |
|        3 | 11001 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 11002 | `	ph7_context_output_format(` |
|        1 | 11003 | `		pCtx,` |
|        - | 11004 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 11005 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 11006 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 11007 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 11008 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 11009 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 11010 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 11011 | `#ifdef __WINNT__` |
|        - | 11012 | `		"Windows NT"` |
|        - | 11013 | `#elif defined(__UNIXES__)` |
|        - | 11014 | `		"UNIX-Like"` |
|        - | 11015 | `#else` |
|        - | 11016 | `		"Other OS"` |
|        - | 11017 | `#endif` |
|        - | 11018 | `		);` |
|        3 | 11019 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 11020 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11021 | `	SXUNUSED(apArg);` |
|        - | 11022 | `	/* Return TRUE */` |
|        - | 11023 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 11024 | `	return PH7_OK;` |
|        1 | 11025 |  |
|        - | 11026 | `/*` |
|        - | 11027 | ` * Section:` |
|        - | 11028 | ` *    URL related routines.` |
|        - | 11029 | ` * Status:` |
|        - | 11030 | ` *    Stable.` |
|        - | 11031 | ` */` |
|        - | 11032 | `/*` |
|        - | 11033 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 11034 | ` *  Parse a URL and return its fields.` |
|        - | 11035 | ` * Parameters` |
|        - | 11036 | ` *  $url` |
|        - | 11037 | ` *   The URL to parse.` |
|        - | 11038 | ` * $component` |
|        - | 11039 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 11040 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 11041 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 11042 | ` *  in which case the return value will be an integer).` |
|        - | 11043 | ` * Return` |
|        - | 11044 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 11045 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 11046 | ` *  this array are:` |
|        - | 11047 | ` *   scheme - e.g. http` |
|        - | 11048 | ` *   host` |
|        - | 11049 | ` *   port` |
|        - | 11050 | ` *   user` |
|        - | 11051 | ` *   pass` |
|        - | 11052 | ` *   path` |
|        - | 11053 | ` *   query - after the question mark ?` |
|        - | 11054 | ` *   fragment - after the hashmark #` |
|        - | 11055 | ` * Note:` |
|        - | 11056 | ` *  FALSE is returned on failure.` |
|        - | 11057 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 11058 | ` *  with the standard PHP engine.` |
|        - | 11059 | ` */` |
|       28 | 11060 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11061 |  |
|        - | 11062 | `	const char *zStr; /* Input string */` |
|        - | 11063 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 11064 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 11065 | `	int nLen;` |
|        - | 11066 | `	sxi32 rc;` |
|       29 | 11067 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11068 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11069 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11070 | `		return PH7_OK;` |
|        - | 11071 | `	}` |
|        - | 11072 | `	/* Extract the given URI */` |
|       29 | 11073 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 11074 | `	if( nLen < 1 ){` |
|        - | 11075 | `		/* Nothing to process,return FALSE */` |
|        3 | 11076 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11077 | `		return PH7_OK;` |
|        - | 11078 | `	}` |
|        - | 11079 | `	/* Get a parse */` |
|       27 | 11080 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 11081 | `	if( rc != SXRET_OK ){` |
|        - | 11082 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 11083 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11084 | `		return PH7_OK;` |
|        - | 11085 | `	}` |
|       27 | 11086 | `	if( nArg > 1 ){` |
|      ! 0 | 11087 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 11088 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 11089 | `		switch(nComponent){` |
|      ! 0 | 11090 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 11091 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 11092 | `			if( pComp->nByte < 1 ){` |
|        - | 11093 | `				/* No available value,return NULL */` |
|      ! 0 | 11094 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11095 | `			}else{` |
|      ! 0 | 11096 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11097 | `			}` |
|      ! 0 | 11098 | `			break;` |
|      ! 0 | 11099 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 11100 | `			pComp = &sURI.sHost;` |
|      ! 0 | 11101 | `			if( pComp->nByte < 1 ){` |
|        - | 11102 | `				/* No available value,return NULL */` |
|      ! 0 | 11103 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11104 | `			}else{` |
|      ! 0 | 11105 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11106 | `			}` |
|      ! 0 | 11107 | `			break;` |
|      ! 0 | 11108 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 11109 | `			pComp = &sURI.sPort;` |
|      ! 0 | 11110 | `			if( pComp->nByte < 1 ){` |
|        - | 11111 | `				/* No available value,return NULL */` |
|      ! 0 | 11112 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11113 | `			}else{` |
|      ! 0 | 11114 | `				int iPort = 0;` |
|        - | 11115 | `				/* Cast the value to integer */` |
|      ! 0 | 11116 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 11117 | `				ph7_result_int(pCtx,iPort);` |
|        - | 11118 | `			}` |
|      ! 0 | 11119 | `			break;` |
|      ! 0 | 11120 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 11121 | `			pComp = &sURI.sUser;` |
|      ! 0 | 11122 | `			if( pComp->nByte < 1 ){` |
|        - | 11123 | `				/* No available value,return NULL */` |
|      ! 0 | 11124 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11125 | `			}else{` |
|      ! 0 | 11126 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11127 | `			}` |
|      ! 0 | 11128 | `			break;` |
|      ! 0 | 11129 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 11130 | `			pComp = &sURI.sPass;` |
|      ! 0 | 11131 | `			if( pComp->nByte < 1 ){` |
|        - | 11132 | `				/* No available value,return NULL */` |
|      ! 0 | 11133 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11134 | `			}else{` |
|      ! 0 | 11135 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11136 | `			}` |
|      ! 0 | 11137 | `			break;` |
|      ! 0 | 11138 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 11139 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 11140 | `			if( pComp->nByte < 1 ){` |
|        - | 11141 | `				/* No available value,return NULL */` |
|      ! 0 | 11142 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11143 | `			}else{` |
|      ! 0 | 11144 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11145 | `			}` |
|      ! 0 | 11146 | `			break;` |
|      ! 0 | 11147 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 11148 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 11149 | `			if( pComp->nByte < 1 ){` |
|        - | 11150 | `				/* No available value,return NULL */` |
|      ! 0 | 11151 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11152 | `			}else{` |
|      ! 0 | 11153 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11154 | `			}` |
|      ! 0 | 11155 | `			break;` |
|      ! 0 | 11156 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 11157 | `			pComp = &sURI.sPath;` |
|      ! 0 | 11158 | `			if( pComp->nByte < 1 ){` |
|        - | 11159 | `				/* No available value,return NULL */` |
|      ! 0 | 11160 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11161 | `			}else{` |
|      ! 0 | 11162 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11163 | `			}` |
|      ! 0 | 11164 | `			break;` |
|      ! 0 | 11165 | `		default:` |
|        - | 11166 | `			/* No such entry,return NULL */` |
|      ! 0 | 11167 | `			ph7_result_null(pCtx);` |
|      ! 0 | 11168 | `			break;` |
|        - | 11169 | `		}` |
|      ! 0 | 11170 | `	}else{` |
|        - | 11171 | `		ph7_value *pArray,*pValue;` |
|        - | 11172 | `		/* Return an associative array */` |
|       27 | 11173 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11174 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11175 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11176 | `			/* Out of memory */` |
|      ! 0 | 11177 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11178 | `			/* Return false */` |
|      ! 0 | 11179 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11180 | `			return PH7_OK;` |
|        - | 11181 | `		}` |
|        - | 11182 | `		/* Fill the array */` |
|       27 | 11183 | `		pComp = &sURI.sScheme;` |
|       27 | 11184 | `		if( pComp->nByte > 0 ){` |
|       19 | 11185 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11186 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11187 | `		}` |
|        - | 11188 | `		/* Reset the string cursor */` |
|       27 | 11189 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11190 | `		pComp = &sURI.sHost;` |
|       27 | 11191 | `		if( pComp->nByte > 0 ){` |
|       25 | 11192 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11193 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11194 | `		}` |
|        - | 11195 | `		/* Reset the string cursor */` |
|       27 | 11196 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11197 | `		pComp = &sURI.sPort;` |
|       27 | 11198 | `		if( pComp->nByte > 0 ){` |
|       11 | 11199 | `			int iPort = 0;/* cc warning */` |
|        - | 11200 | `			/* Convert to integer */` |
|       11 | 11201 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11202 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11203 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11204 | `		}` |
|        - | 11205 | `		/* Reset the string cursor */` |
|       27 | 11206 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11207 | `		pComp = &sURI.sUser;` |
|       27 | 11208 | `		if( pComp->nByte > 0 ){` |
|        7 | 11209 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11210 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11211 | `		}` |
|        - | 11212 | `		/* Reset the string cursor */` |
|       27 | 11213 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11214 | `		pComp = &sURI.sPass;` |
|       27 | 11215 | `		if( pComp->nByte > 0 ){` |
|        7 | 11216 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11217 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11218 | `		}` |
|        - | 11219 | `		/* Reset the string cursor */` |
|       27 | 11220 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11221 | `		pComp = &sURI.sPath;` |
|       27 | 11222 | `		if( pComp->nByte > 0 ){` |
|       17 | 11223 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11224 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11225 | `		}` |
|        - | 11226 | `		/* Reset the string cursor */` |
|       27 | 11227 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11228 | `		pComp = &sURI.sQuery;` |
|       27 | 11229 | `		if( pComp->nByte > 0 ){` |
|        5 | 11230 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11231 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11232 | `		}` |
|        - | 11233 | `		/* Reset the string cursor */` |
|       27 | 11234 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11235 | `		pComp = &sURI.sFragment;` |
|       27 | 11236 | `		if( pComp->nByte > 0 ){` |
|        5 | 11237 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11238 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11239 | `		}` |
|        - | 11240 | `		/* Return the created array */` |
|       27 | 11241 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11242 | `		/* NOTE:` |
|        - | 11243 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11244 | `		 * automatically as soon we return from this function.` |
|        - | 11245 | `		 */` |
|        - | 11246 | `	}` |
|        - | 11247 | `	/* All done */` |
|       27 | 11248 | `	return PH7_OK;` |
|       15 | 11249 |  |
|        - | 11250 | `/*` |
|        - | 11251 | ` * Section:` |
|        - | 11252 | ` *   Array related routines.` |
|        - | 11253 | ` * Status:` |
|        - | 11254 | ` *    Stable.` |
|        - | 11255 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11256 | ` *  Array related functions that need access to the underlying` |
|        - | 11257 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11258 | ` */` |
|        - | 11259 | `/*` |
|        - | 11260 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11261 | ` * of the following structure.` |
|        - | 11262 | ` */` |
|        - | 11263 | `struct compact_data` |
|        - | 11264 |  |
|        - | 11265 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11266 | `	int nRecCount;      /* Recursion count */` |
|        - | 11267 | `};` |
|        - | 11268 | `/*` |
|        - | 11269 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11270 | ` */` |
|      ! 0 | 11271 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11272 |  |
|      ! 0 | 11273 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11274 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11275 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11276 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11277 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11278 | `		SyString sVar;` |
|      ! 0 | 11279 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11280 | `		if( sVar.nByte > 0 ){` |
|        - | 11281 | `			/* Query the current frame */` |
|      ! 0 | 11282 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11283 | `			/* ^` |
|        - | 11284 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11285 | `			 */` |
|      ! 0 | 11286 | `			if( pKey ){` |
|        - | 11287 | `				/* Perform the insertion */` |
|      ! 0 | 11288 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11289 | `			}` |
|      ! 0 | 11290 | `		}` |
|      ! 0 | 11291 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11292 | `		int rc;` |
|        - | 11293 | `		/* Recursively traverse this array */` |
|      ! 0 | 11294 | `		pData->nRecCount++;` |
|      ! 0 | 11295 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11296 | `		pData->nRecCount--;` |
|      ! 0 | 11297 | `		return rc;` |
|        - | 11298 | `	}` |
|      ! 0 | 11299 | `	return SXRET_OK;` |
|      ! 0 | 11300 |  |
|        - | 11301 | `/*` |
|        - | 11302 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11303 | ` *  Create array containing variables and their values.` |
|        - | 11304 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11305 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11306 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11307 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11308 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11309 | ` * Parameters` |
|        - | 11310 | ` *  $varname` |
|        - | 11311 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11312 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11313 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11314 | ` *   it recursively.` |
|        - | 11315 | ` * Return` |
|        - | 11316 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11317 | ` */` |
|        2 | 11318 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11319 |  |
|        - | 11320 | `	ph7_value *pArray,*pObj;` |
|        3 | 11321 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11322 | `	const char *zName;` |
|        - | 11323 | `	SyString sVar;` |
|        - | 11324 | `	int i,nLen;` |
|        3 | 11325 | `	if( nArg < 1 ){` |
|        - | 11326 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11327 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11328 | `		return PH7_OK;` |
|        - | 11329 | `	}` |
|        - | 11330 | `	/* Create the array */` |
|        3 | 11331 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11332 | `	if( pArray == 0 ){` |
|        - | 11333 | `		/* Out of memory */` |
|      ! 0 | 11334 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11335 | `		/* Return NULL */` |
|      ! 0 | 11336 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11337 | `		return PH7_OK;` |
|        - | 11338 | `	}` |
|        - | 11339 | `	/* Perform the requested operation */` |
|        7 | 11340 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11341 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11342 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11343 | `				struct compact_data sData;` |
|      ! 0 | 11344 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11345 | `				/* Recursively walk the array */` |
|      ! 0 | 11346 | `				sData.nRecCount = 0;` |
|      ! 0 | 11347 | `				sData.pArray = pArray;` |
|      ! 0 | 11348 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11349 | `			}` |
|      ! 0 | 11350 | `		}else{` |
|        - | 11351 | `			/* Extract variable name */` |
|        5 | 11352 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11353 | `			if( nLen > 0 ){` |
|        5 | 11354 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11355 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11356 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11357 | `				if( pObj ){` |
|        5 | 11358 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11359 | `				}` |
|        2 | 11360 | `			}` |
|        - | 11361 | `		}` |
|        3 | 11362 | `	}` |
|        - | 11363 | `	/* Return the array */` |
|        3 | 11364 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11365 | `	return PH7_OK;` |
|        2 | 11366 |  |
|        - | 11367 | `/*` |
|        - | 11368 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11369 | ` * of the following structure.` |
|        - | 11370 | ` */` |
|        - | 11371 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11372 | `struct extract_aux_data` |
|        - | 11373 |  |
|        - | 11374 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11375 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11376 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11377 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11378 | `	int iFlags;           /* Control flags */` |
|        - | 11379 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11380 | `};` |
|        - | 11381 | `/* Forward declaration */` |
|        - | 11382 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11383 | `/*` |
|        - | 11384 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11385 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11386 | ` * Parameters` |
|        - | 11387 | ` * $var_array` |
|        - | 11388 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11389 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11390 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11391 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11392 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11393 | ` * $extract_type` |
|        - | 11394 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11395 | ` *  It can be one of the following values:` |
|        - | 11396 | ` *   EXTR_OVERWRITE` |
|        - | 11397 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11398 | ` *   EXTR_SKIP` |
|        - | 11399 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11400 | ` *   EXTR_PREFIX_SAME` |
|        - | 11401 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11402 | ` *   EXTR_PREFIX_ALL` |
|        - | 11403 | ` *       Prefix all variable names with prefix.` |
|        - | 11404 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11405 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11406 | ` *   EXTR_IF_EXISTS` |
|        - | 11407 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11408 | ` *       otherwise do nothing.` |
|        - | 11409 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11410 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11411 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11412 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11413 | ` *      the current symbol table.` |
|        - | 11414 | ` * $prefix` |
|        - | 11415 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11416 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11417 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11418 | ` *  underscore character.` |
|        - | 11419 | ` * Return` |
|        - | 11420 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11421 | ` */` |
|        4 | 11422 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11423 |  |
|        - | 11424 | `	extract_aux_data sAux;` |
|        - | 11425 | `	ph7_hashmap *pMap;` |
|        5 | 11426 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11427 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11428 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11429 | `		return PH7_OK;` |
|        - | 11430 | `	}` |
|        - | 11431 | `	/* Point to the target hashmap */` |
|        5 | 11432 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11433 | `	if( pMap->nEntry < 1 ){` |
|        - | 11434 | `		/* Empty map,return  0 */` |
|      ! 0 | 11435 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11436 | `		return PH7_OK;` |
|        - | 11437 | `	}` |
|        - | 11438 | `	/* Prepare the aux data */` |
|        5 | 11439 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11440 | `	if( nArg > 1 ){` |
|        3 | 11441 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11442 | `		if( nArg > 2 ){` |
|      ! 0 | 11443 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11444 | `		}` |
|        1 | 11445 | `	}` |
|        5 | 11446 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11447 | `	/* Invoke the worker callback */` |
|        5 | 11448 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11449 | `	/* Number of variables successfully imported */` |
|        5 | 11450 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11451 | `	return PH7_OK;` |
|        3 | 11452 |  |
|        - | 11453 | `/*` |
|        - | 11454 | ` * Worker callback for the [extract()] function defined` |
|        - | 11455 | ` * below.` |
|        - | 11456 | ` */` |
|        8 | 11457 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11458 |  |
|        9 | 11459 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11460 | `	int iFlags = pAux->iFlags;` |
|        9 | 11461 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11462 | `	ph7_value *pObj;` |
|        - | 11463 | `	SyString sVar;` |
|        9 | 11464 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11465 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11466 | `	}` |
|        - | 11467 | `	/* Perform a string cast */` |
|        9 | 11468 | `	PH7_MemObjToString(pKey);` |
|        9 | 11469 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11470 | `		/* Unavailable variable name */` |
|      ! 0 | 11471 | `		return SXRET_OK;` |
|        - | 11472 | `	}` |
|        9 | 11473 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11474 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11475 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11476 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11477 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11478 | `			);` |
|      ! 0 | 11479 | `	}else{` |
|       13 | 11480 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11481 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11482 | `	}` |
|        9 | 11483 | `	sVar.zString = pAux->zWorker;` |
|        - | 11484 | `	/* Try to extract the variable */` |
|        9 | 11485 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11486 | `	if( pObj ){` |
|        - | 11487 | `		/* Collision */` |
|        5 | 11488 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11489 | `			return SXRET_OK;` |
|        - | 11490 | `		}` |
|        5 | 11491 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11492 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11493 | `				/* Already prefixed */` |
|      ! 0 | 11494 | `				return SXRET_OK;` |
|        - | 11495 | `			}` |
|      ! 0 | 11496 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11497 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11498 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11499 | `				);` |
|      ! 0 | 11500 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11501 | `		}` |
|        3 | 11502 | `	}else{` |
|        - | 11503 | `		/* Create the variable */` |
|        5 | 11504 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11505 | `	}` |
|        9 | 11506 | `	if( pObj ){` |
|        - | 11507 | `		/* Overwrite the old value */` |
|        9 | 11508 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11509 | `		/* Increment counter */` |
|        9 | 11510 | `		pAux->iCount++;` |
|        4 | 11511 | `	}` |
|        9 | 11512 | `	return SXRET_OK;` |
|        5 | 11513 |  |
|        - | 11514 | `/*` |
|        - | 11515 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11516 | ` * defined below.` |
|        - | 11517 | ` */` |
|        2 | 11518 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11519 |  |
|        3 | 11520 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11521 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11522 | `	ph7_value *pObj;` |
|        - | 11523 | `	SyString sVar;` |
|        - | 11524 | `	/* Perform a string cast */` |
|        3 | 11525 | `	PH7_MemObjToString(pKey);` |
|        3 | 11526 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11527 | `		/* Unavailable variable name */` |
|      ! 0 | 11528 | `		return SXRET_OK;` |
|        - | 11529 | `	}` |
|        3 | 11530 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11531 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11532 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11533 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11534 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11535 | `			);` |
|        2 | 11536 | `	}else{` |
|      ! 0 | 11537 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11538 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11539 | `	}` |
|        3 | 11540 | `	sVar.zString = pAux->zWorker;` |
|        - | 11541 | `	/* Extract the variable */` |
|        3 | 11542 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11543 | `	if( pObj ){` |
|        3 | 11544 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11545 | `	}` |
|        3 | 11546 | `	return SXRET_OK;` |
|        2 | 11547 |  |
|        - | 11548 | `/*` |
|        - | 11549 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11550 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11551 | ` * Parameters` |
|        - | 11552 | ` * $types` |
|        - | 11553 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11554 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11555 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11556 | ` *  POST includes the POST uploaded file information.` |
|        - | 11557 | ` *  Note:` |
|        - | 11558 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11559 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11560 | ` * $prefix` |
|        - | 11561 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11562 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11563 | ` *  variable named $pref_userid.` |
|        - | 11564 | ` * Return` |
|        - | 11565 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11566 | ` */` |
|        2 | 11567 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11568 |  |
|        - | 11569 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11570 | `	extract_aux_data sAux;` |
|        - | 11571 | `	int nLen,nPrefixLen;` |
|        - | 11572 | `	ph7_value *pSuper;` |
|        - | 11573 | `	ph7_vm *pVm;` |
|        - | 11574 | `	/* By default import only $_GET variables  */` |
|        3 | 11575 | `	zImport = "G";` |
|        3 | 11576 | `	nLen = (int)sizeof(char);` |
|        3 | 11577 | `	zPrefix = 0;` |
|        3 | 11578 | `	nPrefixLen = 0;` |
|        3 | 11579 | `	if( nArg > 0 ){` |
|        3 | 11580 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11581 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11582 | `		}` |
|        3 | 11583 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11584 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11585 | `		}` |
|        1 | 11586 | `	}` |
|        - | 11587 | `	/* Point to the underlying VM */` |
|        3 | 11588 | `	pVm = pCtx->pVm;` |
|        - | 11589 | `	/* Initialize the aux data */` |
|        3 | 11590 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11591 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11592 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11593 | `	sAux.pVm = pVm;` |
|        - | 11594 | `	/* Extract */` |
|        3 | 11595 | `	zEnd = &zImport[nLen];` |
|        5 | 11596 | `	while( zImport < zEnd ){` |
|        3 | 11597 | `		int c = zImport[0];` |
|        3 | 11598 | `		pSuper = 0;` |
|        3 | 11599 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11600 | `			/* Import $_GET variables */` |
|        3 | 11601 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11602 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11603 | `			/* Import $_POST variables */` |
|      ! 0 | 11604 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11605 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11606 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11607 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11608 | `		}` |
|        3 | 11609 | `		if( pSuper ){` |
|        - | 11610 | `			/* Iterate throw array entries */` |
|        3 | 11611 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11612 | `		}` |
|        - | 11613 | `		/* Advance the cursor */` |
|        3 | 11614 | `		zImport++;` |
|        1 | 11615 | `	}` |
|        - | 11616 | `	/* All done,return TRUE*/` |
|        3 | 11617 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11618 | `	return PH7_OK;` |
|        1 | 11619 |  |
|        - | 11620 | `/*` |
|        - | 11621 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11622 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11623 | ` * information.` |
|        - | 11624 | ` */` |
|    10954 | 11625 | `static sxi32 VmEvalChunk(` |
|        - | 11626 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11627 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11628 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11629 | `	int iFlags,         /* Compile flag */` |
|        - | 11630 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11631 | `	)` |
|        2 | 11632 |  |
|        - | 11633 | `	SySet *pByteCode,aByteCode;` |
|        - | 11634 | `	SyBlob sSavedNs;` |
|    10956 | 11635 | `	ProcConsumer xErr = 0;` |
|    10956 | 11636 | `	void *pErrData = 0;` |
|        - | 11637 | `	/* Initialize bytecode container */` |
|    10956 | 11638 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10956 | 11639 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11640 | `	/* Reset the code generator */` |
|    10956 | 11641 | `	if( bTrueReturn ){` |
|        - | 11642 | `		/* Included file,log compile-time errors */` |
|     8280 | 11643 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8280 | 11644 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4139 | 11645 | `	}` |
|    10956 | 11646 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11647 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11648 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11649 | `	 * the caller's namespace is restored. */` |
|    10956 | 11650 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10956 | 11651 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10956 | 11652 | `	if( bTrueReturn ){` |
|        - | 11653 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8280 | 11654 | `		SyBlobReset(&pVm->sNamespace);` |
|     4139 | 11655 | `	}` |
|        - | 11656 | `	/* Swap bytecode container */` |
|    10956 | 11657 | `	pByteCode = pVm->pByteContainer;` |
|    10956 | 11658 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11659 | `	/* Compile the chunk */` |
|    10956 | 11660 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    16433 | 11661 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11662 | `		/* Compilation error,return false */` |
|        3 | 11663 | `		if( pCtx ){` |
|        3 | 11664 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11665 | `		}` |
|        2 | 11666 | `	}else{` |
|        - | 11667 | `		/* Mount any newly defined classes */` |
|        - | 11668 | `		SyHashEntry *pEntry;` |
|        - | 11669 | `		ph7_class *pClass;` |
|        - | 11670 | `		ph7_value sResult; /* Return value */` |
|        - | 11671 | `		sxi32 rc;` |
|    10954 | 11672 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   412494 | 11673 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   396066 | 11674 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11675 | `			/* Only mount classes that haven't been mounted yet */` |
|   396066 | 11676 | `			if( !pClass->bMounted ){` |
|    84550 | 11677 | `				rc = VmMountUserClass(pVm,pClass);` |
|    84550 | 11678 | `				if( rc != SXRET_OK ){` |
|        - | 11679 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11680 | `					if( pCtx ){` |
|      ! 0 | 11681 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11682 | `					}` |
|      ! 0 | 11683 | `					goto Cleanup;` |
|        - | 11684 | `				}` |
|    42274 | 11685 | `			}` |
|        2 | 11686 | `		}` |
|    10954 | 11687 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11688 | `			/* Out of memory */` |
|      ! 0 | 11689 | `			if( pCtx ){` |
|      ! 0 | 11690 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11691 | `			}` |
|      ! 0 | 11692 | `			goto Cleanup;` |
|        - | 11693 | `		}` |
|    10954 | 11694 | `		if( bTrueReturn ){` |
|        - | 11695 | `			/* Assume a boolean true return value */` |
|     8280 | 11696 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4141 | 11697 | `		}else{` |
|        - | 11698 | `			/* Assume a null return value */` |
|     2676 | 11699 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11700 | `		}` |
|        - | 11701 | `		/* Execute the compiled chunk */` |
|    10954 | 11702 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10954 | 11703 | `		if( pCtx ){` |
|        - | 11704 | `			/* Set the execution result */` |
|     8298 | 11705 | `			ph7_result_value(pCtx,&sResult);` |
|     4148 | 11706 | `		}` |
|    10954 | 11707 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11708 | `	}` |
|     5477 | 11709 | `Cleanup:` |
|        - | 11710 | `	/* Cleanup the mess left behind */` |
|    10956 | 11711 | `	pVm->pByteContainer = pByteCode;` |
|    10956 | 11712 | `	SySetRelease(&aByteCode);` |
|        - | 11713 | `	/* Restore caller's namespace state */` |
|    10956 | 11714 | `	SyBlobReset(&pVm->sNamespace);` |
|    10956 | 11715 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10956 | 11716 | `	SyBlobRelease(&sSavedNs);` |
|    10956 | 11717 | `	return SXRET_OK;` |
|        2 | 11718 |  |
|        - | 11719 | `/*` |
|        - | 11720 | ` * value eval(string $code)` |
|        - | 11721 | ` *   Evaluate a string as PHP code.` |
|        - | 11722 | ` * Parameter` |
|        - | 11723 | ` *  code: PHP code to evaluate.` |
|        - | 11724 | ` * Return` |
|        - | 11725 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11726 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11727 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11728 | ` */` |
|       22 | 11729 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11730 |  |
|        - | 11731 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11732 | `	if( nArg < 1 ){` |
|        - | 11733 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11734 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11735 | `		return SXRET_OK;` |
|        - | 11736 | `	}` |
|        - | 11737 | `	/* Chunk to evaluate */` |
|       24 | 11738 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11739 | `	if( sChunk.nByte < 1 ){` |
|        - | 11740 | `		/* Empty string,return NULL */` |
|        3 | 11741 | `		ph7_result_null(pCtx);` |
|        3 | 11742 | `		return SXRET_OK;` |
|        - | 11743 | `	}` |
|        - | 11744 | `	/* Eval the chunk */` |
|       22 | 11745 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11746 | `	return SXRET_OK;` |
|       13 | 11747 |  |
|        - | 11748 | `/*` |
|        - | 11749 | ` * Check if a file path is already included.` |
|        - | 11750 | ` */` |
|    16552 | 11751 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11752 |  |
|        - | 11753 | `	SyString *aEntries;` |
|        - | 11754 | `	sxu32 n;` |
|    16554 | 11755 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11756 | `	/* Perform a linear search */` |
| 68446144 | 11757 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 68429598 | 11758 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11759 | `			/* Already included */` |
|        7 | 11760 | `			return TRUE;` |
|        - | 11761 | `		}` |
| 34214797 | 11762 | `	}` |
|    16548 | 11763 | `	return FALSE;` |
|     8278 | 11764 |  |
|        - | 11765 | `/*` |
|        - | 11766 | ` * Push a file path in the appropriate VM container.` |
|        - | 11767 | ` */` |
|    19200 | 11768 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11769 |  |
|        - | 11770 | `	SyString sPath;` |
|        - | 11771 | `	char *zDup;` |
|        - | 11772 | `#ifdef __WINNT__` |
|        - | 11773 | `	char *zCur;` |
|        - | 11774 | `#endif` |
|        - | 11775 | `	sxi32 rc;` |
|    19202 | 11776 | `	if( nLen < 0 ){` |
|     2650 | 11777 | `		nLen = SyStrlen(zPath);` |
|     1324 | 11778 | `	}` |
|        - | 11779 | `	/* Duplicate the file path first */` |
|    19202 | 11780 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    19202 | 11781 | `	if( zDup == 0 ){` |
|      ! 0 | 11782 | `		return SXERR_MEM;` |
|        - | 11783 | `	}` |
|        - | 11784 | `#ifdef __WINNT__` |
|        - | 11785 | `	/* Normalize path on windows` |
|        - | 11786 | `	 * Example:` |
|        - | 11787 | `	 *    Path/To/File.php` |
|        - | 11788 | `	 * becomes` |
|        - | 11789 | `	 *   path\to\file.php` |
|        - | 11790 | `	 */` |
|        2 | 11791 | `	zCur = zDup;` |
|        2 | 11792 | `	while( zCur[0] != 0 ){` |
|        2 | 11793 | `		if( zCur[0] == '/' ){` |
|        2 | 11794 | `			zCur[0] = '\\';` |
|        2 | 11795 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11796 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11797 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11798 | `		}` |
|        2 | 11799 | `		zCur++;` |
|        2 | 11800 | `	}` |
|        - | 11801 | `#endif` |
|        - | 11802 | `	/* Install the file path */` |
|    19202 | 11803 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    19202 | 11804 | `	if( !bMain ){` |
|    16554 | 11805 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11806 | `			/* Already included */` |
|        7 | 11807 | `			*pNew = 0;` |
|        4 | 11808 | `		}else{` |
|        - | 11809 | `			/* Insert in the corresponding container */` |
|    16548 | 11810 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16548 | 11811 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11812 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11813 | `				return rc;` |
|        - | 11814 | `			}` |
|    16548 | 11815 | `			*pNew = 1;` |
|        - | 11816 | `		}` |
|     8276 | 11817 | `	}` |
|    19202 | 11818 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    19202 | 11819 | `	return SXRET_OK;` |
|     9602 | 11820 |  |
|        - | 11821 | `/*` |
|        - | 11822 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11823 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11824 | ` * indicates failure.` |
|        - | 11825 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11826 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11827 | ` * operations.` |
|        - | 11828 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11829 | ` * this function is a no-op.` |
|        - | 11830 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11831 | ` * constructs for more information.` |
|        - | 11832 | ` */` |
|     8288 | 11833 | `static sxi32 VmExecIncludedFile(` |
|        - | 11834 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11835 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11836 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11837 | `	 )` |
|        2 | 11838 |  |
|        - | 11839 | `	sxi32 rc;` |
|        - | 11840 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11841 | `	const ph7_io_stream *pStream;` |
|        - | 11842 | `	SyBlob sContents;` |
|        - | 11843 | `	void *pHandle;` |
|        - | 11844 | `	ph7_vm *pVm;` |
|        - | 11845 | `	int isNew;` |
|        - | 11846 | `	/* Initialize fields */` |
|     8290 | 11847 | `	pVm = pCtx->pVm;` |
|     8290 | 11848 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8290 | 11849 | `	isNew = 0;` |
|        - | 11850 | `	/* Extract the associated stream */` |
|     8290 | 11851 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11852 | `	/*` |
|        - | 11853 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11854 | `	 * in a read-only mode.` |
|        - | 11855 | `	 */` |
|     8290 | 11856 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8290 | 11857 | `	if( pHandle == 0 ){` |
|        8 | 11858 | `		return SXERR_IO;` |
|        - | 11859 | `	}` |
|     8284 | 11860 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8284 | 11861 | `	if( IncludeOnce && !isNew ){` |
|        - | 11862 | `		/* Already included */` |
|        5 | 11863 | `		rc = SXERR_EXISTS;` |
|        3 | 11864 | `	}else{` |
|        - | 11865 | `		/* Read the whole file contents */` |
|     8280 | 11866 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8280 | 11867 | `		if( rc == SXRET_OK ){` |
|        - | 11868 | `			SyString sScript;` |
|        - | 11869 | `			/* Compile and execute the script */` |
|     8280 | 11870 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8280 | 11871 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4139 | 11872 | `		}` |
|        - | 11873 | `	}` |
|        - | 11874 | `	/* Pop from the set of included file */` |
|     8284 | 11875 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11876 | `	/* Close the handle */` |
|     8284 | 11877 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11878 | `	/* Release the working buffer */` |
|     8284 | 11879 | `	SyBlobRelease(&sContents);` |
|        - | 11880 | `#else` |
|        - | 11881 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11882 | `	SXUNUSED(pPath);` |
|        - | 11883 | `	SXUNUSED(IncludeOnce);` |
|        - | 11884 | `	rc = SXERR_IO;` |
|        - | 11885 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8284 | 11886 | `	return rc;` |
|     4146 | 11887 |  |
|        - | 11888 | `/*` |
|        - | 11889 | ` * string get_include_path(void)` |
|        - | 11890 | ` *  Gets the current include_path configuration option.` |
|        - | 11891 | ` * Parameter` |
|        - | 11892 | ` *  None` |
|        - | 11893 | ` * Return` |
|        - | 11894 | ` *  Included paths as a string` |
|        - | 11895 | ` */` |
|        2 | 11896 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11897 |  |
|        3 | 11898 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11899 | `	SyString *aEntry;` |
|        - | 11900 | `	int dir_sep;` |
|        - | 11901 | `	sxu32 n;` |
|        - | 11902 | `#ifdef __WINNT__` |
|        1 | 11903 | `	dir_sep = ';';` |
|        - | 11904 | `#else` |
|        - | 11905 | `	/* Assume UNIX path separator */` |
|        2 | 11906 | `	dir_sep = ':';` |
|        - | 11907 | `#endif` |
|        1 | 11908 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11909 | `	SXUNUSED(apArg);` |
|        - | 11910 | `	/* Point to the list of import paths */` |
|        3 | 11911 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11912 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11913 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11914 | `		if( n > 0 ){` |
|        - | 11915 | `			/* Append dir seprator */` |
|      ! 0 | 11916 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11917 | `		}` |
|        - | 11918 | `		/* Append path */` |
|        3 | 11919 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11920 | `	}` |
|        3 | 11921 | `	return PH7_OK;` |
|        1 | 11922 |  |
|        - | 11923 | `/*` |
|        - | 11924 | ` * string get_get_included_files(void)` |
|        - | 11925 | ` *  Gets the current include_path configuration option.` |
|        - | 11926 | ` * Parameter` |
|        - | 11927 | ` *  None` |
|        - | 11928 | ` * Return` |
|        - | 11929 | ` *  Included paths as a string` |
|        - | 11930 | ` */` |
|        2 | 11931 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11932 |  |
|        3 | 11933 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11934 | `	ph7_value *pArray,*pWorker;` |
|        - | 11935 | `	SyString *pEntry;` |
|        - | 11936 | `	int c,d;` |
|        - | 11937 | `	/* Create an array and a working value */` |
|        3 | 11938 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11939 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11940 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11941 | `		/* Out of memory,return null */` |
|      ! 0 | 11942 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11943 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11944 | `		SXUNUSED(apArg);` |
|      ! 0 | 11945 | `		return PH7_OK;` |
|        - | 11946 | `	}` |
|        3 | 11947 | `	c = d = '/';` |
|        - | 11948 | `#ifdef __WINNT__` |
|        1 | 11949 | `	d = '\\';` |
|        - | 11950 | `#endif` |
|        - | 11951 | `	/* Iterate throw entries */` |
|        3 | 11952 | `	SySetResetCursor(pFiles);` |
|     3839 | 11953 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11954 | `		const char *zBase,*zEnd;` |
|        - | 11955 | `		int iLen;` |
|        - | 11956 | `		/* reset the string cursor */` |
|     3837 | 11957 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11958 | `		/* Extract base name */` |
|     3837 | 11959 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11960 | `		/* Ignore trailing '/' */` |
|     5755 | 11961 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11962 | `			zEnd--;` |
|      ! 0 | 11963 | `		}` |
|     3837 | 11964 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 11965 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 11966 | `			zEnd--;` |
|        1 | 11967 | `		}` |
|     3837 | 11968 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 11969 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11970 | `		/* Copy entry name */` |
|     3837 | 11971 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11972 | `		/* Perform the insertion */` |
|     3837 | 11973 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11974 | `	}` |
|        - | 11975 | `	/* All done,return the created array */` |
|        3 | 11976 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11977 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11978 | `	 * by the engine as soon we return from this foreign` |
|        - | 11979 | `	 * function.` |
|        - | 11980 | `	 */` |
|        3 | 11981 | `	return PH7_OK;` |
|        2 | 11982 |  |
|        - | 11983 | `/*` |
|        - | 11984 | ` * include:` |
|        - | 11985 | ` * According to the PHP reference manual.` |
|        - | 11986 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11987 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11988 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11989 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11990 | ` *  and the current working directory before failing. The include()` |
|        - | 11991 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11992 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11993 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11994 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11995 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11996 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11997 | ` *  directory to find the requested file.` |
|        - | 11998 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11999 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 12000 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 12001 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 12002 | ` */` |
|     8270 | 12003 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12004 |  |
|        - | 12005 | `	SyString sFile;` |
|        - | 12006 | `	sxi32 rc;` |
|     8272 | 12007 | `	if( nArg < 1 ){` |
|        - | 12008 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12009 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12010 | `		return SXRET_OK;` |
|        - | 12011 | `	}` |
|        - | 12012 | `	/* File to include */` |
|     8272 | 12013 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8272 | 12014 | `	if( sFile.nByte < 1 ){` |
|        - | 12015 | `		/* Empty string,return NULL */` |
|      ! 0 | 12016 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12017 | `		return SXRET_OK;` |
|        - | 12018 | `	}` |
|        - | 12019 | `	/* Open,compile and execute the desired script */` |
|     8272 | 12020 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8272 | 12021 | `	if( rc != SXRET_OK ){` |
|        - | 12022 | `		/* Emit a warning and return false */` |
|        3 | 12023 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 12024 | `		ph7_result_bool(pCtx,0);` |
|        1 | 12025 | `	}` |
|     8272 | 12026 | `	return SXRET_OK;` |
|     4137 | 12027 |  |
|        - | 12028 | `/*` |
|        - | 12029 | ` * include_once:` |
|        - | 12030 | ` *  According to the PHP reference manual.` |
|        - | 12031 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 12032 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 12033 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 12034 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 12035 | ` *   just once.` |
|        - | 12036 | ` */` |
|        4 | 12037 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12038 |  |
|        - | 12039 | `	SyString sFile;` |
|        - | 12040 | `	sxi32 rc;` |
|        5 | 12041 | `	if( nArg < 1 ){` |
|        - | 12042 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12043 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12044 | `		return SXRET_OK;` |
|        - | 12045 | `	}` |
|        - | 12046 | `	/* File to include */` |
|        5 | 12047 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12048 | `	if( sFile.nByte < 1 ){` |
|        - | 12049 | `		/* Empty string,return NULL */` |
|      ! 0 | 12050 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12051 | `		return SXRET_OK;` |
|        - | 12052 | `	}` |
|        - | 12053 | `	/* Open,compile and execute the desired script */` |
|        5 | 12054 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12055 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12056 | `		/* File already included,return TRUE */` |
|        3 | 12057 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12058 | `		return SXRET_OK;` |
|        - | 12059 | `	}` |
|        3 | 12060 | `	if( rc != SXRET_OK ){` |
|        - | 12061 | `		/* Emit a warning and return false */` |
|      ! 0 | 12062 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12063 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12064 | ` 	}` |
|        3 | 12065 | `	return SXRET_OK;` |
|        3 | 12066 |  |
|        - | 12067 | `/*` |
|        - | 12068 | ` * require.` |
|        - | 12069 | ` *  According to the PHP reference manual.` |
|        - | 12070 | ` *   require() is identical to include() except upon failure it will` |
|        - | 12071 | ` *   also produce a fatal level error.` |
|        - | 12072 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 12073 | ` *   emits a warning  which allows the script to continue.` |
|        - | 12074 | ` */` |
|        6 | 12075 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12076 |  |
|        - | 12077 | `	SyString sFile;` |
|        - | 12078 | `	sxi32 rc;` |
|        8 | 12079 | `	if( nArg < 1 ){` |
|        - | 12080 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12081 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12082 | `		return SXRET_OK;` |
|        - | 12083 | `	}` |
|        - | 12084 | `	/* File to include */` |
|        8 | 12085 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 12086 | `	if( sFile.nByte < 1 ){` |
|        - | 12087 | `		/* Empty string,return NULL */` |
|      ! 0 | 12088 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12089 | `		return SXRET_OK;` |
|        - | 12090 | `	}` |
|        - | 12091 | `	/* Open,compile and execute the desired script */` |
|        8 | 12092 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 12093 | `	if( rc != SXRET_OK ){` |
|        - | 12094 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12095 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12096 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12097 | `		return PH7_ABORT;` |
|        - | 12098 | `	}` |
|        8 | 12099 | `	return SXRET_OK;` |
|        5 | 12100 |  |
|        - | 12101 | `/*` |
|        - | 12102 | ` * require_once:` |
|        - | 12103 | ` *  According to the PHP reference manual.` |
|        - | 12104 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 12105 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 12106 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 12107 | ` *   and how it differs from its non _once siblings.` |
|        - | 12108 | ` */` |
|        4 | 12109 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12110 |  |
|        - | 12111 | `	SyString sFile;` |
|        - | 12112 | `	sxi32 rc;` |
|        5 | 12113 | `	if( nArg < 1 ){` |
|        - | 12114 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12115 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12116 | `		return SXRET_OK;` |
|        - | 12117 | `	}` |
|        - | 12118 | `	/* File to include */` |
|        5 | 12119 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12120 | `	if( sFile.nByte < 1 ){` |
|        - | 12121 | `		/* Empty string,return NULL */` |
|      ! 0 | 12122 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12123 | `		return SXRET_OK;` |
|        - | 12124 | `	}` |
|        - | 12125 | `	/* Open,compile and execute the desired script */` |
|        5 | 12126 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12127 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12128 | `		/* File already included,return TRUE */` |
|        3 | 12129 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12130 | `		return SXRET_OK;` |
|        - | 12131 | `	}` |
|        3 | 12132 | `	if( rc != SXRET_OK ){` |
|        - | 12133 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12134 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12135 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12136 | `		return PH7_ABORT;` |
|        - | 12137 | `	}` |
|        3 | 12138 | `	return SXRET_OK;` |
|        3 | 12139 |  |
|        - | 12140 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 12141 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 12142 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 12143 | `/*` |
|        - | 12144 | ` * Section:` |
|        - | 12145 | ` *  SPL Autoloading functions.` |
|        - | 12146 | ` * Status:` |
|        - | 12147 | ` *  Stable.` |
|        - | 12148 | ` */` |
|        - | 12149 | `/*` |
|        - | 12150 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 12151 | ` *  Register given function as __autoload() implementation.` |
|        - | 12152 | ` * Parameters` |
|        - | 12153 | ` *  callback` |
|        - | 12154 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 12155 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 12156 | ` *  throw` |
|        - | 12157 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 12158 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 12159 | ` *  prepend` |
|        - | 12160 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 12161 | ` *   autoload stack instead of appending it.` |
|        - | 12162 | ` * Return` |
|        - | 12163 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12164 | ` */` |
|       34 | 12165 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12166 |  |
|        - | 12167 | `	VmAutoloadCB sEntry;` |
|       36 | 12168 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 12169 | `	int iPrepend = 0;` |
|        - | 12170 | `	sxu32 n;` |
|       36 | 12171 | `	if( nArg < 1 ){` |
|        - | 12172 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12173 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12174 | `		/* Check for duplicates first */` |
|        9 | 12175 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12176 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12177 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12178 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12179 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12180 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12181 | `				return SXRET_OK;` |
|        - | 12182 | `			}` |
|      ! 0 | 12183 | `		}` |
|        5 | 12184 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12185 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12186 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12187 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12188 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12189 | `		return SXRET_OK;` |
|        - | 12190 | `	}` |
|        - | 12191 | `	/* Validate that the callback is callable */` |
|       28 | 12192 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12193 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12194 | `		if( nArg >= 2 ){` |
|      ! 0 | 12195 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12196 | `		}` |
|      ! 0 | 12197 | `		if( iThrow ){` |
|      ! 0 | 12198 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12199 | `				"Argument is not callable");` |
|      ! 0 | 12200 | `		}` |
|      ! 0 | 12201 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12202 | `		return SXRET_OK;` |
|        - | 12203 | `	}` |
|        - | 12204 | `	/* Check for duplicates */` |
|       46 | 12205 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12206 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12207 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12208 | `			/* Already registered */` |
|      ! 0 | 12209 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12210 | `			return SXRET_OK;` |
|        - | 12211 | `		}` |
|       11 | 12212 | `	}` |
|        - | 12213 | `	/* Check prepend flag */` |
|       28 | 12214 | `	if( nArg >= 3 ){` |
|        3 | 12215 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12216 | `	}` |
|        - | 12217 | `	/* Store the callback */` |
|       28 | 12218 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12219 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12220 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12221 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12222 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12223 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12224 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12225 | `		VmAutoloadCB *aBase;` |
|        3 | 12226 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12227 | `		/* Rotate: move last entry to front */` |
|        3 | 12228 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12229 | `		if( aBase ){` |
|        - | 12230 | `			VmAutoloadCB sTemp;` |
|        - | 12231 | `			sxu32 i;` |
|        3 | 12232 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12233 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12234 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12235 | `			}` |
|        3 | 12236 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12237 | `		}` |
|        2 | 12238 | `	}else{` |
|       26 | 12239 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12240 | `	}` |
|       28 | 12241 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12242 | `	return SXRET_OK;` |
|       19 | 12243 |  |
|        - | 12244 | `/*` |
|        - | 12245 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12246 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12247 | ` * Parameters` |
|        - | 12248 | ` *  callback` |
|        - | 12249 | ` *   The autoload function being unregistered.` |
|        - | 12250 | ` * Return` |
|        - | 12251 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12252 | ` */` |
|       32 | 12253 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12254 |  |
|       34 | 12255 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12256 | `	sxu32 n,nEntry;` |
|       34 | 12257 | `	if( nArg < 1 ){` |
|      ! 0 | 12258 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12259 | `		return SXRET_OK;` |
|        - | 12260 | `	}` |
|       34 | 12261 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12262 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12263 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12264 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12265 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12266 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12267 | `			sxu32 i;` |
|       32 | 12268 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12269 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12270 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12271 | `			}` |
|        - | 12272 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12273 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12274 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12275 | `			return SXRET_OK;` |
|        - | 12276 | `		}` |
|        3 | 12277 | `	}` |
|        3 | 12278 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12279 | `	return SXRET_OK;` |
|       18 | 12280 |  |
|        - | 12281 | `/*` |
|        - | 12282 | ` * array spl_autoload_functions(void)` |
|        - | 12283 | ` *  Return all registered __autoload() functions.` |
|        - | 12284 | ` * Return` |
|        - | 12285 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12286 | ` *  an empty array is returned.` |
|        - | 12287 | ` */` |
|       20 | 12288 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12289 |  |
|       21 | 12290 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12291 | `	ph7_value *pArray;` |
|        - | 12292 | `	sxu32 n,nEntry;` |
|       10 | 12293 | `	SXUNUSED(nArg);` |
|       10 | 12294 | `	SXUNUSED(apArg);` |
|       21 | 12295 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12296 | `	if( pArray == 0 ){` |
|      ! 0 | 12297 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12298 | `		return SXRET_OK;` |
|        - | 12299 | `	}` |
|       21 | 12300 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12301 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12302 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12303 | `		if( pEntry ){` |
|       15 | 12304 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12305 | `		}` |
|        8 | 12306 | `	}` |
|       21 | 12307 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12308 | `	return SXRET_OK;` |
|       11 | 12309 |  |
|        - | 12310 | `/*` |
|        - | 12311 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12312 | ` *  Default implementation of __autoload().` |
|        - | 12313 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12314 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12315 | ` * Parameters` |
|        - | 12316 | ` *  class` |
|        - | 12317 | ` *   The class name being searched.` |
|        - | 12318 | ` *  file_extensions` |
|        - | 12319 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12320 | ` */` |
|        2 | 12321 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12322 |  |
|        - | 12323 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12324 | `	SyBlob sPath;` |
|        - | 12325 | `	int nClass;` |
|        - | 12326 | `	sxi32 rc;` |
|        3 | 12327 | `	if( nArg < 1 ){` |
|      ! 0 | 12328 | `		return SXRET_OK;` |
|        - | 12329 | `	}` |
|        3 | 12330 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12331 | `	if( nClass < 1 ){` |
|      ! 0 | 12332 | `		return SXRET_OK;` |
|        - | 12333 | `	}` |
|        - | 12334 | `	/* Default extensions */` |
|        3 | 12335 | `	zExt = ".php,.inc";` |
|        3 | 12336 | `	if( nArg >= 2 ){` |
|        - | 12337 | `		int nExt;` |
|      ! 0 | 12338 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12339 | `		if( nExt < 1 ){` |
|      ! 0 | 12340 | `			zExt = ".php,.inc";` |
|      ! 0 | 12341 | `		}` |
|      ! 0 | 12342 | `	}` |
|        3 | 12343 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12344 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12345 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12346 | `	zCur = zExt;` |
|        7 | 12347 | `	while( zCur < zEnd ){` |
|        - | 12348 | `		const char *zComma;` |
|        - | 12349 | `		SyString sFile;` |
|        - | 12350 | `		int i;` |
|        - | 12351 | `		/* Find next comma or end */` |
|        5 | 12352 | `		zComma = zCur;` |
|       21 | 12353 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12354 | `			zComma++;` |
|        1 | 12355 | `		}` |
|        - | 12356 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12357 | `		SyBlobReset(&sPath);` |
|       69 | 12358 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12359 | `			char c = zClass[i];` |
|       65 | 12360 | `			if( c == '\\' ){` |
|      ! 0 | 12361 | `				c = '/';` |
|       65 | 12362 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12363 | `				c = c + ('a' - 'A');` |
|        6 | 12364 | `			}` |
|       65 | 12365 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12366 | `		}` |
|        - | 12367 | `		/* Append extension */` |
|        5 | 12368 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12369 | `		/* Try to include the file */` |
|        5 | 12370 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12371 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12372 | `		if( rc == SXRET_OK ){` |
|        - | 12373 | `			/* File included successfully */` |
|      ! 0 | 12374 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12375 | `			return SXRET_OK;` |
|        - | 12376 | `		}` |
|        - | 12377 | `		/* Move past the comma */` |
|        5 | 12378 | `		zCur = zComma;` |
|        5 | 12379 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12380 | `			zCur++;` |
|        1 | 12381 | `		}` |
|        1 | 12382 | `	}` |
|        3 | 12383 | `	SyBlobRelease(&sPath);` |
|        3 | 12384 | `	return SXRET_OK;` |
|        2 | 12385 |  |
|        - | 12386 | `/* Table of built-in VM functions. */` |
|        - | 12387 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12388 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12389 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12390 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12391 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12392 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12393 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12394 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12395 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12396 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12397 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12398 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12399 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12400 | `	    /* Constants management */` |
|        - | 12401 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12402 | `	{ "define",   vm_builtin_define               },` |
|        - | 12403 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12404 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12405 | `	   /* Class/Object functions */` |
|        - | 12406 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12407 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12408 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12409 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12410 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12411 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12412 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12413 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12414 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12415 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12416 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12417 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12418 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12419 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12420 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12421 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12422 | `	   /* SPL Autoloading */` |
|        - | 12423 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12424 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12425 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12426 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12427 | `	   /* Random numbers/strings generators */` |
|        - | 12428 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12429 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12430 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12431 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12432 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12433 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12434 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12435 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12436 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12437 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12438 | `	   /* Language constructs functions */` |
|        - | 12439 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12440 | `	{ "print", vm_builtin_print                   },` |
|        - | 12441 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12442 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12443 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12444 | `	  /* Variable handling functions */` |
|        - | 12445 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12446 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12447 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12448 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12449 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12450 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12451 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12452 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12453 | `	  /* Ouput control functions */` |
|        - | 12454 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12455 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12456 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12457 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12458 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12459 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12460 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12461 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12462 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12463 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12464 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12465 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12466 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12467 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12468 | `	  /* Assertion functions */` |
|        - | 12469 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12470 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12471 | `	  /* Error reporting functions */` |
|        - | 12472 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12473 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12474 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12475 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12476 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12477 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12478 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12479 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12480 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12481 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12482 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12483 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12484 | `	  /* Release info */` |
|        - | 12485 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12486 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12487 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12488 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12489 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12490 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12491 | `	  /* hashmap */` |
|        - | 12492 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12493 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12494 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12495 | `	  /* URL related function */` |
|        - | 12496 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12497 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12498 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12499 | `	   /* XML processing functions */` |
|        - | 12500 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12501 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12502 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12503 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12504 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12505 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12506 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12507 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12508 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12509 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12510 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12511 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12512 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12513 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12514 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12515 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12516 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12517 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12518 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12519 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12520 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12521 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12522 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12523 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12524 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12525 | `	   /* Command line processing */` |
|        - | 12526 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12527 | `	   /* JSON encoding/decoding */` |
|        - | 12528 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12529 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12530 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12531 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12532 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12533 | `	   /* Files/URI inclusion facility */` |
|        - | 12534 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12535 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12536 | `	{ "include",      vm_builtin_include          },` |
|        - | 12537 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12538 | `	{ "require",      vm_builtin_require          },` |
|        - | 12539 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12540 | `};` |
|        - | 12541 | `/*` |
|        - | 12542 | ` * Register the built-in VM functions defined above.` |
|        - | 12543 | ` */` |
|     2390 | 12544 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12545 |  |
|        - | 12546 | `	sxi32 rc;` |
|        - | 12547 | `	sxu32 n;` |
|   308312 | 12548 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12549 | `		/* Note that these special functions have access` |
|        - | 12550 | `		 * to the underlying virtual machine as their` |
|        - | 12551 | `		 * private data.` |
|        - | 12552 | `		 */` |
|   305922 | 12553 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   305922 | 12554 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12555 | `			return rc;` |
|        - | 12556 | `		}` |
|   152962 | 12557 | `	}` |
|     2392 | 12558 | `	return SXRET_OK;` |
|     1197 | 12559 |  |
|        - | 12560 | `/*` |
|        - | 12561 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12562 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12563 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12564 | ` */` |
|    33674 | 12565 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12566 |  |
|    33676 | 12567 | `	if( !iLoadable ){` |
|    32312 | 12568 | `		return pClass;` |
|        - | 12569 | `	}` |
|     1366 | 12570 | `	while(pClass){` |
|     1366 | 12571 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1366 | 12572 | `			return pClass;` |
|        - | 12573 | `		}` |
|      ! 0 | 12574 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12575 | `	}` |
|      ! 0 | 12576 | `	return 0;` |
|    16839 | 12577 |  |
|        - | 12578 | `/*` |
|        - | 12579 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12580 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12581 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12582 | ` * registered in the VM's class table.` |
|        - | 12583 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12584 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12585 | ` */` |
|       36 | 12586 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12587 |  |
|        - | 12588 | `	VmAutoloadCB *pEntry;` |
|        - | 12589 | `	ph7_value sArg,sResult;` |
|        - | 12590 | `	SyHashEntry *pHashEntry;` |
|        - | 12591 | `	ph7_class *pClass;` |
|        - | 12592 | `	sxu32 n,nEntry;` |
|       38 | 12593 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12594 | `	if( nEntry < 1 ){` |
|       24 | 12595 | `		return 0;` |
|        - | 12596 | `	}` |
|        - | 12597 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12598 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12599 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12600 | `	}` |
|        - | 12601 | `	/* Mark this class as being autoloaded */` |
|       14 | 12602 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12603 | `	/* Prepare the class name argument */` |
|       14 | 12604 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12605 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12606 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12607 | `	pClass = 0;` |
|       28 | 12608 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12609 | `		ph7_value *apArg[1];` |
|       24 | 12610 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12611 | `		if( pEntry == 0 ){` |
|      ! 0 | 12612 | `			continue;` |
|        - | 12613 | `		}` |
|       24 | 12614 | `		apArg[0] = &sArg;` |
|       24 | 12615 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12616 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12617 | `			continue;` |
|        - | 12618 | `		}` |
|        - | 12619 | `		/* Check if the class is now available */` |
|       24 | 12620 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12621 | `		if( pHashEntry ){` |
|       10 | 12622 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12623 | `			if( pClass ){` |
|       10 | 12624 | `				break;` |
|        - | 12625 | `			}` |
|      ! 0 | 12626 | `		}` |
|        9 | 12627 | `	}` |
|       14 | 12628 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12629 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12630 | `	/* Remove reentrancy guard */` |
|       14 | 12631 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12632 | `	return pClass;` |
|       20 | 12633 |  |
|        - | 12634 | `/*` |
|        - | 12635 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12636 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12637 | ` */` |
|       18 | 12638 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12639 |  |
|       20 | 12640 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12641 |  |
|        - | 12642 | `/*` |
|        - | 12643 | ` * Check if the given name refer to an installed class.` |
|        - | 12644 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12645 | ` */` |
|    33684 | 12646 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12647 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12648 | `	const char *zName,  /* Name of the target class */` |
|        - | 12649 | `	sxu32 nByte,        /* zName length */` |
|        - | 12650 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12651 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12652 | `						 */` |
|        - | 12653 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12654 | `	)` |
|        2 | 12655 |  |
|        - | 12656 | `	SyHashEntry *pEntry;` |
|        - | 12657 | `	ph7_class *pClass;` |
|    16842 | 12658 | `	SXUNUSED(iNest);` |
|        - | 12659 | `	/* Exact class lookup.` |
|        - | 12660 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12661 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    33686 | 12662 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    33686 | 12663 | `	if( pEntry == 0 ){` |
|        - | 12664 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 12665 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12666 | `	}` |
|    33668 | 12667 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    33668 | 12668 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    16844 | 12669 |  |
|        - | 12670 | `/*` |
|        - | 12671 | ` * Reference Table Implementation` |
|        - | 12672 | ` * Status: stable <chm@symisc.net>` |
|        - | 12673 | ` * Intro` |
|        - | 12674 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12675 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12676 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12677 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12678 | ` *  Refer to the official for more information on this powerful` |
|        - | 12679 | ` *  extension.` |
|        - | 12680 | ` */` |
|        - | 12681 | `/*` |
|        - | 12682 | ` * Allocate a new reference entry.` |
|        - | 12683 | ` */` |
|  3077342 | 12684 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12685 |  |
|        - | 12686 | `	VmRefObj *pRef;` |
|        - | 12687 | `	/* Allocate a new instance */` |
|  3077344 | 12688 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3077344 | 12689 | `	if( pRef == 0 ){` |
|      ! 0 | 12690 | `		return 0;` |
|        - | 12691 | `	}` |
|        - | 12692 | `	/* Zero the structure */` |
|  3077344 | 12693 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12694 | `	/* Initialize fields */` |
|  3077344 | 12695 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3077344 | 12696 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3077344 | 12697 | `	pRef->nIdx = nIdx;` |
|  3077344 | 12698 | `	return pRef;` |
|  1538673 | 12699 |  |
|        - | 12700 | `/*` |
|        - | 12701 | ` * Default hash function used by the reference table` |
|        - | 12702 | ` * for lookup/insertion operations.` |
|        - | 12703 | ` */` |
| 16972425 | 12704 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12705 |  |
|        - | 12706 | `	/* Calculate the hash based on the memory object index */` |
| 16972427 | 12707 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12708 |  |
|        - | 12709 | `/*` |
|        - | 12710 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12711 | ` * in the reference table.` |
|        - | 12712 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12713 | ` * otherwise.` |
|        - | 12714 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12715 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12716 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12717 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12718 | ` * Refer to the official for more information on this powerful` |
|        - | 12719 | ` * extension.` |
|        - | 12720 | ` */` |
|  9183882 | 12721 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12722 |  |
|        - | 12723 | `	VmRefObj *pRef;` |
|        - | 12724 | `	sxu32 nBucket;` |
|        - | 12725 | `	/* Point to the appropriate bucket */` |
|  9183884 | 12726 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12727 | `	/* Perform the lookup */` |
|  9183884 | 12728 | `	pRef = pVm->apRefObj[nBucket];` |
| 20009528 | 12729 | `	for(;;){` |
| 40003729 | 12730 | `		if( pRef == 0 ){` |
|  3159272 | 12731 | `			break;` |
|        - | 12732 | `		}` |
| 36844459 | 12733 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12734 | `			/* Entry found */` |
|  6024614 | 12735 | `			return pRef;` |
|        - | 12736 | `		}` |
|        - | 12737 | `		/* Point to the next entry */` |
| 30819847 | 12738 | `		pRef = pRef->pNextCollide;` |
|        2 | 12739 | `	}` |
|        - | 12740 | `	/* No such entry,return NULL */` |
|  3159272 | 12741 | `	return 0;` |
|  4591943 | 12742 |  |
|        - | 12743 | `/*` |
|        - | 12744 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12745 | ` *` |
|        - | 12746 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12747 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12748 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12749 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12750 | ` * Refer to the official for more information on this powerful` |
|        - | 12751 | ` * extension.` |
|        - | 12752 | ` */` |
|  3077342 | 12753 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12754 |  |
|        - | 12755 | `	sxu32 nBucket;` |
|  3077344 | 12756 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12757 | `		VmRefObj **apNew;` |
|        - | 12758 | `		sxu32 nNew;` |
|        - | 12759 | `		/* Allocate a larger table */` |
|     4066 | 12760 | `		nNew = pVm->nRefSize << 1;` |
|     4066 | 12761 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4066 | 12762 | `		if( apNew ){` |
|     4066 | 12763 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12764 | `			sxu32 n;` |
|        - | 12765 | `			/* Zero the structure */` |
|     4066 | 12766 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12767 | `			/* Rehash all referenced entries */` |
|  2841420 | 12768 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12769 | `				/* Remove old collision links */` |
|  2837356 | 12770 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12771 | `				/* Point to the appropriate bucket */` |
|  2837356 | 12772 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12773 | `				/* Insert the entry  */` |
|  2837356 | 12774 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2837356 | 12775 | `				if( apNew[nBucket] ){` |
|  2298896 | 12776 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12777 | `				}` |
|  2837356 | 12778 | `				apNew[nBucket] = pEntry;` |
|        - | 12779 | `				/* Point to the next entry */` |
|  2837356 | 12780 | `				pEntry = pEntry->pNext;` |
|  1418679 | 12781 | `			}` |
|        - | 12782 | `			/* Release the old table */` |
|     4066 | 12783 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12784 | `			/* Install the new one */` |
|     4066 | 12785 | `			pVm->apRefObj = apNew;` |
|     4066 | 12786 | `			pVm->nRefSize = nNew;` |
|     2032 | 12787 | `		}` |
|     2032 | 12788 | `	}` |
|        - | 12789 | `	/* Point to the appropriate bucket */` |
|  3077344 | 12790 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12791 | `	/* Insert the entry */` |
|  3077344 | 12792 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3077344 | 12793 | `	if( pVm->apRefObj[nBucket] ){` |
|  2535173 | 12794 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1267627 | 12795 | `	}` |
|  3077344 | 12796 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3077344 | 12797 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3077344 | 12798 | `	pVm->nRefUsed++;` |
|  3077344 | 12799 | `	return SXRET_OK;` |
|        2 | 12800 |  |
|        - | 12801 | `/*` |
|        - | 12802 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12803 | ` * the reference table.` |
|        - | 12804 | ` * This function is invoked when the user perform an unset` |
|        - | 12805 | ` * call [i.e: unset($var); ].` |
|        - | 12806 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12807 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12808 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12809 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12810 | ` * Refer to the official for more information on this powerful` |
|        - | 12811 | ` * extension.` |
|        - | 12812 | ` */` |
|  3042924 | 12813 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12814 |  |
|        - | 12815 | `	ph7_hashmap_node **apNode;` |
|        - | 12816 | `	SyHashEntry **apEntry;` |
|        - | 12817 | `	sxu32 n;` |
|        - | 12818 | `	/* Point to the reference table */` |
|  3042926 | 12819 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3042926 | 12820 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12821 | `	/* Unlink the entry from the reference table */` |
|  3130890 | 12822 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    87966 | 12823 | `		if( apEntry[n] ){` |
|    87916 | 12824 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    43957 | 12825 | `		}` |
|    43984 | 12826 | `	}` |
|  6000470 | 12827 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2957546 | 12828 | `		if( apNode[n] ){` |
|     6964 | 12829 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3481 | 12830 | `		}` |
|  1478774 | 12831 | `	}` |
|  3042926 | 12832 | `	if( pRef->pPrevCollide ){` |
|  1169079 | 12833 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   585089 | 12834 | `	}else{` |
|  1873849 | 12835 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12836 | `	}` |
|  3042926 | 12837 | `	if( pRef->pNextCollide ){` |
|  1722855 | 12838 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   861414 | 12839 | `	}` |
|  3042926 | 12840 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12841 | `	/* Release the node */` |
|  3042926 | 12842 | `	SySetRelease(&pRef->aReference);` |
|  3042926 | 12843 | `	SySetRelease(&pRef->aArrEntries);` |
|  3042926 | 12844 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3042926 | 12845 | `	pVm->nRefUsed--;` |
|  3042926 | 12846 | `	return SXRET_OK;` |
|        2 | 12847 |  |
|        - | 12848 | `/*` |
|        - | 12849 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12850 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12851 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12852 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12853 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12854 | ` * Refer to the official for more information on this powerful` |
|        - | 12855 | ` * extension.` |
|        - | 12856 | ` */` |
|  3108386 | 12857 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12858 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12859 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12860 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12861 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12862 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12863 | `	)` |
|        2 | 12864 |  |
|  3108388 | 12865 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12866 | `	VmRefObj *pRef;` |
|        - | 12867 | `	/* Check if the referenced object already exists */` |
|  3108388 | 12868 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3108388 | 12869 | `	if( pRef == 0 ){` |
|        - | 12870 | `		/* Create a new entry */` |
|  3077344 | 12871 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3077344 | 12872 | `		if( pRef == 0 ){` |
|      ! 0 | 12873 | `			return SXERR_MEM;` |
|        - | 12874 | `		}` |
|  3077344 | 12875 | `		pRef->iFlags = iFlags;` |
|        - | 12876 | `		/* Install the entry */` |
|  3077344 | 12877 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1538671 | 12878 | `	}` |
|  3108388 | 12879 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3108388 | 12880 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12881 | `		VmSlot sRef;` |
|        - | 12882 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12883 | `		 * be deleted when we leave this frame.` |
|        - | 12884 | `		 */` |
|    82008 | 12885 | `		sRef.nIdx = nIdx;` |
|    82008 | 12886 | `		sRef.pUserData = pEntry;` |
|    82008 | 12887 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12888 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12889 | `		}` |
|    41003 | 12890 | `	}` |
|  3108388 | 12891 | `	if( pEntry ){` |
|        - | 12892 | `		/* Address of the hash-entry */` |
|   112860 | 12893 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    56429 | 12894 | `	}` |
|  3108388 | 12895 | `	if( pMapEntry ){` |
|        - | 12896 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2990236 | 12897 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1495117 | 12898 | `	}` |
|  3108388 | 12899 | `	return SXRET_OK;` |
|  1554195 | 12900 |  |
|        - | 12901 | `/*` |
|        - | 12902 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12903 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12904 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12905 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12906 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12907 | ` * Refer to the official for more information on this powerful` |
|        - | 12908 | ` * extension.` |
|        - | 12909 | ` */` |
|  3032566 | 12910 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12911 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12912 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12913 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12914 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12915 | `	)` |
|        2 | 12916 |  |
|        - | 12917 | `	VmRefObj *pRef;` |
|        - | 12918 | `	sxu32 n;` |
|        - | 12919 | `	/* Check if the referenced object already exists */` |
|  3032568 | 12920 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3032568 | 12921 | `	if( pRef == 0 ){` |
|        - | 12922 | `		/* Not such entry */` |
|    81924 | 12923 | `		return SXERR_NOTFOUND;` |
|        - | 12924 | `	}` |
|        - | 12925 | `	/* Remove the desired entry */` |
|  2950646 | 12926 | `	if( pEntry ){` |
|        - | 12927 | `		SyHashEntry **apEntry;` |
|       56 | 12928 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12929 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12930 | `			if( apEntry[n] == pEntry ){` |
|        - | 12931 | `				/* Nullify the entry */` |
|       56 | 12932 | `				apEntry[n] = 0;` |
|        - | 12933 | `				/*` |
|        - | 12934 | `				 * NOTE:` |
|        - | 12935 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12936 | `				 * we avoid wasting spaces.` |
|        - | 12937 | `				 */` |
|       27 | 12938 | `			}` |
|       79 | 12939 | `		}` |
|       27 | 12940 | `	}` |
|  2950646 | 12941 | `	if( pMapEntry ){` |
|        - | 12942 | `		ph7_hashmap_node **apNode;` |
|  2950592 | 12943 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5901276 | 12944 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2950686 | 12945 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12946 | `				/* nullify the entry */` |
|  2950592 | 12947 | `				apNode[n] = 0;` |
|  1475295 | 12948 | `			}` |
|  1475344 | 12949 | `		}` |
|  1475295 | 12950 | `	}` |
|  2950646 | 12951 | `	return SXRET_OK;` |
|  1516285 | 12952 |  |
|        - | 12953 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12954 | `/*` |
|        - | 12955 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12956 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12957 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12958 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12959 | ` * For more information on how to register IO stream devices,please` |
|        - | 12960 | ` * refer to the official documentation.` |
|        - | 12961 | ` */` |
|    25118 | 12962 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12963 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12964 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12965 | `	int nByte              /* *pzDevice length*/` |
|        - | 12966 | `	)` |
|        2 | 12967 |  |
|        - | 12968 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12969 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12970 | `	SyString sDev,sCur;` |
|        - | 12971 | `	sxu32 n,nEntry;` |
|        - | 12972 | `	int rc;` |
|        - | 12973 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    25120 | 12974 | `	zNext = zCur = zIn = *pzDevice;` |
|    25120 | 12975 | `	zEnd = &zIn[nByte];` |
|  1599076 | 12976 | `	while( zIn < zEnd ){` |
|  1573960 | 12977 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12978 | `			/* Got one */` |
|        3 | 12979 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12980 | `			break;` |
|        - | 12981 | `		}` |
|        - | 12982 | `		/* Advance the cursor */` |
|  1573958 | 12983 | `		zIn++;` |
|        2 | 12984 | `	}` |
|    25120 | 12985 | `	if( zIn >= zEnd ){` |
|        - | 12986 | `		/* No such scheme,return the default stream */` |
|    25118 | 12987 | `		return pVm->pDefStream;` |
|        - | 12988 | `	}` |
|        3 | 12989 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12990 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12991 | `	SyStringFullTrim(&sDev);` |
|        - | 12992 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12993 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12994 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12995 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12996 | `		pStream = apStream[n];` |
|        3 | 12997 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12998 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12999 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 13000 | `		if( rc == 0 ){` |
|        - | 13001 | `			/* Stream device found */` |
|        3 | 13002 | `			*pzDevice = zNext;` |
|        3 | 13003 | `			return pStream;` |
|        - | 13004 | `		}` |
|      ! 0 | 13005 | `	}` |
|        - | 13006 | `	/* No such stream,return NULL */` |
|      ! 0 | 13007 | `	return 0;` |
|    12561 | 13008 |  |
|        - | 13009 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 13010 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 13011 |  |
