# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5780/7523 lines (76.83%)

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
|   848248 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   848250 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   848216 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   848206 |   104 | `	return FALSE;` |
|   424148 |   105 |  |
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
|   554244 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   554246 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   554246 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   554242 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   554242 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   554242 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   554242 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   554242 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   554242 |   152 | `	pCons->xExpand = xExpand;` |
|   554242 |   153 | `	pCons->pUserData = pUserData;` |
|   554242 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   554242 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   554242 |   161 | `	return SXRET_OK;` |
|   277124 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1218546 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1218548 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1218548 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1218548 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1218548 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1218548 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1218548 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1218548 |   195 | `	pFunc->pVm   = pVm;` |
|  1218548 |   196 | `	pFunc->xFunc = xFunc;` |
|  1218548 |   197 | `	pFunc->pUserData = pUserData;` |
|  1218548 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1218548 |   200 | `	*ppOut = pFunc;` |
|  1218548 |   201 | `	return SXRET_OK;` |
|   609275 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1221100 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1221102 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1221102 |   223 | `	if( pEntry ){` |
|     2556 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2556 |   225 | `		pFunc->pUserData = pUserData;` |
|     2556 |   226 | `		pFunc->xFunc = xFunc;` |
|     2556 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2556 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1218548 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1218548 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1218548 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1218548 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1218548 |   243 | `	return SXRET_OK;` |
|   610552 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   174528 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   174530 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   174530 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   174530 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   174530 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   174530 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   174530 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   270 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   174530 |   271 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   174530 |   272 | `	pFunc->iFlags = iFlags;` |
|   174530 |   273 | `	pFunc->pUserData = pUserData;` |
|   174530 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   174530 |   275 | `	return SXRET_OK;` |
|        2 |   276 |  |
|        - |   277 | `/*` |
|        - |   278 | ` * Namespace-aware function lookup.` |
|        - |   279 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   280 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   281 | ` */` |
|        - |   282 | `/*` |
|        - |   283 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   284 | ` */` |
|   685660 |   285 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   286 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   287 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   288 | `	SyString *pName     /* Function name */` |
|        - |   289 | `	)` |
|        2 |   290 |  |
|        - |   291 | `	SyHashEntry *pEntry;` |
|        - |   292 | `	sxi32 rc;` |
|   685662 |   293 | `	if( pName == 0 ){` |
|        - |   294 | `		/* Use the built-in name */` |
|    37788 |   295 | `		pName = &pFunc->sName;` |
|    18893 |   296 | `	}` |
|        - |   297 | `	/* Check for duplicates (functions with the same name) first */` |
|   685662 |   298 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   685662 |   299 | `	if( pEntry ){` |
|   534112 |   300 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   534112 |   301 | `		if( pLink != pFunc ){` |
|        - |   302 | `			/* Link */` |
|      188 |   303 | `			pFunc->pNextName = pLink;` |
|      188 |   304 | `			pEntry->pUserData = pFunc;` |
|       93 |   305 | `		}` |
|   534112 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* First time seen */` |
|   151552 |   309 | `	pFunc->pNextName = 0;` |
|   151552 |   310 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   151552 |   311 | `	return rc;` |
|   342832 |   312 |  |
|        - |   313 | `/*` |
|        - |   314 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   315 | ` */` |
|    48954 |   316 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   317 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   318 | `	ph7_class *pClass /* Target Class */` |
|        - |   319 | `	)` |
|        2 |   320 |  |
|    48956 |   321 | `	SyString *pName = &pClass->sName;` |
|        - |   322 | `	SyHashEntry *pEntry;` |
|        - |   323 | `	sxi32 rc;` |
|        - |   324 | `	/* Check for duplicates */` |
|    48956 |   325 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    48956 |   326 | `	if( pEntry ){` |
|       31 |   327 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   328 | `		/* Link entry with the same name */` |
|       31 |   329 | `		pClass->pNextName = pLink;` |
|       31 |   330 | `		pEntry->pUserData = pClass;` |
|       31 |   331 | `		return SXRET_OK;` |
|        - |   332 | `	}` |
|    48926 |   333 | `	pClass->pNextName = 0;` |
|        - |   334 | `	/* Perform a simple hashtable insertion */` |
|    48926 |   335 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    48926 |   336 | `	return rc;` |
|    24479 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Instruction builder interface.` |
|        - |   340 | ` */` |
|  3519190 |   341 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3519192 |   353 | `	sInstr.iOp = (sxu8)iOp;` |
|  3519192 |   354 | `	sInstr.iP1 = iP1;` |
|  3519192 |   355 | `	sInstr.iP2 = iP2;` |
|  3519192 |   356 | `	sInstr.p3  = p3;` |
|  3519192 |   357 | `	if( pIndex ){` |
|        - |   358 | `		/* Instruction index in the bytecode array */` |
|   202632 |   359 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   101315 |   360 | `	}` |
|        - |   361 | `	/* Finally,record the instruction */` |
|  3519192 |   362 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3519192 |   363 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   364 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   365 | `		/* Fall throw */` |
|      ! 0 |   366 | `	}` |
|  3519192 |   367 | `	return rc;` |
|        2 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Swap the current bytecode container with the given one.` |
|        - |   371 | ` */` |
|   418960 |   372 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   373 |  |
|   418962 |   374 | `	if( pContainer == 0 ){` |
|        - |   375 | `		/* Point to the default container */` |
|      ! 0 |   376 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   377 | `	}else{` |
|        - |   378 | `		/* Change container */` |
|   418962 |   379 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   380 | `	}` |
|   418962 |   381 | `	return SXRET_OK;` |
|        2 |   382 |  |
|        - |   383 | `/*` |
|        - |   384 | ` * Return the current bytecode container.` |
|        - |   385 | ` */` |
|   209480 |   386 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   387 |  |
|   209482 |   388 | `	return pVm->pByteContainer;` |
|        2 |   389 |  |
|        - |   390 | `/*` |
|        - |   391 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   392 | ` */` |
|   199716 |   393 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   394 |  |
|        - |   395 | `	VmInstr *pInstr;` |
|   199718 |   396 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   199718 |   397 | `	return pInstr;` |
|        2 |   398 |  |
|        - |   399 | `/*` |
|        - |   400 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   401 | ` */` |
|  1054320 |   402 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   403 |  |
|  1054322 |   404 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Pop the last VM instruction.` |
|        - |   408 | ` */` |
|   190178 |   409 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   410 |  |
|   190180 |   411 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   412 |  |
|        - |   413 | `/*` |
|        - |   414 | ` * Peek the last VM instruction.` |
|        - |   415 | ` */` |
|   682214 |   416 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   417 |  |
|   682216 |   418 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   419 |  |
|    29516 |   420 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   421 |  |
|        - |   422 | `	VmInstr *aInstr;` |
|        - |   423 | `	sxu32 n;` |
|    29518 |   424 | `	n = SySetUsed(pVm->pByteContainer);` |
|    29518 |   425 | `	if( n < 2 ){` |
|      ! 0 |   426 | `		return 0;` |
|        - |   427 | `	}` |
|    29518 |   428 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    29518 |   429 | `	return &aInstr[n - 2];` |
|    14760 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Allocate a new virtual machine frame.` |
|        - |   433 | ` */` |
|    18368 |   434 | `static VmFrame * VmNewFrame(` |
|        - |   435 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   436 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   437 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   438 | `	)` |
|        2 |   439 |  |
|        - |   440 | `	VmFrame *pFrame;` |
|        - |   441 | `	/* Allocate a new vm frame */` |
|    18370 |   442 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    18370 |   443 | `	if( pFrame == 0 ){` |
|      ! 0 |   444 | `		return 0;` |
|        - |   445 | `	}` |
|        - |   446 | `	/* Zero the structure */` |
|    18370 |   447 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   448 | `	/* Initialize frame fields */` |
|    18370 |   449 | `	pFrame->pUserData = pUserData;` |
|    18370 |   450 | `	pFrame->pThis = pThis;` |
|    18370 |   451 | `	pFrame->pVm = pVm;` |
|    18370 |   452 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    18370 |   453 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    18370 |   454 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    18370 |   455 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    18370 |   456 | `	return pFrame;` |
|     9186 |   457 |  |
|        - |   458 | `/* Forward declaration */` |
|        - |   459 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   460 | `/*` |
|        - |   461 | ` * Enter a VM frame.` |
|        - |   462 | ` */` |
|    18322 |   463 | `static sxi32 VmEnterFrame(` |
|        - |   464 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   465 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   466 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   467 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   468 | `	)` |
|        2 |   469 |  |
|        - |   470 | `	VmFrame *pFrame;` |
|        - |   471 | `	/* Allocate a new frame */` |
|    18324 |   472 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    18324 |   473 | `	if( pFrame == 0 ){` |
|      ! 0 |   474 | `		return SXERR_MEM;` |
|        - |   475 | `	}` |
|        - |   476 | `	/* Link to the list of active VM frame */` |
|    18324 |   477 | `	pFrame->pParent = pVm->pFrame;` |
|    18324 |   478 | `	pVm->pFrame = pFrame;` |
|    18324 |   479 | `	if( ppFrame ){` |
|        - |   480 | `		/* Write a pointer to the new VM frame */` |
|    15486 |   481 | `		*ppFrame = pFrame;` |
|     7742 |   482 | `	}` |
|    18324 |   483 | `	return SXRET_OK;` |
|     9163 |   484 |  |
|        - |   485 | `/*` |
|        - |   486 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   487 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   488 | ` * information.` |
|        - |   489 | ` */` |
|       56 |   490 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   491 |  |
|        - |   492 | `	VmFrame *pTarget,*pFrame;` |
|       58 |   493 | `	SyHashEntry *pEntry = 0;` |
|        - |   494 | `	sxi32 rc;` |
|        - |   495 | `	/* Point to the upper frame */` |
|       58 |   496 | `	pFrame = pVm->pFrame;` |
|       58 |   497 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       58 |   498 | `	pTarget = pFrame;` |
|       58 |   499 | `	pFrame = pTarget->pParent;` |
|       58 |   500 | `	while( pFrame ){` |
|       58 |   501 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   502 | `			/* Query the current frame */` |
|       58 |   503 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       58 |   504 | `			if( pEntry ){` |
|        - |   505 | `				/* Variable found */` |
|       58 |   506 | `				break;` |
|        - |   507 | `			}` |
|      ! 0 |   508 | `		}` |
|        - |   509 | `		/* Point to the upper frame */` |
|      ! 0 |   510 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   511 | `	}` |
|       58 |   512 | `	if( pEntry == 0 ){` |
|        - |   513 | `		/* Inexistant variable */` |
|      ! 0 |   514 | `		return SXERR_NOTFOUND;` |
|        - |   515 | `	}` |
|        - |   516 | `	/* Link to the current frame */` |
|       58 |   517 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       58 |   518 | `	if( rc == SXRET_OK ){` |
|        - |   519 | `		sxu32 nIdx;` |
|       58 |   520 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       58 |   521 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       28 |   522 | `	}` |
|       58 |   523 | `	return rc;` |
|       30 |   524 |  |
|        - |   525 | `/*` |
|        - |   526 | ` * Leave the top-most active frame.` |
|        - |   527 | ` */` |
|    15478 |   528 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   529 |  |
|    15480 |   530 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    15480 |   531 | `	if( pCurFrame ){` |
|        - |   532 | `		/* Unlink from the list of active VM frame */` |
|    15480 |   533 | `		pVm->pFrame = pCurFrame->pParent;` |
|    15480 |   534 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   535 | `			VmSlot  *aSlot;` |
|        - |   536 | `			sxu32 n;` |
|        - |   537 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    15322 |   538 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   104368 |   539 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   540 | `				/* Unset the local variable */` |
|    89048 |   541 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    44525 |   542 | `			}` |
|        - |   543 | `			/* Remove local reference */` |
|    15322 |   544 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   104428 |   545 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    89108 |   546 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    44555 |   547 | `			}` |
|     7660 |   548 | `		}` |
|        - |   549 | `		/* Release internal containers */` |
|    15480 |   550 | `		SyHashRelease(&pCurFrame->hVar);` |
|    15480 |   551 | `		SySetRelease(&pCurFrame->sArg);` |
|    15480 |   552 | `		SySetRelease(&pCurFrame->sLocal);` |
|    15480 |   553 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   554 | `		/* Release the whole structure */` |
|    15480 |   555 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     7739 |   556 | `	}` |
|    15480 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   560 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   561 | ` * should be skipped when looking for the real execution context.` |
|        - |   562 | ` */` |
|  6685464 |   563 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   564 |  |
|  6686328 |   565 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      864 |   566 | `		pFrame = pFrame->pParent;` |
|        2 |   567 | `	}` |
|  6685466 |   568 | `	return pFrame;` |
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
|   134758 |   688 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   689 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   690 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   691 | `	)` |
|        2 |   692 |  |
|        - |   693 | `	ph7_class_method *pMeth;` |
|        - |   694 | `	ph7_class_attr *pAttr;` |
|        - |   695 | `	SyHashEntry *pEntry;` |
|        - |   696 | `	sxi32 rc;` |
|        - |   697 | `	/* Reset the loop cursor */` |
|   134760 |   698 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   699 | `	/* Process only static and constant attribute */` |
|   566469 |   700 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   701 | `		/* Extract the current attribute */` |
|   364332 |   702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   364332 |   703 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   704 | `			ph7_value *pMemObj;` |
|        - |   705 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1494 |   706 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1494 |   707 | `			if( pMemObj == 0 ){` |
|      ! 0 |   708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   709 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   710 | `					&pClass->sName,&pAttr->sName` |
|        - |   711 | `					);` |
|      ! 0 |   712 | `				return SXERR_MEM;` |
|        - |   713 | `			}` |
|     1494 |   714 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   715 | `				/* Initialize attribute default value (any complex expression) */` |
|     1492 |   716 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      745 |   717 | `			}` |
|        - |   718 | `			/* Record attribute index */` |
|     1494 |   719 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   720 | `			/* Install static attribute in the reference table */` |
|     1494 |   721 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   722 | `			/* If this is a typed static property, register the slot so the` |
|        - |   723 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   724 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   725 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1494 |   726 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|      746 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Install class methods */` |
|   134760 |   748 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   749 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   750 | `		 */` |
|    58706 |   751 | `		return SXRET_OK;` |
|        - |   752 | `	}` |
|        - |   753 | `	/* Create constructor alias if not yet done */` |
|    76056 |   754 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   755 | `		/* User constructor with the same base class name */` |
|     5870 |   756 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5870 |   757 | `		if( pEntry ){` |
|      ! 0 |   758 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   759 | `			/* Create the alias */` |
|      ! 0 |   760 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   761 | `		}` |
|     2934 |   762 | `	}` |
|        - |   763 | `	/* Install the methods now */` |
|    76056 |   764 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   761965 |   765 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   647884 |   766 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   647884 |   767 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   647876 |   768 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   647876 |   769 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   770 | `				return rc;` |
|        - |   771 | `			}` |
|   323937 |   772 | `		}` |
|        2 |   773 | `	}` |
|        - |   774 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    76056 |   775 | `	pClass->bMounted = TRUE;` |
|    76056 |   776 | `	return SXRET_OK;` |
|    67381 |   777 |  |
|        - |   778 | `/*` |
|        - |   779 | ` * Allocate a private frame for attributes of the given` |
|        - |   780 | ` * class instance (Object in the PHP jargon).` |
|        - |   781 | ` */` |
|     1484 |   782 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   784 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   785 | `	)` |
|        2 |   786 |  |
|     1486 |   787 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   788 | `	ph7_class_attr *pAttr;` |
|        - |   789 | `	SyHashEntry *pEntry;` |
|        - |   790 | `	sxi32 rc;` |
|        - |   791 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1486 |   792 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     6006 |   793 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   794 | `		VmClassAttr *pVmAttr;` |
|        - |   795 | `		/* Extract the current attribute */` |
|     4522 |   796 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     4522 |   797 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     4522 |   798 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   799 | `			return SXERR_MEM;` |
|        - |   800 | `		}` |
|     4522 |   801 | `		pVmAttr->pAttr = pAttr;` |
|     4522 |   802 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   803 | `			ph7_value *pMemObj;` |
|        - |   804 | `			/* Reserve a memory object for this attribute */` |
|     4498 |   805 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     4498 |   806 | `			if( pMemObj == 0 ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|     4498 |   810 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     4498 |   811 | `			pVmAttr->iState = 0;` |
|     4498 |   812 | `			pVmAttr->pOwner = pClass;` |
|     4498 |   813 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   814 | `				/* Initialize attribute default value (any complex expression) */` |
|     1524 |   815 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     3737 |   816 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   817 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   818 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       28 |   819 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       13 |   820 | `			}` |
|     4498 |   821 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     4498 |   822 | `			if( rc != SXRET_OK ){` |
|        - |   823 | `				VmSlot sSlot;` |
|        - |   824 | `				/* Restore memory object */` |
|      ! 0 |   825 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   826 | `				sSlot.pUserData = 0;` |
|      ! 0 |   827 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|        - |   831 | `			/* Install attribute in the reference table */` |
|     4498 |   832 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   833 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   834 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   835 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     4498 |   836 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|     2250 |   848 | `		}else{` |
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
|     1486 |   860 | `	return SXRET_OK;` |
|      744 |   861 |  |
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
|   404140 |   873 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   874 |  |
|        - |   875 | `	ph7_value *pObj;` |
|        - |   876 | `	sxi32 rc;` |
|   404142 |   877 | `	if( pIndex ){` |
|        - |   878 | `		/* Object index in the object table */` |
|   395628 |   879 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   197813 |   880 | `	}` |
|        - |   881 | `	/* Reserve a slot for the new object */` |
|   404142 |   882 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   404142 |   883 | `	if( rc != SXRET_OK ){` |
|        - |   884 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   885 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   886 | `		 */` |
|      ! 0 |   887 | `		return 0;` |
|        - |   888 | `	}` |
|   404142 |   889 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   404142 |   890 | `	return pObj;` |
|   202072 |   891 |  |
|        - |   892 | `/*` |
|        - |   893 | ` * Reserve a memory object.` |
|        - |   894 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   895 | ` */` |
|  2146612 |   896 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   897 |  |
|        - |   898 | `	ph7_value *pObj;` |
|        - |   899 | `	sxi32 rc;` |
|  2146614 |   900 | `	if( pIndex ){` |
|        - |   901 | `		/* Object index in the object table */` |
|  2146614 |   902 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073306 |   903 | `	}` |
|        - |   904 | `	/* Reserve a slot for the new object */` |
|  2146614 |   905 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2146614 |   906 | `	if( rc != SXRET_OK ){` |
|        - |   907 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   908 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   909 | `		 */` |
|      ! 0 |   910 | `		return 0;` |
|        - |   911 | `	}` |
|  2146614 |   912 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2146614 |   913 | `	return pObj;` |
|  1073308 |   914 |  |
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
|     2838 |  1327 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1328 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1329 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1330 | `	 )` |
|        2 |  1331 |  |
|        - |  1332 | `	SyString sBuiltin;` |
|        - |  1333 | `	ph7_value *pObj;` |
|        - |  1334 | `	sxi32 rc;` |
|        - |  1335 | `	/* Zero the structure */` |
|     2840 |  1336 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1337 | `	/* Initialize VM fields */` |
|     2840 |  1338 | `	pVm->pEngine = &(*pEngine);` |
|     2840 |  1339 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1340 | `	/* Instructions containers */` |
|     2840 |  1341 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2840 |  1342 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2840 |  1343 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1344 | `	/* Object containers */` |
|     2840 |  1345 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2840 |  1346 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1347 | `	/* Virtual machine internal containers */` |
|     2840 |  1348 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2840 |  1349 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2840 |  1350 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2840 |  1351 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2840 |  1352 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2840 |  1353 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2840 |  1354 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2840 |  1355 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2840 |  1356 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2840 |  1357 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2840 |  1358 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2840 |  1359 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2840 |  1360 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2840 |  1361 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2840 |  1362 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2840 |  1363 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2840 |  1364 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2840 |  1365 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2840 |  1366 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2840 |  1367 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2840 |  1368 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2840 |  1369 | `	pVm->pPendingException = 0;` |
|        - |  1370 | `	/* Configuration containers */` |
|     2840 |  1371 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2840 |  1372 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2840 |  1373 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2840 |  1374 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2840 |  1375 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2840 |  1376 | `	pVm->iResponseStatus = 200;` |
|     2840 |  1377 | `	pVm->bHeadersSent = 0;` |
|     2840 |  1378 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1379 | `	/* Error callbacks containers */` |
|     2840 |  1380 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2840 |  1381 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2840 |  1382 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2840 |  1383 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2840 |  1384 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1385 | `	/* Set a default recursion limit */` |
|        - |  1386 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2840 |  1387 | `	pVm->nMaxDepth = 32;` |
|        - |  1388 | `#else` |
|        - |  1389 | `	pVm->nMaxDepth = 16;` |
|        - |  1390 | `#endif` |
|        - |  1391 | `	/* Default assertion flags */` |
|     2840 |  1392 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1393 | `	/* JSON return status */` |
|     2840 |  1394 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1395 | `	/* PRNG context */` |
|     2840 |  1396 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1397 | `	/* Install the null constant */` |
|     2840 |  1398 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2840 |  1399 | `	if( pObj == 0 ){` |
|      ! 0 |  1400 | `		rc = SXERR_MEM;` |
|      ! 0 |  1401 | `		goto Err;` |
|        - |  1402 | `	}` |
|     2840 |  1403 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1404 | `	/* Install the boolean TRUE constant */` |
|     2840 |  1405 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2840 |  1406 | `	if( pObj == 0 ){` |
|      ! 0 |  1407 | `		rc = SXERR_MEM;` |
|      ! 0 |  1408 | `		goto Err;` |
|        - |  1409 | `	}` |
|     2840 |  1410 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1411 | `	/* Install the boolean FALSE constant */` |
|     2840 |  1412 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2840 |  1413 | `	if( pObj == 0 ){` |
|      ! 0 |  1414 | `		rc = SXERR_MEM;` |
|      ! 0 |  1415 | `		goto Err;` |
|        - |  1416 | `	}` |
|     2840 |  1417 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1418 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1419 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1420 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2840 |  1421 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2840 |  1422 | `	if( pObj == 0 ){` |
|      ! 0 |  1423 | `		rc = SXERR_MEM;` |
|      ! 0 |  1424 | `		goto Err;` |
|        - |  1425 | `	}` |
|     2840 |  1426 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1427 | `	/* Create the global frame */` |
|     2840 |  1428 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2840 |  1429 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1430 | `		goto Err;` |
|        - |  1431 | `	}` |
|        - |  1432 | `	/* Initialize the code generator */` |
|     2840 |  1433 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2840 |  1434 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1435 | `		goto Err;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* VM correctly initialized,set the magic number */` |
|     2840 |  1438 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2840 |  1439 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1440 | `	/* Compile the built-in library */` |
|     2840 |  1441 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1442 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2840 |  1443 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1444 | `	/* Register Fiber internal C functions */` |
|     2840 |  1445 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2840 |  1446 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2840 |  1447 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2840 |  1448 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2840 |  1449 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2840 |  1450 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2840 |  1451 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2840 |  1452 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2840 |  1453 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2840 |  1454 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1455 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2840 |  1456 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2840 |  1457 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2840 |  1458 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2840 |  1459 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2840 |  1460 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2840 |  1461 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2840 |  1462 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2840 |  1463 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2840 |  1464 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2840 |  1465 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1466 | `	/* Reset the code generator */` |
|     2840 |  1467 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2840 |  1468 | `	return SXRET_OK;` |
|      ! 0 |  1469 | `Err:` |
|      ! 0 |  1470 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1471 | `	return rc;` |
|     1421 |  1472 |  |
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
|    16186 |  1499 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1500 |  |
|    16188 |  1501 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    16188 |  1502 | `	if( xCons != VmObConsumer ){` |
|     6960 |  1503 | `		pVm->nOutputLen += nLen;` |
|     6960 |  1504 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      894 |  1505 | `			pVm->bHeadersSent = 1;` |
|      446 |  1506 | `		}` |
|     3479 |  1507 | `	}` |
|    16188 |  1508 |  |
|        - |  1509 | `#define VM_STACK_GUARD 16` |
|        - |  1510 | `/*` |
|        - |  1511 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1512 | ` * our compiled PHP program.` |
|        - |  1513 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1514 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1515 | ` */` |
|    37300 |  1516 | `static ph7_value * VmNewOperandStack(` |
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
|    37302 |  1529 | `	nInstr += VM_STACK_GUARD;` |
|    37302 |  1530 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    37302 |  1531 | `	if( pStack == 0 ){` |
|      ! 0 |  1532 | `		return 0;` |
|        - |  1533 | `	}` |
|        - |  1534 | `	/* Initialize the operand stack */` |
|  2435112 |  1535 | `	while( nInstr > 0 ){` |
|  2397812 |  1536 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2397812 |  1537 | `		--nInstr;` |
|        2 |  1538 | `	}` |
|        - |  1539 | `	/* Ready for bytecode execution */` |
|    37302 |  1540 | `	return pStack;` |
|    18652 |  1541 |  |
|        - |  1542 | `/* Forward declaration */` |
|        - |  1543 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1544 | `/*` |
|        - |  1545 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1546 | ` * This routine gets called by the PH7 engine after` |
|        - |  1547 | ` * successful compilation of the target PHP program.` |
|        - |  1548 | ` */` |
|     2554 |  1549 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1550 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1551 | `	)` |
|        2 |  1552 |  |
|        - |  1553 | `	SyHashEntry *pEntry;` |
|        - |  1554 | `	sxi32 rc;` |
|     2556 |  1555 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1556 | `		/* Initialize your VM first */` |
|      ! 0 |  1557 | `		return SXERR_CORRUPT;` |
|        - |  1558 | `	}` |
|        - |  1559 | `	/* Mark the VM ready for byte-code execution */` |
|     2556 |  1560 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1561 | `	/* Release the code generator now we have compiled our program */` |
|     2556 |  1562 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1563 | `	/* Emit the DONE instruction */` |
|     2556 |  1564 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2556 |  1565 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1566 | `		return SXERR_MEM;` |
|        - |  1567 | `	}` |
|        - |  1568 | `	/* Script return value */` |
|     2556 |  1569 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1570 | `	/* Allocate a new operand stack */` |
|     2556 |  1571 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2556 |  1572 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1573 | `		return SXERR_MEM;` |
|        - |  1574 | `	}` |
|        - |  1575 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1576 | `	 * private data. */` |
|     2556 |  1577 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2556 |  1578 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1579 | `	/* Allocate the reference table */` |
|     2556 |  1580 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2556 |  1581 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2556 |  1582 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1583 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1584 | `		return SXERR_MEM;` |
|        - |  1585 | `	}` |
|        - |  1586 | `	/* Zero the reference table */` |
|     2556 |  1587 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1588 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2556 |  1589 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2556 |  1590 | `	if( rc != SXRET_OK ){` |
|        - |  1591 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1592 | `		return rc;` |
|        - |  1593 | `	}` |
|        - |  1594 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2556 |  1595 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2556 |  1596 | `	if( rc != SXRET_OK ){` |
|        - |  1597 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1598 | `		return rc;` |
|        - |  1599 | `	}` |
|        - |  1600 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2556 |  1601 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1602 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2556 |  1603 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1604 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2556 |  1605 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1606 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1607 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2556 |  1608 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2556 |  1609 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1610 | `#endif` |
|        - |  1611 | `	/* Initialize and install static and constants class attributes */` |
|     2556 |  1612 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    46220 |  1613 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    43666 |  1614 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    43666 |  1615 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1616 | `			return rc;` |
|        - |  1617 | `		}` |
|        2 |  1618 | `	}` |
|        - |  1619 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2556 |  1620 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1621 | `	/* VM is ready for bytecode execution */` |
|     2556 |  1622 | `	return SXRET_OK;` |
|     1279 |  1623 |  |
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
|     2546 |  1648 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1649 |  |
|        - |  1650 | `	/* Set the stale magic number */` |
|     2548 |  1651 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1652 | `	/* Release the private memory subsystem */` |
|     2548 |  1653 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2548 |  1654 | `	return SXRET_OK;` |
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
|   629196 |  1666 | `static sxi32 VmInitCallContext(` |
|        - |  1667 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1668 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1669 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1670 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1671 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1672 | `	)` |
|        2 |  1673 |  |
|   629198 |  1674 | `	pOut->pFunc = pFunc;` |
|   629198 |  1675 | `	pOut->pVm   = pVm;` |
|   629198 |  1676 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   629198 |  1677 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1678 | `	/* Assume a null return value */` |
|   629198 |  1679 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   629198 |  1680 | `	pOut->pRet = pRet;` |
|   629198 |  1681 | `	pOut->iFlags = iFlags;` |
|   629198 |  1682 | `	return SXRET_OK;` |
|        2 |  1683 |  |
|        - |  1684 | `/*` |
|        - |  1685 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1686 | ` * left behind.` |
|        - |  1687 | ` */` |
|   629196 |  1688 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1689 |  |
|        - |  1690 | `	sxu32 n;` |
|   629198 |  1691 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7612 |  1692 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    21892 |  1693 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    14282 |  1694 | `			if( apObj[n] == 0 ){` |
|        - |  1695 | `				/* Already released */` |
|      298 |  1696 | `				continue;` |
|        - |  1697 | `			}` |
|    13986 |  1698 | `			PH7_MemObjRelease(apObj[n]);` |
|    13986 |  1699 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6994 |  1700 | `		}` |
|     7612 |  1701 | `		SySetRelease(&pCtx->sVar);` |
|     3805 |  1702 | `	}` |
|   629198 |  1703 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   629198 |  1719 |  |
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
|  3614470 |  1750 | `static void VmPopOperand(` |
|        - |  1751 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1752 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|  3614472 |  1755 | `	ph7_value *pTos = *ppTos;` |
|  7690436 |  1756 | `	while( nPop > 0 ){` |
|  4075966 |  1757 | `		PH7_MemObjRelease(pTos);` |
|  4075966 |  1758 | `		pTos--;` |
|  4075966 |  1759 | `		nPop--;` |
|        2 |  1760 | `	}` |
|        - |  1761 | `	/* Top of the stack */` |
|  3614472 |  1762 | `	*ppTos = pTos;` |
|  3614472 |  1763 |  |
|        - |  1764 | `/*` |
|        - |  1765 | ` * Reserve a memory object.` |
|        - |  1766 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1767 | ` */` |
|  3105840 |  1768 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1769 |  |
|  3105842 |  1770 | `	ph7_value *pObj = 0;` |
|        - |  1771 | `	VmSlot *pSlot;` |
|        - |  1772 | `	sxu32 nIdx;` |
|        - |  1773 | `	/* Check for a free slot */` |
|  3105842 |  1774 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3105842 |  1775 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3105842 |  1776 | `	if( pSlot ){` |
|   959230 |  1777 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   959230 |  1778 | `		nIdx = pSlot->nIdx;` |
|   479614 |  1779 | `	}` |
|  3105842 |  1780 | `	if( pObj == 0 ){` |
|        - |  1781 | `		/* Reserve a new memory object */` |
|  2146614 |  1782 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2146614 |  1783 | `		if( pObj == 0 ){` |
|      ! 0 |  1784 | `			return 0;` |
|        - |  1785 | `		}` |
|  1073306 |  1786 | `	}` |
|        - |  1787 | `	/* Set a null default value */` |
|  3105842 |  1788 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3105842 |  1789 | `	pObj->nIdx = nIdx;` |
|  3105842 |  1790 | `	return pObj;` |
|  1552922 |  1791 |  |
|        - |  1792 | `/*` |
|        - |  1793 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1794 | ` */` |
|    32840 |  1795 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1796 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1797 | `	const char *zKey,  /* Entry key */` |
|        - |  1798 | `	sxu32 nByte,       /* Key length */` |
|        - |  1799 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1800 | `	)` |
|        2 |  1801 |  |
|        - |  1802 | `	ph7_value sKey;` |
|        - |  1803 | `	sxi32 rc;` |
|    32842 |  1804 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    32842 |  1805 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1806 | `	/* Perform the insertion */` |
|    32842 |  1807 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    32842 |  1808 | `	PH7_MemObjRelease(&sKey);` |
|    32842 |  1809 | `	return rc;` |
|        2 |  1810 |  |
|        - |  1811 | `/*` |
|        - |  1812 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1813 | ` * Return a pointer to the variable value on success.` |
|        - |  1814 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1815 | ` */` |
|  3364476 |  1816 | `static ph7_value * VmExtractMemObj(` |
|        - |  1817 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1818 | `	const SyString *pName, /* Variable name */` |
|        - |  1819 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1820 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1821 | `	)` |
|        2 |  1822 |  |
|  3364478 |  1823 | `	int bNullify = FALSE;` |
|        - |  1824 | `	SyHashEntry *pEntry;` |
|        - |  1825 | `	VmFrame *pFrame;` |
|        - |  1826 | `	ph7_value *pObj;` |
|        - |  1827 | `	sxu32 nIdx;` |
|        - |  1828 | `	sxi32 rc;` |
|        - |  1829 | `	/* Point to the top active frame */` |
|  3364478 |  1830 | `	pFrame = pVm->pFrame;` |
|  3364478 |  1831 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1832 | `	/* Perform the lookup */` |
|  3364478 |  1833 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1834 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1835 | `		pName = &sAnnon;` |
|        - |  1836 | `		/* Always nullify the object */` |
|      ! 0 |  1837 | `		bNullify = TRUE;` |
|      ! 0 |  1838 | `		bDup = FALSE;` |
|      ! 0 |  1839 | `	}` |
|        - |  1840 | `	/* Check the superglobals table first */` |
|  3364478 |  1841 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3364478 |  1842 | `	if( pEntry == 0 ){` |
|        - |  1843 | `		/* Query the top active frame */` |
|  3364438 |  1844 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3364438 |  1845 | `		if( pEntry == 0 ){` |
|    96482 |  1846 | `			char *zName = (char *)pName->zString;` |
|        - |  1847 | `			VmSlot sLocal;` |
|    96482 |  1848 | `			if( !bCreate ){` |
|        - |  1849 | `				/* Do not create the variable,return NULL instead */` |
|      116 |  1850 | `				return 0;` |
|        - |  1851 | `			}` |
|        - |  1852 | `			/* No such variable,automatically create a new one and install` |
|        - |  1853 | `			 * it in the current frame.` |
|        - |  1854 | `			 */` |
|    96368 |  1855 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    96368 |  1856 | `			if( pObj == 0 ){` |
|      ! 0 |  1857 | `				return 0;` |
|        - |  1858 | `			}` |
|    96368 |  1859 | `			nIdx = pObj->nIdx;` |
|    96368 |  1860 | `			if( bDup ){` |
|        - |  1861 | `				/* Duplicate name */` |
|      168 |  1862 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1863 | `				if( zName == 0 ){` |
|      ! 0 |  1864 | `					return 0;` |
|        - |  1865 | `				}` |
|       83 |  1866 | `			}` |
|        - |  1867 | `			/* Link to the top active VM frame */` |
|    96368 |  1868 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    96368 |  1869 | `			if( rc != SXRET_OK ){` |
|        - |  1870 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1871 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1872 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1873 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1874 | `				return 0;` |
|        - |  1875 | `			}` |
|    96368 |  1876 | `			if( pFrame->pParent != 0 ){` |
|        - |  1877 | `				/* Local variable */` |
|    89096 |  1878 | `				sLocal.nIdx = nIdx;` |
|    89096 |  1879 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    44549 |  1880 | `			}else{` |
|        - |  1881 | `				/* Register in the $GLOBALS array */` |
|     7274 |  1882 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1883 | `			}` |
|        - |  1884 | `			/* Install in the reference table */` |
|    96368 |  1885 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1886 | `			/* Save object index */` |
|    96368 |  1887 | `			pObj->nIdx = nIdx;` |
|    48185 |  1888 | `		}else{` |
|        - |  1889 | `			/* Extract variable contents */` |
|  3267958 |  1890 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3267958 |  1891 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3267958 |  1892 | `			if( bNullify && pObj ){` |
|      ! 0 |  1893 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1894 | `			}` |
|        - |  1895 | `		}` |
|  1682273 |  1896 | `	}else{` |
|        - |  1897 | `		/* Superglobal */` |
|       42 |  1898 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1899 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1900 | `	}` |
|  3364364 |  1901 | `	return pObj;` |
|  1682350 |  1902 |  |
|        - |  1903 | `/*` |
|        - |  1904 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1905 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1906 | ` */` |
|     2858 |  1907 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1908 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1909 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1910 | `	sxu32 nByte        /* zName length */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|        - |  1913 | `	SyHashEntry *pEntry;` |
|        - |  1914 | `	ph7_value *pValue;` |
|        - |  1915 | `	sxu32 nIdx;` |
|        - |  1916 | `	/* Query the superglobal table */` |
|     2860 |  1917 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2860 |  1918 | `	if( pEntry == 0 ){` |
|        - |  1919 | `		/* No such entry */` |
|      ! 0 |  1920 | `		return 0;` |
|        - |  1921 | `	}` |
|        - |  1922 | `	/* Extract the superglobal index in the global object pool */` |
|     2860 |  1923 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1924 | `	/* Extract the variable value  */` |
|     2860 |  1925 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2860 |  1926 | `	return pValue;` |
|     1431 |  1927 |  |
|        - |  1928 | `/*` |
|        - |  1929 | ` * Perform a raw hashmap insertion.` |
|        - |  1930 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1931 | ` */` |
|     2888 |  1932 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1933 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1934 | `	const char *zKey,   /* Entry key */` |
|        - |  1935 | `	int nKeylen,        /* zKey length*/` |
|        - |  1936 | `	const char *zData,  /* Entry data */` |
|        - |  1937 | `	int nLen            /* zData length */` |
|        - |  1938 | `	)` |
|        2 |  1939 |  |
|        - |  1940 | `	ph7_value sKey,sValue;` |
|        - |  1941 | `	sxi32 rc;` |
|     2890 |  1942 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2890 |  1943 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2890 |  1944 | `	if( zKey ){` |
|     2868 |  1945 | `		if( nKeylen < 0 ){` |
|     2816 |  1946 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1407 |  1947 | `		}` |
|     2868 |  1948 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1433 |  1949 | `	}` |
|     2890 |  1950 | `	if( zData ){` |
|     2890 |  1951 | `		if( nLen < 0 ){` |
|        - |  1952 | `			/* Compute length automatically */` |
|      144 |  1953 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1954 | `		}` |
|     2890 |  1955 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1444 |  1956 | `	}` |
|        - |  1957 | `	/* Perform the insertion */` |
|     2890 |  1958 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2890 |  1959 | `	PH7_MemObjRelease(&sKey);` |
|     2890 |  1960 | `	PH7_MemObjRelease(&sValue);` |
|     2890 |  1961 | `	return rc;` |
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
|    41194 |  1976 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1977 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1978 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1979 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1980 | `	)` |
|        2 |  1981 |  |
|    41196 |  1982 | `	sxi32 rc = SXRET_OK;` |
|    41196 |  1983 | `	switch(nOp){` |
|     1269 |  1984 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2540 |  1985 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2540 |  1986 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1987 | `		/* VM output consumer callback */` |
|        - |  1988 | `#ifdef UNTRUST` |
|        - |  1989 | `		if( xConsumer == 0 ){` |
|        - |  1990 | `			rc = SXERR_CORRUPT;` |
|        - |  1991 | `			break;` |
|        - |  1992 | `		}` |
|        - |  1993 | `#endif` |
|        - |  1994 | `		/* Install the output consumer */` |
|     2540 |  1995 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2540 |  1996 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2540 |  1997 | `		break;` |
|        - |  1998 | `							   }` |
|     1277 |  1999 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2000 | `		/* Import path */` |
|        - |  2001 | `		  const char *zPath;` |
|        - |  2002 | `		  SyString sPath;` |
|     2556 |  2003 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2004 | `#if defined(UNTRUST)` |
|        - |  2005 | `		  if( zPath == 0 ){` |
|        - |  2006 | `			  rc = SXERR_EMPTY;` |
|        - |  2007 | `			  break;` |
|        - |  2008 | `		  }` |
|        - |  2009 | `#endif` |
|     2556 |  2010 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2011 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2012 | `#ifdef __WINNT__` |
|        2 |  2013 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2014 | `#endif` |
|     5110 |  2015 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2016 | `		  /* Remove leading and trailing white spaces */` |
|     2556 |  2017 | `		  SyStringFullTrim(&sPath);` |
|     2556 |  2018 | `		  if( sPath.nByte > 0 ){` |
|        - |  2019 | `			  /* Store the path in the corresponding conatiner */` |
|     2556 |  2020 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1277 |  2021 | `		  }` |
|     2556 |  2022 | `		  break;` |
|        - |  2023 | `									 }` |
|     1277 |  2024 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2025 | `		/* Run-Time Error report */` |
|     2556 |  2026 | `		pVm->bErrReport = 1;` |
|     2556 |  2027 | `		break;` |
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
|    12770 |  2049 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2050 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2051 | `		/* Create a new superglobal/global variable */` |
|    25542 |  2052 | `		const char *zName = va_arg(ap,const char *);` |
|    25542 |  2053 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    25542 |  2064 | `		nByte = SyStrlen(zName);` |
|    25542 |  2065 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2066 | `			/* Check if the superglobal is already installed */` |
|    25542 |  2067 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12772 |  2068 | `		}else{` |
|        - |  2069 | `			/* Query the top active VM frame */` |
|      ! 0 |  2070 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2071 | `		}` |
|    25542 |  2072 | `		if( pEntry ){` |
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
|    25542 |  2083 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25542 |  2084 | `			if( pObj == 0 ){` |
|      ! 0 |  2085 | `				rc = SXERR_MEM;` |
|      ! 0 |  2086 | `				break;` |
|        - |  2087 | `			}` |
|    25542 |  2088 | `			nIdx = pObj->nIdx;` |
|        - |  2089 | `			/* Copy value */` |
|    25542 |  2090 | `			PH7_MemObjStore(pValue,pObj);` |
|    25542 |  2091 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2092 | `				/* Install the superglobal */` |
|    25542 |  2093 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12772 |  2094 | `			}else{` |
|        - |  2095 | `				/* Install in the current frame */` |
|      ! 0 |  2096 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2097 | `			}` |
|    25542 |  2098 | `			if( rc == SXRET_OK ){` |
|        - |  2099 | `				SyHashEntry *pRef;` |
|    25542 |  2100 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25542 |  2101 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12772 |  2102 | `				}else{` |
|      ! 0 |  2103 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2104 | `				}` |
|        - |  2105 | `				/* Install in the reference table */` |
|    25542 |  2106 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25542 |  2107 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2108 | `					/* Register in the $GLOBALS array */` |
|    25542 |  2109 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12770 |  2110 | `				}` |
|    12770 |  2111 | `			}` |
|        - |  2112 | `		}` |
|    25542 |  2113 | `		break;` |
|        - |  2114 | `									}` |
|     1407 |  2115 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2116 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2117 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2118 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2119 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2120 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2121 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2816 |  2122 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2816 |  2123 | `		const char *zValue = va_arg(ap,const char *);` |
|     2816 |  2124 | `		int nLen = va_arg(ap,int);` |
|        - |  2125 | `		ph7_hashmap *pMap;` |
|        - |  2126 | `		ph7_value *pValue;` |
|     2816 |  2127 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2128 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2129 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2815 |  2130 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2131 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2132 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2814 |  2133 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2134 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2135 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2814 |  2136 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2137 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2138 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2814 |  2139 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2140 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2141 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2814 |  2142 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2143 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2144 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2145 | `		}else{` |
|        - |  2146 | `			/* Extract the $_SERVER superglobal */` |
|     2814 |  2147 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2148 | `		}` |
|     2816 |  2149 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2150 | `			/* No such entry */` |
|      ! 0 |  2151 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2152 | `			break;` |
|        - |  2153 | `		}` |
|        - |  2154 | `		/* Point to the hashmap */` |
|     2816 |  2155 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2156 | `		/* Perform the insertion */` |
|     2816 |  2157 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2816 |  2158 | `		break;` |
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
|     2554 |  2209 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2210 | `		/* Register an IO stream device */` |
|     5110 |  2211 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2212 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7662 |  2213 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5110 |  2214 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2215 | `				/* Invalid stream */` |
|      ! 0 |  2216 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2217 | `				break;` |
|        - |  2218 | `		}` |
|     5110 |  2219 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2220 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2556 |  2221 | `			pVm->pDefStream = pStream;` |
|     1277 |  2222 | `		}` |
|        - |  2223 | `		/* Insert in the appropriate container */` |
|     5110 |  2224 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5110 |  2225 | `		break;` |
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
|    41196 |  2293 | `	return rc;` |
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
|      134 |  2372 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2373 |  |
|      136 |  2374 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2375 | `		ph7_value apArg[4];` |
|        - |  2376 | `		ph7_value *apArgPtr[4];` |
|        - |  2377 | `		ph7_value sResult;` |
|        - |  2378 | `		SyString sErr;` |
|        - |  2379 | `		/* Prepare arguments */` |
|       61 |  2380 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2381 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2382 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2383 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2384 | `		if( pFile ){` |
|       61 |  2385 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2386 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2387 | `		}else{` |
|      ! 0 |  2388 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2389 | `		}` |
|       61 |  2390 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2391 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2392 | `		/* Set up pointer array */` |
|       61 |  2393 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2394 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2395 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2396 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2397 | `		/* Call the handler */` |
|       61 |  2398 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2399 | `		/* Check return value */` |
|       61 |  2400 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2401 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2402 | `		}` |
|        - |  2403 | `		/* Release */` |
|       61 |  2404 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2405 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2406 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2407 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2408 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2409 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2410 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2411 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2412 | `	}` |
|        - |  2413 | `	/* No handler, always call error handler */` |
|       75 |  2414 | `	return TRUE;` |
|       69 |  2415 |  |
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
|       38 |  2474 | `static sxi32 VmThrowErrorAp(` |
|        - |  2475 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2476 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2477 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2478 | `	const char *zFormat, /* Format message */` |
|        - |  2479 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2480 | `	)` |
|        2 |  2481 |  |
|       40 |  2482 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2483 | `	SyBlob sMsg;` |
|        - |  2484 | `	SyString *pFile;` |
|        - |  2485 | `	char *zErr;` |
|       40 |  2486 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2487 | `	if( !pVm->bErrReport ){` |
|        - |  2488 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2489 | `		return SXRET_OK;` |
|        - |  2490 | `	}` |
|        - |  2491 | `	/* Reset the working buffer */` |
|       40 |  2492 | `	SyBlobReset(pWorker);` |
|        - |  2493 | `	/* Peek the processed file if available */` |
|       40 |  2494 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2495 | `	if( pFile ){` |
|        - |  2496 | `		/* Append file name */` |
|       40 |  2497 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2498 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2499 | `	}` |
|        - |  2500 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2501 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2502 | `	 * the correct errno value. */` |
|       40 |  2503 | `	zErr = "Error:  ";` |
|       40 |  2504 | `	switch(iErr){` |
|        4 |  2505 | `	case PH7_CTX_WARNING:` |
|        9 |  2506 | `		zErr = "Warning:  ";` |
|        9 |  2507 | `		break;` |
|        3 |  2508 | `	case PH7_CTX_NOTICE:` |
|        7 |  2509 | `		zErr = "Notice:  ";` |
|        6 |  2510 | `		break;` |
|       12 |  2511 | `	default:` |
|        - |  2512 | `		/* do not change iErr */` |
|       24 |  2513 | `		break;` |
|        - |  2514 | `	}` |
|       40 |  2515 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2516 | `	if( pFuncName ){` |
|        - |  2517 | `		/* Append function name first */` |
|       26 |  2518 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2519 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2520 | `	}` |
|        - |  2521 | `	/* Format the raw message */` |
|       40 |  2522 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2523 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2524 | `	/* Check if a user error handler is installed */` |
|       40 |  2525 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2526 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2527 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2528 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2529 | `	}` |
|       40 |  2530 | `	SyBlobRelease(&sMsg);` |
|       40 |  2531 | `	return rc;` |
|       21 |  2532 |  |
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
|    12106 |  2851 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2852 |  |
|        - |  2853 | `	SyHashEntry *pSlot;` |
|        - |  2854 | `	VmClassAttr *pVmAttr;` |
|        - |  2855 | `	ph7_class_attr *pAttr;` |
|    12108 |  2856 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    12108 |  2857 | `	if( pSlot == 0 ){` |
|    11962 |  2858 | `		return SXRET_OK; /* Not a typed slot */` |
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
|     6055 |  2959 |  |
|        - |  2960 |  |
|        - |  2961 | `/*` |
|        - |  2962 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2963 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2964 | ` * information.` |
|        - |  2965 | ` * ------------------------------------` |
|        - |  2966 | ` * Simple boring wrapper function.` |
|        - |  2967 | ` * ------------------------------------` |
|        - |  2968 | ` */` |
|       14 |  2969 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2970 |  |
|        - |  2971 | `	va_list ap;` |
|        - |  2972 | `	sxi32 rc;` |
|       15 |  2973 | `	va_start(ap,zFormat);` |
|       15 |  2974 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2975 | `	va_end(ap);` |
|       15 |  2976 | `	return rc;` |
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
|    37398 |  3354 | `static sxi32 VmByteCodeExec(` |
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
|    37400 |  3372 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    37400 |  3373 | `	if( nTos < 0 ){` |
|    35156 |  3374 | `		pTos = &pStack[-1];` |
|    17579 |  3375 | `	}else{` |
|     2246 |  3376 | `		pTos = &pStack[nTos];` |
|        - |  3377 | `	}` |
|    37400 |  3378 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    37400 |  3379 | `	pc = nPc;` |
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
|  5408713 |  3400 | `	for(;;){` |
|        - |  3401 | `		/* Fetch the instruction to execute */` |
| 10816724 |  3402 | `		pInstr = &aInstr[pc];` |
| 10816724 |  3403 | `		rc = SXRET_OK;` |
|        - |  3404 | `/*` |
|        - |  3405 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3406 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3407 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3408 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3409 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3410 | ` */` |
| 10816724 |  3411 | `		switch(pInstr->iOp){` |
|        - |  3412 | `/*` |
|        - |  3413 | ` * DONE: P1 * *` |
|        - |  3414 | ` *` |
|        - |  3415 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3416 | ` * and return immediately.` |
|        - |  3417 | ` */` |
|    18370 |  3418 | `case PH7_OP_DONE:` |
|    36742 |  3419 | `	if( pInstr->iP1 ){` |
|        - |  3420 | `#ifdef UNTRUST` |
|        - |  3421 | `		if( pTos < pStack ){` |
|        - |  3422 | `			goto Abort;` |
|        - |  3423 | `		}` |
|        - |  3424 | `#endif` |
|    21642 |  3425 | `		if( pLastRef ){` |
|    13788 |  3426 | `			*pLastRef = pTos->nIdx;` |
|     6893 |  3427 | `		}` |
|    21642 |  3428 | `		if( pResult ){` |
|        - |  3429 | `			/* Execution result */` |
|    20558 |  3430 | `			PH7_MemObjStore(pTos,pResult);` |
|    10278 |  3431 | `		}` |
|    21642 |  3432 | `		VmPopOperand(&pTos,1);` |
|    25922 |  3433 | `	}else if( pLastRef ){` |
|        - |  3434 | `		/* Nothing referenced */` |
|     1326 |  3435 | `		*pLastRef = SXU32_HIGH;` |
|      662 |  3436 | `	}` |
|        - |  3437 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3438 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3439 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3440 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3441 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3442 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3443 | `	 * block can override it.` |
|        - |  3444 | `	 */` |
|    36744 |  3445 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    36742 |  3460 | `	goto Done;` |
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
|   232052 |  3505 | `case PH7_OP_JMP:` |
|   464150 |  3506 | `	pc = pInstr->iP2 - 1;` |
|   464150 |  3507 | `	break;` |
|        - |  3508 | `/*` |
|        - |  3509 | ` * JZ: P1 P2 *` |
|        - |  3510 | ` *` |
|        - |  3511 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3512 | ` * entry in the stack if P1 is zero.` |
|        - |  3513 | ` */` |
|   547131 |  3514 | `case PH7_OP_JZ:` |
|        - |  3515 | `#ifdef UNTRUST` |
|        - |  3516 | `	if( pTos < pStack ){` |
|        - |  3517 | `		goto Abort;` |
|        - |  3518 | `	}` |
|        - |  3519 | `#endif` |
|        - |  3520 | `	/* Get a boolean value */` |
|  1094352 |  3521 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  3522 | `		PH7_MemObjToBool(pTos);` |
|       80 |  3523 | `	}` |
|  1094352 |  3524 | `	if( !pTos->x.iVal ){` |
|        - |  3525 | `		/* Take the jump */` |
|   558560 |  3526 | `		pc = pInstr->iP2 - 1;` |
|   279279 |  3527 | `	}` |
|  1094352 |  3528 | `	if( !pInstr->iP1 ){` |
|   868990 |  3529 | `		VmPopOperand(&pTos,1);` |
|   434516 |  3530 | `	}` |
|  1094352 |  3531 | `	break;` |
|        - |  3532 | `/*` |
|        - |  3533 | ` * JNZ: P1 P2 *` |
|        - |  3534 | ` *` |
|        - |  3535 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3536 | ` * entry in the stack if P1 is zero.` |
|        - |  3537 | ` */` |
|    57277 |  3538 | `case PH7_OP_JNZ:` |
|        - |  3539 | `#ifdef UNTRUST` |
|        - |  3540 | `	if( pTos < pStack ){` |
|        - |  3541 | `		goto Abort;` |
|        - |  3542 | `	}` |
|        - |  3543 | `#endif` |
|        - |  3544 | `	/* Get a boolean value */` |
|   114556 |  3545 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3546 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3547 | `	}` |
|   114556 |  3548 | `	if( pTos->x.iVal ){` |
|        - |  3549 | `		/* Take the jump */` |
|     5006 |  3550 | `		pc = pInstr->iP2 - 1;` |
|     2502 |  3551 | `	}` |
|   114556 |  3552 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3553 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3554 | `	}` |
|   114556 |  3555 | `	break;` |
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
|   423411 |  3569 | `case PH7_OP_POP: {` |
|   846868 |  3570 | `	sxi32 n = pInstr->iP1;` |
|   846868 |  3571 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3572 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3573 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3574 | `	}` |
|   846868 |  3575 | `	VmPopOperand(&pTos,n);` |
|   846868 |  3576 | `	break;` |
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
|     7095 |  3599 | `case PH7_OP_NSSWITCH:` |
|    14192 |  3600 | `	SyBlobReset(&pVm->sNamespace);` |
|    14192 |  3601 | `	if( pInstr->p3 ){` |
|       96 |  3602 | `		const char *zNs = (const char *)pInstr->p3;` |
|       96 |  3603 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       47 |  3604 | `	}` |
|        - |  3605 | `	/* Clear namespace-scoped use-const imports */` |
|    14192 |  3606 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    14192 |  3607 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    14192 |  3608 | `	break;` |
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
|    14374 |  3752 | `case PH7_OP_ERR_CTRL:` |
|        - |  3753 | `	/*` |
|        - |  3754 | `	 * TICKET 1433-038:` |
|        - |  3755 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3756 | `	 * use the public API,to control error output.` |
|        - |  3757 | `	 */` |
|    28748 |  3758 | `	break;` |
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
|   917371 |  3818 | `case PH7_OP_LOADC: {` |
|        - |  3819 | `	ph7_value *pObj;` |
|        - |  3820 | `	/* Reserve a room */` |
|  1834788 |  3821 | `	pTos++;` |
|  2743318 |  3822 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1834788 |  3823 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3824 | `			SyHashEntry *pEntry;` |
|        - |  3825 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3826 | `			{` |
|        - |  3827 | `				SyHashEntry *pConstImport;` |
|    26660 |  3828 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    17772 |  3829 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17774 |  3830 | `				if( pConstImport ){` |
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
|    17764 |  3845 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17764 |  3846 | `			if( pEntry ){` |
|    17760 |  3847 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3848 | `				/* Set a NULL default value */` |
|    17760 |  3849 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    17760 |  3850 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3851 | `				/* Invoke the callback and deal with the expanded value */` |
|    17760 |  3852 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3853 | `				/* Mark as constant */` |
|    17760 |  3854 | `				pTos->nIdx = SXU32_HIGH;` |
|    17760 |  3855 | `				break;` |
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
|  1817018 |  3903 | `		PH7_MemObjLoad(pObj,pTos);` |
|   908532 |  3904 | `	}else{` |
|        - |  3905 | `		/* Set a NULL value */` |
|      ! 0 |  3906 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3907 | `	}` |
|   908487 |  3908 | `LoadC_Done:` |
|        - |  3909 | `	/* Mark as constant */` |
|  1817020 |  3910 | `	pTos->nIdx = SXU32_HIGH;` |
|  1817020 |  3911 | `	break;` |
|        - |  3912 | `				  }` |
|        - |  3913 | `/*` |
|        - |  3914 | ` * LOAD: P1 * P3` |
|        - |  3915 | ` *` |
|        - |  3916 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3917 | ` * from the P3 operand.` |
|        - |  3918 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3919 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3920 | ` */` |
|  1456458 |  3921 | `case PH7_OP_LOAD:{` |
|        - |  3922 | `	ph7_value *pObj;` |
|        - |  3923 | `	SyString sName;` |
|  2913138 |  3924 | `	if( pInstr->p3 == 0 ){` |
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
|  2913120 |  3937 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3938 | `		/* Reserve a room for the target object */` |
|  2913120 |  3939 | `		pTos++;` |
|        - |  3940 | `	}` |
|        - |  3941 | `	/* Extract the requested memory object */` |
|  2913138 |  3942 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2913138 |  3943 | `	if( pObj == 0 ){` |
|       28 |  3944 | `		if( pInstr->iP1 ){` |
|        - |  3945 | `			/* Variable not found,load NULL */` |
|       28 |  3946 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3947 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3948 | `			}else{` |
|       28 |  3949 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3950 | `			}` |
|       28 |  3951 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1456473 |  3952 | `			break;` |
|      ! 0 |  3953 | `		}else{` |
|        - |  3954 | `			/* Fatal error */` |
|      ! 0 |  3955 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3956 | `			goto Abort;` |
|        - |  3957 | `		}` |
|        - |  3958 | `	}` |
|        - |  3959 | `	/* Load variable contents */` |
|  2913112 |  3960 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2913112 |  3961 | `	pTos->nIdx = pObj->nIdx;` |
|  2913112 |  3962 | `	break;` |
|        - |  3963 | `				   }` |
|        - |  3964 | `/*` |
|        - |  3965 | ` * LOAD_MAP P1 * *` |
|        - |  3966 | ` *` |
|        - |  3967 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3968 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3969 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3970 | ` */` |
|    20457 |  3971 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3972 | `	ph7_hashmap *pMap;` |
|        - |  3973 | `	/* Allocate a new hashmap instance */` |
|    40916 |  3974 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    40916 |  3975 | `	if( pMap == 0 ){` |
|      ! 0 |  3976 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3977 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3978 | `		goto Abort;` |
|        - |  3979 | `	}` |
|    40916 |  3980 | `	if( pInstr->iP1 > 0 ){` |
|     2366 |  3981 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3982 | `		/* Perform the insertion */` |
|     7252 |  3983 | `		while( pEntry < pTos ){` |
|     4888 |  3984 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3985 | `				/* Insertion by reference */` |
|      142 |  3986 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3987 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3988 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3989 | `					);` |
|       48 |  3990 | `			}else{` |
|        - |  3991 | `				/* Standard insertion */` |
|     7190 |  3992 | `				PH7_HashmapInsert(pMap,` |
|     4792 |  3993 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2396 |  3994 | `					&pEntry[1]` |
|        - |  3995 | `				);` |
|        - |  3996 | `			}` |
|        - |  3997 | `			/* Next pair on the stack */` |
|     4888 |  3998 | `			pEntry += 2;` |
|        2 |  3999 | `		}` |
|        - |  4000 | `		/* Pop P1 elements */` |
|     2366 |  4001 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1182 |  4002 | `	}` |
|        - |  4003 | `	/* Push the hashmap */` |
|    40916 |  4004 | `	pTos++;` |
|    40916 |  4005 | `	pTos->nIdx = SXU32_HIGH;` |
|    40916 |  4006 | `	pTos->x.pOther = pMap;` |
|    40916 |  4007 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    40916 |  4008 | `	break;` |
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
|   233934 |  4097 | `case PH7_OP_LOAD_IDX: {` |
|   467914 |  4098 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   467914 |  4099 | `	ph7_hashmap *pMap = 0;` |
|        - |  4100 | `	ph7_value *pIdx;` |
|   467914 |  4101 | `	pIdx = 0;` |
|   467914 |  4102 | `	if( pInstr->iP1 == 0 ){` |
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
|   467914 |  4119 | `		pIdx = pTos;` |
|   467914 |  4120 | `		pTos--;` |
|        - |  4121 | `	}` |
|   467914 |  4122 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4123 | `		/* String access */` |
|   365764 |  4124 | `		if( pIdx ){` |
|        - |  4125 | `			sxu32 nOfft;` |
|   365764 |  4126 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4127 | `				/* Force an int cast */` |
|      ! 0 |  4128 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4129 | `			}` |
|   365764 |  4130 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   365764 |  4131 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4132 | `				/* Invalid offset,load null */` |
|      ! 0 |  4133 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4134 | `			}else{` |
|   365764 |  4135 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   365764 |  4136 | `				int c = zData[nOfft];` |
|   365764 |  4137 | `				PH7_MemObjRelease(pTos);` |
|   365764 |  4138 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   365764 |  4139 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4140 | `			}` |
|   182905 |  4141 | `		}else{` |
|        - |  4142 | `			/* No available index,load NULL */` |
|      ! 0 |  4143 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4144 | `		}` |
|   365764 |  4145 | `		break;` |
|        - |  4146 | `	}` |
|   102152 |  4147 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4148 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4149 | `			ph7_value *pObj;` |
|        3 |  4150 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4151 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4152 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4153 | `			}` |
|        1 |  4154 | `		}` |
|        1 |  4155 | `	}` |
|   102152 |  4156 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   102152 |  4157 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   102152 |  4158 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4159 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4160 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4161 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4162 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  4163 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  4164 | `		}` |
|        - |  4165 | `		/* Point to the hashmap */` |
|   102152 |  4166 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   102152 |  4167 | `		if( pIdx ){` |
|        - |  4168 | `			/* Load the desired entry */` |
|   102152 |  4169 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    51075 |  4170 | `		}` |
|   102152 |  4171 | `		if( pInstr->iP2 == 3 ){` |
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
|   102152 |  4199 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4200 | `			/* Create a new empty entry */` |
|      273 |  4201 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4202 | `			if( rc == SXRET_OK ){` |
|        - |  4203 | `				/* Point to the last inserted entry */` |
|      273 |  4204 | `				pNode = pMap->pLast;` |
|      136 |  4205 | `			}` |
|      136 |  4206 | `		}` |
|    51075 |  4207 | `	}` |
|   102152 |  4208 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4209 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4210 | `		char zMsg[128];` |
|      ! 0 |  4211 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4212 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4213 | `		}` |
|      ! 0 |  4214 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4215 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4216 | `	}` |
|   102152 |  4217 | `	if( pIdx ){` |
|   102152 |  4218 | `		PH7_MemObjRelease(pIdx);` |
|    51075 |  4219 | `	}` |
|   102152 |  4220 | `	if( rc == SXRET_OK ){` |
|        - |  4221 | `		/* Load entry contents */` |
|    45920 |  4222 | `		if( pMap->iRef < 2 ){` |
|        - |  4223 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4224 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4225 | `			 */` |
|       24 |  4226 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4227 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4228 | `		}else{` |
|    45898 |  4229 | `			pTos->nIdx = pNode->nValIdx;` |
|    45898 |  4230 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    45898 |  4231 | `			PH7_HashmapUnref(pMap);` |
|        - |  4232 | `		}` |
|    22961 |  4233 | `	}else{` |
|        - |  4234 | `		/* No such entry,load NULL */` |
|    56234 |  4235 | `		PH7_MemObjRelease(pTos);` |
|    56234 |  4236 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4237 | `	}` |
|   102152 |  4238 | `	break;` |
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
|   127148 |  4318 | `case PH7_OP_STORE: {` |
|        - |  4319 | `	ph7_value *pObj;` |
|        - |  4320 | `	SyString sName;` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pTos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|   254298 |  4326 | `	if( pInstr->iP2 ){` |
|        - |  4327 | `		sxu32 nIdx;` |
|        - |  4328 | `		sxi32 rcT;` |
|        - |  4329 | `		/* Member store operation */` |
|     3644 |  4330 | `		nIdx = pTos->nIdx;` |
|     3644 |  4331 | `		VmPopOperand(&pTos,1);` |
|     3644 |  4332 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4333 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4334 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4335 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4336 | `		}else{` |
|        - |  4337 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4338 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     3640 |  4339 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     3640 |  4340 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4341 | `				goto Abort;` |
|        - |  4342 | `			}` |
|     3640 |  4343 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4344 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4345 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4346 | `				 * propagate out of the VM loop. */` |
|       35 |  4347 | `				VmPopOperand(&pTos,1);` |
|        - |  4348 | `				{` |
|       35 |  4349 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       35 |  4350 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       35 |  4351 | `						pc = pFrm2->iExceptionJump - 1;` |
|   127166 |  4352 | `						break;` |
|        - |  4353 | `					}` |
|        - |  4354 | `				}` |
|      ! 0 |  4355 | `				goto Exception;` |
|        - |  4356 | `			}` |
|        - |  4357 | `			/* Point to the desired memory object */` |
|     3606 |  4358 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3606 |  4359 | `			if( pObj ){` |
|        - |  4360 | `				/* Perform the store operation */` |
|     3606 |  4361 | `				PH7_MemObjStore(pTos,pObj);` |
|     1802 |  4362 | `			}` |
|        - |  4363 | `		}` |
|     3610 |  4364 | `		break;` |
|   250656 |  4365 | `	}else if( pInstr->p3 == 0 ){` |
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
|   250650 |  4379 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4380 | `	}` |
|        - |  4381 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   250656 |  4382 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   250656 |  4383 | `	if( pObj == 0 ){` |
|      ! 0 |  4384 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4385 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4386 | `		goto Abort;` |
|        - |  4387 | `	}` |
|   250656 |  4388 | `	if( !pInstr->p3 ){` |
|        7 |  4389 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4390 | `	}` |
|        - |  4391 | `	/* Perform the store operation */` |
|   250656 |  4392 | `	PH7_MemObjStore(pTos,pObj);` |
|   250656 |  4393 | `	break;` |
|        - |  4394 | `				   }` |
|        - |  4395 | `/*` |
|        - |  4396 | ` * STORE_IDX:   P1 * P3` |
|        - |  4397 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4398 | ` *` |
|        - |  4399 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4400 | ` */` |
|    89357 |  4401 | `case PH7_OP_STORE_IDX:` |
|        - |  4402 | `case PH7_OP_STORE_IDX_REF: {` |
|   178716 |  4403 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4404 | `	ph7_value *pKey;` |
|        - |  4405 | `	sxu32 nIdx;` |
|   178716 |  4406 | `	if( pInstr->iP1 ){` |
|        - |  4407 | `		/* Key is next on stack */` |
|    60304 |  4408 | `		pKey = pTos;` |
|    60304 |  4409 | `		pTos--;` |
|    30153 |  4410 | `	}else{` |
|   118414 |  4411 | `		pKey = 0;` |
|        - |  4412 | `	}` |
|   178716 |  4413 | `	nIdx = pTos->nIdx;` |
|   178716 |  4414 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4415 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4416 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4417 | `		 * checking true sharing count, then re-add after separation. */` |
|   178664 |  4418 | `		if( nIdx != SXU32_HIGH ){` |
|   178664 |  4419 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   267995 |  4420 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   178664 |  4421 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4422 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4423 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4424 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4425 | `				 * refcounts if the backing array was already separated. */` |
|   178664 |  4426 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   178664 |  4427 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   178664 |  4428 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   178664 |  4429 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   178664 |  4430 | `					pTos->x.pOther = pMap;` |
|    89333 |  4431 | `				}else{` |
|        - |  4432 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4433 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4434 | `					pMap = pCur;` |
|        - |  4435 | `				}` |
|    89333 |  4436 | `			}else{` |
|      ! 0 |  4437 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4438 | `			}` |
|    89333 |  4439 | `		}else{` |
|      ! 0 |  4440 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4441 | `		}` |
|   178664 |  4442 | `		if( pMap->iRef < 2 ){` |
|        - |  4443 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4444 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4445 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4446 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4447 | `			pMap->iRef = 2;` |
|      ! 0 |  4448 | `		}` |
|    89333 |  4449 | `	}else{` |
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
|   178664 |  4504 | `	VmPopOperand(&pTos,1);` |
|        - |  4505 | `	/* Phase#2: Perform the insertion */` |
|   178664 |  4506 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4507 | `		/* Insertion by reference */` |
|       15 |  4508 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4509 | `	}else{` |
|   178650 |  4510 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4511 | `	}` |
|   178664 |  4512 | `	if( pKey ){` |
|    60254 |  4513 | `		PH7_MemObjRelease(pKey);` |
|    30126 |  4514 | `	}` |
|   178664 |  4515 | `	break;` |
|        - |  4516 | `					   }` |
|        - |  4517 | `/*` |
|        - |  4518 | ` * INCR: P1 * *` |
|        - |  4519 | ` *` |
|        - |  4520 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4521 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4522 | ` * the stack and increment after that.` |
|        - |  4523 | ` */` |
|   159703 |  4524 | `case PH7_OP_INCR:` |
|        - |  4525 | `#ifdef UNTRUST` |
|        - |  4526 | `	if( pTos < pStack ){` |
|        - |  4527 | `		goto Abort;` |
|        - |  4528 | `	}` |
|        - |  4529 | `#endif` |
|   319452 |  4530 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   319452 |  4531 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4532 | `			ph7_value *pObj;` |
|   319452 |  4533 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4534 | `				/* Force a numeric cast */` |
|   319452 |  4535 | `				PH7_MemObjToNumeric(pObj);` |
|   319452 |  4536 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4537 | `					pObj->rVal++;` |
|        - |  4538 | `					/* Try to get an integer representation */` |
|      ! 0 |  4539 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4540 | `				}else{` |
|   319452 |  4541 | `					pObj->x.iVal++;` |
|   319452 |  4542 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4543 | `				}` |
|   319452 |  4544 | `				if( pInstr->iP1 ){` |
|        - |  4545 | `					/* Pre-icrement */` |
|       77 |  4546 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4547 | `				}` |
|   159747 |  4548 | `			}` |
|   159749 |  4549 | `		}else{` |
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
|   159747 |  4564 | `	}` |
|   319452 |  4565 | `	break;` |
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
|    26613 |  4620 | `case PH7_OP_UMINUS:` |
|        - |  4621 | `#ifdef UNTRUST` |
|        - |  4622 | `	if( pTos < pStack ){` |
|        - |  4623 | `		goto Abort;` |
|        - |  4624 | `	}` |
|        - |  4625 | `#endif` |
|        - |  4626 | `	/* Force a numeric (integer,real or both) cast */` |
|    53228 |  4627 | `	PH7_MemObjToNumeric(pTos);` |
|    53228 |  4628 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4629 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4630 | `	}` |
|    53228 |  4631 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    53198 |  4632 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    26598 |  4633 | `	}` |
|    53228 |  4634 | `	break;` |
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
|    42311 |  4661 | `case PH7_OP_LNOT:` |
|        - |  4662 | `#ifdef UNTRUST` |
|        - |  4663 | `	if( pTos < pStack ){` |
|        - |  4664 | `		goto Abort;` |
|        - |  4665 | `	}` |
|        - |  4666 | `#endif` |
|        - |  4667 | `	/* Force a boolean cast */` |
|    84668 |  4668 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4669 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4670 | `	}` |
|    84668 |  4671 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    84668 |  4672 | `	break;` |
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
|      488 |  4753 | `case PH7_OP_ADD:{` |
|      978 |  4754 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4755 | `#ifdef UNTRUST` |
|        - |  4756 | `	if( pNos < pStack ){` |
|        - |  4757 | `		goto Abort;` |
|        - |  4758 | `	}` |
|        - |  4759 | `#endif` |
|        - |  4760 | `	/* Perform the addition */` |
|      978 |  4761 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      978 |  4762 | `	VmPopOperand(&pTos,1);` |
|      978 |  4763 | `	break;` |
|        - |  4764 | `				}` |
|        - |  4765 | `/*` |
|        - |  4766 | ` * OP_ADD_STORE * * *` |
|        - |  4767 | ` *` |
|        - |  4768 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4769 | ` * and push the result back onto the stack.` |
|        - |  4770 | ` */` |
|      497 |  4771 | `case PH7_OP_ADD_STORE:{` |
|      996 |  4772 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4773 | `	ph7_value *pObj;` |
|        - |  4774 | `	sxu32 nIdx;` |
|        - |  4775 | `#ifdef UNTRUST` |
|        - |  4776 | `	if( pNos < pStack ){` |
|        - |  4777 | `		goto Abort;` |
|        - |  4778 | `	}` |
|        - |  4779 | `#endif` |
|        - |  4780 | `	/* Perform the addition */` |
|      996 |  4781 | `	nIdx = pTos->nIdx;` |
|      996 |  4782 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4783 | `	/* Peform the store operation */` |
|      996 |  4784 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4785 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      996 |  4786 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      996 |  4787 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      996 |  4788 | `		PH7_MemObjStore(pTos,pObj);` |
|      497 |  4789 | `	}` |
|        - |  4790 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      996 |  4791 | `	PH7_MemObjStore(pTos,pNos);` |
|      996 |  4792 | `	VmPopOperand(&pTos,1);` |
|      996 |  4793 | `	break;` |
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
|    67203 |  5282 | `case PH7_OP_CAT:{` |
|        - |  5283 | `	ph7_value *pNos,*pCur;` |
|   134408 |  5284 | `	if( pInstr->iP1 < 1 ){` |
|   107144 |  5285 | `		pNos = &pTos[-1];` |
|    53573 |  5286 | `	}else{` |
|    27266 |  5287 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5288 | `	}` |
|        - |  5289 | `#ifdef UNTRUST` |
|        - |  5290 | `	if( pNos < pStack ){` |
|        - |  5291 | `		goto Abort;` |
|        - |  5292 | `	}` |
|        - |  5293 | `#endif` |
|        - |  5294 | `	/* Force a string cast */` |
|   134408 |  5295 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5296 | `		PH7_MemObjToString(pNos);` |
|      817 |  5297 | `	}` |
|   134408 |  5298 | `	pCur = &pNos[1];` |
|   271352 |  5299 | `	while( pCur <= pTos ){` |
|   136946 |  5300 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50868 |  5301 | `			PH7_MemObjToString(pCur);` |
|    25433 |  5302 | `		}` |
|        - |  5303 | `		/* Perform the concatenation */` |
|   136946 |  5304 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   136904 |  5305 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    68451 |  5306 | `		}` |
|   136946 |  5307 | `		SyBlobRelease(&pCur->sBlob);` |
|   136946 |  5308 | `		pCur++;` |
|        2 |  5309 | `	}` |
|   134408 |  5310 | `	pTos = pNos;` |
|   134408 |  5311 | `	break;` |
|        - |  5312 | `				}` |
|        - |  5313 | `/*  CAT_STORE: * * *` |
|        - |  5314 | ` *` |
|        - |  5315 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5316 | ` * back.` |
|        - |  5317 | ` */` |
|     3682 |  5318 | `case PH7_OP_CAT_STORE:{` |
|     7366 |  5319 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5320 | `	ph7_value *pObj;` |
|        - |  5321 | `#ifdef UNTRUST` |
|        - |  5322 | `	if( pNos < pStack ){` |
|        - |  5323 | `		goto Abort;` |
|        - |  5324 | `	}` |
|        - |  5325 | `#endif` |
|     7366 |  5326 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5327 | `		/* Force a string cast */` |
|        3 |  5328 | `		PH7_MemObjToString(pTos);` |
|        1 |  5329 | `	}` |
|     7366 |  5330 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5331 | `		/* Force a string cast */` |
|      ! 0 |  5332 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5333 | `	}` |
|        - |  5334 | `	/* Perform the concatenation (Reverse order) */` |
|     7366 |  5335 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7366 |  5336 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3682 |  5337 | `	}` |
|        - |  5338 | `	/* Perform the store operation */` |
|     7366 |  5339 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5340 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7366 |  5341 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7366 |  5342 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7364 |  5343 | `		PH7_MemObjStore(pTos,pObj);` |
|     3681 |  5344 | `	}` |
|     7364 |  5345 | `	PH7_MemObjStore(pTos,pNos);` |
|     7364 |  5346 | `	VmPopOperand(&pTos,1);` |
|     7364 |  5347 | `	break;` |
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
|   101366 |  5361 | `case PH7_OP_LAND:` |
|        - |  5362 | `case PH7_OP_LOR: {` |
|   202778 |  5363 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5364 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5365 | `#ifdef UNTRUST` |
|        - |  5366 | `	if( pNos < pStack ){` |
|        - |  5367 | `		goto Abort;` |
|        - |  5368 | `	}` |
|        - |  5369 | `#endif` |
|        - |  5370 | `	/* Force a boolean cast */` |
|   202778 |  5371 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5372 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5373 | `	}` |
|   202778 |  5374 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5375 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5376 | `	}` |
|   202778 |  5377 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   202778 |  5378 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   202778 |  5379 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5380 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    93228 |  5381 | `		v1 = and_logic[v1*3+v2];` |
|    46637 |  5382 | `	}else{` |
|        - |  5383 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   109552 |  5384 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5385 | `	}` |
|   202778 |  5386 | `	if( v1 == 2 ){` |
|      ! 0 |  5387 | `		v1 = 1;` |
|      ! 0 |  5388 | `	}` |
|   202778 |  5389 | `	VmPopOperand(&pTos,1);` |
|   202778 |  5390 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   202778 |  5391 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   202778 |  5392 | `	break;` |
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
|       19 |  5406 | `case PH7_OP_NULLC: {` |
|        - |  5407 | `#ifdef UNTRUST` |
|        - |  5408 | `	if( pTos < pStack ){` |
|        - |  5409 | `		goto Abort;` |
|        - |  5410 | `	}` |
|        - |  5411 | `#endif` |
|       40 |  5412 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5413 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  5414 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  5415 | `	}else{` |
|        - |  5416 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  5417 | `		VmPopOperand(&pTos, 1);` |
|        - |  5418 | `	}` |
|       40 |  5419 | `	break;` |
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
|       14 |  5446 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5447 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5448 | `	ph7_value *pObj;` |
|        - |  5449 | `	sxu32 nIdx;` |
|        - |  5450 | `#ifdef UNTRUST` |
|        - |  5451 | `	if( pNos < pStack ){` |
|        - |  5452 | `		goto Abort;` |
|        - |  5453 | `	}` |
|        - |  5454 | `#endif` |
|       29 |  5455 | `	nIdx = pNos->nIdx;` |
|       29 |  5456 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5457 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5458 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5459 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5460 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5461 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5462 | `	}` |
|       29 |  5463 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5464 | `	VmPopOperand(&pTos,1);` |
|       29 |  5465 | `	break;` |
|        - |  5466 |  |
|        - |  5467 | `/*` |
|        - |  5468 | ` * OP_SPREAD: * * *` |
|        - |  5469 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5470 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5471 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5472 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5473 | ` */` |
|        9 |  5474 | `case PH7_OP_SPREAD: {` |
|        - |  5475 | `#ifdef UNTRUST` |
|        - |  5476 | `	if( pTos < pStack ){` |
|        - |  5477 | `		goto Abort;` |
|        - |  5478 | `	}` |
|        - |  5479 | `#endif` |
|       20 |  5480 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  5481 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  5482 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  5483 | `		if( nEntry == 0 ){` |
|        - |  5484 | `			/* Empty array — remove from stack */` |
|        3 |  5485 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5486 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  5487 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5488 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5489 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5490 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5491 | `				VM_STACK_GUARD);` |
|      ! 0 |  5492 | `		}else{` |
|        - |  5493 | `			ph7_hashmap_node *pNode2;` |
|        - |  5494 | `			ph7_value *pElem;` |
|        - |  5495 | `			sxu32 i;` |
|        - |  5496 | `			/* Overwrite TOS with first element */` |
|       18 |  5497 | `			pNode2 = pMap->pFirst;` |
|       18 |  5498 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  5499 | `			PH7_MemObjRelease(pTos);` |
|       18 |  5500 | `			if( pElem ){` |
|       18 |  5501 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  5502 | `			}` |
|       18 |  5503 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5504 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5505 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  5506 | `			pNode2 = pNode2->pPrev;` |
|        - |  5507 | `			/* Push remaining elements */` |
|       44 |  5508 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  5509 | `				pTos++;` |
|       28 |  5510 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  5511 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  5512 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  5513 | `				if( pElem ){` |
|       28 |  5514 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  5515 | `				}` |
|       28 |  5516 | `				pNode2 = pNode2->pPrev;` |
|       15 |  5517 | `			}` |
|       18 |  5518 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5519 | `		}` |
|        9 |  5520 | `	}` |
|        - |  5521 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  5522 | `	break;` |
|        - |  5523 |  |
|        - |  5524 | `/* OP_LXOR: * * *` |
|        - |  5525 | ` *` |
|        - |  5526 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5527 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5528 | ` * stack.` |
|        - |  5529 | ` * According to the PHP language reference manual:` |
|        - |  5530 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5531 | ` *  TRUE,but not both.` |
|        - |  5532 | ` */` |
|        5 |  5533 | `case PH7_OP_LXOR:{` |
|       11 |  5534 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5535 | `	sxi32 v = 0;` |
|        - |  5536 | `#ifdef UNTRUST` |
|        - |  5537 | `	if( pNos < pStack ){` |
|        - |  5538 | `		goto Abort;` |
|        - |  5539 | `	}` |
|        - |  5540 | `#endif` |
|        - |  5541 | `	/* Force a boolean cast */` |
|       11 |  5542 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5543 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5544 | `	}` |
|       11 |  5545 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5546 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5547 | `	}` |
|       11 |  5548 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5549 | `		v = 1;` |
|        3 |  5550 | `	}` |
|       11 |  5551 | `	VmPopOperand(&pTos,1);` |
|       11 |  5552 | `	pTos->x.iVal = v;` |
|       11 |  5553 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5554 | `	break;` |
|        - |  5555 | `				 }` |
|        - |  5556 | `/* OP_EQ P1 P2 P3` |
|        - |  5557 | ` *` |
|        - |  5558 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5559 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5560 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5561 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5562 | ` */` |
|        - |  5563 | `/* OP_NEQ P1 P2 P3` |
|        - |  5564 | ` *` |
|        - |  5565 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5566 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5567 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5568 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5569 | ` */` |
|     4233 |  5570 | `case PH7_OP_EQ:` |
|        - |  5571 | `case PH7_OP_NEQ: {` |
|     8468 |  5572 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5573 | `	/* Perform the comparison and act accordingly */` |
|        - |  5574 | `#ifdef UNTRUST` |
|        - |  5575 | `	if( pNos < pStack ){` |
|        - |  5576 | `		goto Abort;` |
|        - |  5577 | `	}` |
|        - |  5578 | `#endif` |
|     8468 |  5579 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8468 |  5580 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5581 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8459 |  5582 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8424 |  5583 | `		rc = rc == 0;` |
|     4213 |  5584 | `	}else{` |
|       28 |  5585 | `		rc = rc != 0;` |
|        - |  5586 | `	}` |
|     8468 |  5587 | `	VmPopOperand(&pTos,1);` |
|     8468 |  5588 | `	if( !pInstr->iP2 ){` |
|        - |  5589 | `		/* Push comparison result without taking the jump */` |
|     8468 |  5590 | `		PH7_MemObjRelease(pTos);` |
|     8468 |  5591 | `		pTos->x.iVal = rc;` |
|        - |  5592 | `		/* Invalidate any prior representation */` |
|     8468 |  5593 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4235 |  5594 | `	}else{` |
|      ! 0 |  5595 | `		if( rc ){` |
|        - |  5596 | `			/* Jump to the desired location */` |
|      ! 0 |  5597 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5598 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5599 | `		}` |
|        - |  5600 | `	}` |
|     8468 |  5601 | `	break;` |
|        - |  5602 | `				 }` |
|        - |  5603 | `/* OP_TEQ P1 P2 *` |
|        - |  5604 | ` *` |
|        - |  5605 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5606 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5607 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5608 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5609 | ` */` |
|   146900 |  5610 | `case PH7_OP_TEQ: {` |
|   293802 |  5611 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5612 | `	/* Perform the comparison and act accordingly */` |
|        - |  5613 | `#ifdef UNTRUST` |
|        - |  5614 | `	if( pNos < pStack ){` |
|        - |  5615 | `		goto Abort;` |
|        - |  5616 | `	}` |
|        - |  5617 | `#endif` |
|   293802 |  5618 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   293802 |  5619 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5620 | `		rc = 0;` |
|        2 |  5621 | `	}else{` |
|   293800 |  5622 | `		rc = rc == 0;` |
|        - |  5623 | `	}` |
|   293802 |  5624 | `	VmPopOperand(&pTos,1);` |
|   293802 |  5625 | `	if( !pInstr->iP2 ){` |
|        - |  5626 | `		/* Push comparison result without taking the jump */` |
|   293802 |  5627 | `		PH7_MemObjRelease(pTos);` |
|   293802 |  5628 | `		pTos->x.iVal = rc;` |
|        - |  5629 | `		/* Invalidate any prior representation */` |
|   293802 |  5630 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   146902 |  5631 | `	}else{` |
|      ! 0 |  5632 | `		if( rc ){` |
|        - |  5633 | `			/* Jump to the desired location */` |
|      ! 0 |  5634 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5635 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5636 | `		}` |
|        - |  5637 | `	}` |
|   293802 |  5638 | `	break;` |
|        - |  5639 | `				 }` |
|        - |  5640 | `/* OP_TNE P1 P2 *` |
|        - |  5641 | ` *` |
|        - |  5642 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5643 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5644 | ` * instruction.` |
|        - |  5645 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5646 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5647 | ` *` |
|        - |  5648 | ` */` |
|   113380 |  5649 | `case PH7_OP_TNE: {` |
|   226762 |  5650 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5651 | `	/* Perform the comparison and act accordingly */` |
|        - |  5652 | `#ifdef UNTRUST` |
|        - |  5653 | `	if( pNos < pStack ){` |
|        - |  5654 | `		goto Abort;` |
|        - |  5655 | `	}` |
|        - |  5656 | `#endif` |
|   226762 |  5657 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   226762 |  5658 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5659 | `		rc = 1;` |
|        2 |  5660 | `	}else{` |
|   226760 |  5661 | `		rc = rc != 0;` |
|        - |  5662 | `	}` |
|   226762 |  5663 | `	VmPopOperand(&pTos,1);` |
|   226762 |  5664 | `	if( !pInstr->iP2 ){` |
|        - |  5665 | `		/* Push comparison result without taking the jump */` |
|   226762 |  5666 | `		PH7_MemObjRelease(pTos);` |
|   226762 |  5667 | `		pTos->x.iVal = rc;` |
|        - |  5668 | `		/* Invalidate any prior representation */` |
|   226762 |  5669 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   113382 |  5670 | `	}else{` |
|      ! 0 |  5671 | `		if( rc ){` |
|        - |  5672 | `			/* Jump to the desired location */` |
|      ! 0 |  5673 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5674 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5675 | `		}` |
|        - |  5676 | `	}` |
|   226762 |  5677 | `	break;` |
|        - |  5678 | `				 }` |
|        - |  5679 | `/* OP_LT P1 P2 P3` |
|        - |  5680 | ` *` |
|        - |  5681 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5682 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5683 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5684 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5685 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5686 | ` *` |
|        - |  5687 | ` */` |
|        - |  5688 | `/* OP_LE P1 P2 P3` |
|        - |  5689 | ` *` |
|        - |  5690 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5691 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5692 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5693 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5694 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5695 | ` *` |
|        - |  5696 | ` */` |
|   107392 |  5697 | `case PH7_OP_LT:` |
|        - |  5698 | `case PH7_OP_LE: {` |
|   214830 |  5699 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5700 | `	/* Perform the comparison and act accordingly */` |
|        - |  5701 | `#ifdef UNTRUST` |
|        - |  5702 | `	if( pNos < pStack ){` |
|        - |  5703 | `		goto Abort;` |
|        - |  5704 | `	}` |
|        - |  5705 | `#endif` |
|   214830 |  5706 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   214830 |  5707 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5708 | `		rc = 0;` |
|   214826 |  5709 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      784 |  5710 | `		rc = rc < 1;` |
|      393 |  5711 | `	}else{` |
|   214040 |  5712 | `		rc = rc < 0;` |
|        - |  5713 | `	}` |
|   214830 |  5714 | `	VmPopOperand(&pTos,1);` |
|   214830 |  5715 | `	if( !pInstr->iP2 ){` |
|        - |  5716 | `		/* Push comparison result without taking the jump */` |
|   214830 |  5717 | `		PH7_MemObjRelease(pTos);` |
|   214830 |  5718 | `		pTos->x.iVal = rc;` |
|        - |  5719 | `		/* Invalidate any prior representation */` |
|   214830 |  5720 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   107438 |  5721 | `	}else{` |
|      ! 0 |  5722 | `		if( rc ){` |
|        - |  5723 | `			/* Jump to the desired location */` |
|      ! 0 |  5724 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5725 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5726 | `		}` |
|        - |  5727 | `	}` |
|   214830 |  5728 | `	break;` |
|        - |  5729 | `				}` |
|        - |  5730 | `/* OP_GT P1 P2 P3` |
|        - |  5731 | ` *` |
|        - |  5732 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5733 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5734 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5735 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5736 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5737 | ` *` |
|        - |  5738 | ` */` |
|        - |  5739 | `/* OP_GE P1 P2 P3` |
|        - |  5740 | ` *` |
|        - |  5741 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5742 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5743 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5744 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5745 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5746 | ` *` |
|        - |  5747 | ` */` |
|    52172 |  5748 | `case PH7_OP_GT:` |
|        - |  5749 | `case PH7_OP_GE: {` |
|   104346 |  5750 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5751 | `	/* Perform the comparison and act accordingly */` |
|        - |  5752 | `#ifdef UNTRUST` |
|        - |  5753 | `	if( pNos < pStack ){` |
|        - |  5754 | `		goto Abort;` |
|        - |  5755 | `	}` |
|        - |  5756 | `#endif` |
|   104346 |  5757 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   104346 |  5758 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5759 | `		rc = 0;` |
|   104342 |  5760 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   104178 |  5761 | `		rc = rc >= 0;` |
|    52090 |  5762 | `	}else{` |
|      162 |  5763 | `		rc = rc > 0;` |
|        - |  5764 | `	}` |
|   104346 |  5765 | `	VmPopOperand(&pTos,1);` |
|   104346 |  5766 | `	if( !pInstr->iP2 ){` |
|        - |  5767 | `		/* Push comparison result without taking the jump */` |
|   104346 |  5768 | `		PH7_MemObjRelease(pTos);` |
|   104346 |  5769 | `		pTos->x.iVal = rc;` |
|        - |  5770 | `		/* Invalidate any prior representation */` |
|   104346 |  5771 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    52174 |  5772 | `	}else{` |
|      ! 0 |  5773 | `		if( rc ){` |
|        - |  5774 | `			/* Jump to the desired location */` |
|      ! 0 |  5775 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5776 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5777 | `		}` |
|        - |  5778 | `	}` |
|   104346 |  5779 | `	break;` |
|        - |  5780 | `				}` |
|        - |  5781 | `/* OP_SPACESHIP * * *` |
|        - |  5782 | ` *` |
|        - |  5783 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5784 | ` *   -1 if left < right` |
|        - |  5785 | ` *    0 if left == right` |
|        - |  5786 | ` *    1 if left > right` |
|        - |  5787 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5788 | ` */` |
|       25 |  5789 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5790 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5791 | `#ifdef UNTRUST` |
|        - |  5792 | `	if( pNos < pStack ){` |
|        - |  5793 | `		goto Abort;` |
|        - |  5794 | `	}` |
|        - |  5795 | `#endif` |
|       51 |  5796 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5797 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5798 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5799 | `		rc = 1;` |
|        4 |  5800 | `	}else{` |
|        - |  5801 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5802 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5803 | `	}` |
|       51 |  5804 | `	VmPopOperand(&pTos,1);` |
|       51 |  5805 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5806 | `	pTos->x.iVal = rc;` |
|       51 |  5807 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5808 | `	break;` |
|        - |  5809 | `				}` |
|        - |  5810 | `/* OP_SEQ P1 P2 *` |
|        - |  5811 | ` * Strict string comparison.` |
|        - |  5812 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5813 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5814 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5815 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5816 | ` * use PH7_OP_EQ.` |
|        - |  5817 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5818 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5819 | ` */` |
|        - |  5820 | `/* OP_SNE P1 P2 *` |
|        - |  5821 | ` * Strict string comparison.` |
|        - |  5822 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5823 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5824 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5825 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5826 | ` * use PH7_OP_EQ.` |
|        - |  5827 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5828 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5829 | ` */` |
|       18 |  5830 | `case PH7_OP_SEQ:` |
|        - |  5831 | `case PH7_OP_SNE: {` |
|       38 |  5832 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5833 | `	SyString s1,s2;` |
|        - |  5834 | `	/* Perform the comparison and act accordingly */` |
|        - |  5835 | `#ifdef UNTRUST` |
|        - |  5836 | `	if( pNos < pStack ){` |
|        - |  5837 | `		goto Abort;` |
|        - |  5838 | `	}` |
|        - |  5839 | `#endif` |
|        - |  5840 | `	/* Force a string cast */` |
|       38 |  5841 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5842 | `		PH7_MemObjToString(pTos);` |
|        2 |  5843 | `	}` |
|       38 |  5844 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5845 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5846 | `	}` |
|       38 |  5847 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5848 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5849 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5850 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5851 | `		rc = rc != 0;` |
|      ! 0 |  5852 | `	}else{` |
|       38 |  5853 | `		rc = rc == 0;` |
|        - |  5854 | `	}` |
|       38 |  5855 | `	VmPopOperand(&pTos,1);` |
|       38 |  5856 | `	if( !pInstr->iP2 ){` |
|        - |  5857 | `		/* Push comparison result without taking the jump */` |
|       38 |  5858 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5859 | `		pTos->x.iVal = rc;` |
|        - |  5860 | `		/* Invalidate any prior representation */` |
|       38 |  5861 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5862 | `	}else{` |
|      ! 0 |  5863 | `		if( rc ){` |
|        - |  5864 | `			/* Jump to the desired location */` |
|      ! 0 |  5865 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5866 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5867 | `		}` |
|        - |  5868 | `	}` |
|       38 |  5869 | `	break;` |
|        - |  5870 | `				 }` |
|        - |  5871 | `/*` |
|        - |  5872 | ` * OP_LOAD_REF * * *` |
|        - |  5873 | ` * Push the index of a referenced object on the stack.` |
|        - |  5874 | ` */` |
|       57 |  5875 | `case PH7_OP_LOAD_REF: {` |
|        - |  5876 | `	sxu32 nIdx;` |
|        - |  5877 | `#ifdef UNTRUST` |
|        - |  5878 | `	if( pTos < pStack ){` |
|        - |  5879 | `		goto Abort;` |
|        - |  5880 | `	}` |
|        - |  5881 | `#endif` |
|        - |  5882 | `	/* Extract memory object index */` |
|      115 |  5883 | `	nIdx = pTos->nIdx;` |
|      115 |  5884 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5885 | `		/* Nullify the object */` |
|       95 |  5886 | `		PH7_MemObjRelease(pTos);` |
|        - |  5887 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5888 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5889 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5890 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5891 | `	}` |
|      115 |  5892 | `	break;` |
|        - |  5893 | `					  }` |
|        - |  5894 | `/*` |
|        - |  5895 | ` * OP_STORE_REF * * P3` |
|        - |  5896 | ` * Perform an assignment operation by reference.` |
|        - |  5897 | ` */` |
|       16 |  5898 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5899 | `	 SyString sName = { 0 , 0 };` |
|        - |  5900 | `	 VmFrame *pFrameLocal;` |
|        - |  5901 | `	SyHashEntry *pEntry;` |
|        - |  5902 | `	sxu32 nIdx;` |
|        - |  5903 | `#ifdef UNTRUST` |
|        - |  5904 | `	if( pTos < pStack ){` |
|        - |  5905 | `		goto Abort;` |
|        - |  5906 | `	}` |
|        - |  5907 | `#endif` |
|       34 |  5908 | `	if( pInstr->p3 == 0 ){` |
|        - |  5909 | `		char *zName;` |
|        - |  5910 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5911 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5912 | `			/* Force a string cast */` |
|      ! 0 |  5913 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5914 | `		}` |
|      ! 0 |  5915 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5916 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5917 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5918 | `			if( zName ){` |
|      ! 0 |  5919 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5920 | `			}` |
|      ! 0 |  5921 | `		}` |
|      ! 0 |  5922 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5923 | `		pTos--;` |
|      ! 0 |  5924 | `	}else{` |
|       34 |  5925 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5926 | `	}` |
|       34 |  5927 | `	nIdx = pTos->nIdx;` |
|       34 |  5928 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5929 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5930 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5931 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5932 | `		}else{` |
|        - |  5933 | `			ph7_value *pObj;` |
|        - |  5934 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5935 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5936 | `			if( pObj == 0 ){` |
|      ! 0 |  5937 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5938 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5939 | `				goto Abort;` |
|        - |  5940 | `			}` |
|        - |  5941 | `			/* Perform the store operation */` |
|      ! 0 |  5942 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5943 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5944 | `		}` |
|       34 |  5945 | `	}else if( sName.nByte > 0){` |
|       34 |  5946 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5947 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5948 | `		}else{` |
|       34 |  5949 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  5950 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5951 | `			/* Query the local frame */` |
|       34 |  5952 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  5953 | `			if( pEntry ){` |
|      ! 0 |  5954 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5955 | `			}else{` |
|       34 |  5956 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  5957 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5958 | `					/* Insert in the $GLOBALS array */` |
|       30 |  5959 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  5960 | `				}` |
|       34 |  5961 | `				if( rc == SXRET_OK ){` |
|       34 |  5962 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  5963 | `				}` |
|        - |  5964 | `			}` |
|        - |  5965 | `		}` |
|       16 |  5966 | `	}` |
|       34 |  5967 | `	break;` |
|        - |  5968 | `				 }` |
|        - |  5969 | `/*` |
|        - |  5970 | ` * OP_UPLINK P1 * *` |
|        - |  5971 | ` * Link a variable to the top active VM frame.` |
|        - |  5972 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5973 | ` */` |
|       27 |  5974 | `case PH7_OP_UPLINK: {` |
|       56 |  5975 | `	if( pVm->pFrame->pParent ){` |
|       56 |  5976 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5977 | `		SyString sName;` |
|        - |  5978 | `		/* Perform the link */` |
|      112 |  5979 | `		while( pLink <= pTos ){` |
|       58 |  5980 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5981 | `				/* Force a string cast */` |
|      ! 0 |  5982 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5983 | `			}` |
|       58 |  5984 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       58 |  5985 | `			if( sName.nByte > 0 ){` |
|       58 |  5986 | `				VmFrameLink(&(*pVm),&sName);` |
|       28 |  5987 | `			}` |
|       58 |  5988 | `			pLink++;` |
|        2 |  5989 | `		}` |
|       27 |  5990 | `	}` |
|       56 |  5991 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       56 |  5992 | `	break;` |
|        - |  5993 | `					}` |
|        - |  5994 | `/*` |
|        - |  5995 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5996 | ` * Push an exception in the corresponding container so that` |
|        - |  5997 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5998 | ` */` |
|       79 |  5999 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      160 |  6000 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6001 | `	VmFrame *pFrameLocal;` |
|        - |  6002 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      160 |  6003 | `	pException->iFinallyDone = 0;` |
|      160 |  6004 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6005 | `	/* Create the exception frame */` |
|      160 |  6006 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      160 |  6007 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6008 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6009 | `		goto Abort;` |
|        - |  6010 | `	}` |
|        - |  6011 | `	/* Mark the special frame */` |
|      160 |  6012 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      160 |  6013 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6014 | `	/* Point to the frame that trigger the exception */` |
|      160 |  6015 | `	pFrameLocal = pFrameLocal->pParent;` |
|      160 |  6016 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      160 |  6017 | `	pException->pFrame = pFrameLocal;` |
|      160 |  6018 | `	break;` |
|        - |  6019 | `							}` |
|        - |  6020 | `/*` |
|        - |  6021 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6022 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6023 | ` */` |
|       78 |  6024 | `case PH7_OP_POP_EXCEPTION: {` |
|      158 |  6025 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      158 |  6026 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6027 | `		ph7_exception **apException;` |
|        - |  6028 | `		/* Pop the loaded exception */` |
|       28 |  6029 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  6030 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  6031 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  6032 | `		}` |
|       13 |  6033 | `	}` |
|      158 |  6034 | `	pException->pFrame = 0;` |
|        - |  6035 | `	/* Leave the exception frame */` |
|      158 |  6036 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6037 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      158 |  6038 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6039 | `		sxi32 rcFinally;` |
|       20 |  6040 | `		pException->iFinallyDone = 1;` |
|       20 |  6041 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6042 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6043 | `			goto Abort;` |
|        - |  6044 | `		}` |
|        9 |  6045 | `	}` |
|      158 |  6046 | `	break;` |
|        - |  6047 | `							}` |
|        - |  6048 |  |
|        - |  6049 | `/*` |
|        - |  6050 | ` * OP_THROW * P2 *` |
|        - |  6051 | ` * Throw an user exception.` |
|        - |  6052 | ` */` |
|       30 |  6053 | `case PH7_OP_THROW: {` |
|       62 |  6054 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  6055 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6056 | `#ifdef UNTRUST` |
|        - |  6057 | `	if( pTos < pStack ){` |
|        - |  6058 | `		goto Abort;` |
|        - |  6059 | `	}` |
|        - |  6060 | `#endif` |
|       62 |  6061 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6062 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  6063 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  6064 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  6065 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6066 | `		ph7_class *pException;` |
|        - |  6067 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  6068 | `		 */` |
|       62 |  6069 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  6070 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  6071 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  6072 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6073 | `			if( rc == SXERR_ABORT ){` |
|        - |  6074 | `				/* Abort processing immediately */` |
|      ! 0 |  6075 | `				goto Abort;` |
|        - |  6076 | `			}` |
|      ! 0 |  6077 | `		}else{` |
|        - |  6078 | `			/* Throw the exception */` |
|       62 |  6079 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  6080 | `			if( rc == SXERR_ABORT ){` |
|        - |  6081 | `				/* Abort processing immediately */` |
|        9 |  6082 | `				goto Abort;` |
|        - |  6083 | `			}` |
|        - |  6084 | `		}` |
|       28 |  6085 | `	}else{` |
|        - |  6086 | `		/* Expecting a class instance */` |
|      ! 0 |  6087 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6088 | `		if( rc == SXERR_ABORT ){` |
|        - |  6089 | `			/* Abort processing immediately */` |
|      ! 0 |  6090 | `			goto Abort;` |
|        - |  6091 | `		}` |
|        - |  6092 | `	}` |
|        - |  6093 | `	/* Pop the top entry */` |
|       54 |  6094 | `	VmPopOperand(&pTos,1);` |
|        - |  6095 | `	/* Perform an unconditional jump */` |
|       54 |  6096 | `	pc = nJump - 1;` |
|       54 |  6097 | `	break;` |
|        - |  6098 | `				   }` |
|        - |  6099 | `/*` |
|        - |  6100 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6101 | ` * Prepare a foreach step.` |
|        - |  6102 | ` */` |
|     5544 |  6103 | `case PH7_OP_FOREACH_INIT: {` |
|    11090 |  6104 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6105 | `	void *pName;` |
|        - |  6106 | `#ifdef UNTRUST` |
|        - |  6107 | `	if( pTos < pStack ){` |
|        - |  6108 | `		goto Abort;` |
|        - |  6109 | `	}` |
|        - |  6110 | `#endif` |
|    11090 |  6111 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6112 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6113 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6114 | `			/* Force a string cast */` |
|      ! 0 |  6115 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6116 | `		}` |
|        - |  6117 | `		/* Duplicate name */` |
|      ! 0 |  6118 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6119 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6120 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6121 | `		}` |
|      ! 0 |  6122 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6123 | `	}` |
|    11090 |  6124 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6125 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6126 | `			/* Force a string cast */` |
|      ! 0 |  6127 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6128 | `		}` |
|        - |  6129 | `		/* Duplicate name */` |
|      ! 0 |  6130 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6131 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6132 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6133 | `		}` |
|      ! 0 |  6134 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6135 | `	}` |
|        - |  6136 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11090 |  6137 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6138 | `		/* Jump out of the loop */` |
|      ! 0 |  6139 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6140 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6141 | `		}` |
|      ! 0 |  6142 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6143 | `	}else{` |
|        - |  6144 | `		ph7_foreach_step *pStep;` |
|    11090 |  6145 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11090 |  6146 | `		if( pStep == 0 ){` |
|      ! 0 |  6147 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6148 | `			/* Jump out of the loop */` |
|      ! 0 |  6149 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6150 | `		}else{` |
|        - |  6151 | `			/* Zero the structure */` |
|    11090 |  6152 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6153 | `			/* Prepare the step */` |
|    11090 |  6154 | `			pStep->iFlags = pInfo->iFlags;` |
|    11090 |  6155 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6156 | `				ph7_hashmap *pMap;` |
|        - |  6157 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6158 | `				 * source array so mutations don't affect other sharers. */` |
|    11058 |  6159 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6160 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6161 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6162 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6163 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6164 | `						 * variable still points at the same hashmap as` |
|        - |  6165 | `						 * the stack value. */` |
|        9 |  6166 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6167 | `							pCur->iRef--;` |
|        9 |  6168 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6169 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6170 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6171 | `						}` |
|        4 |  6172 | `					}` |
|        4 |  6173 | `				}` |
|    11058 |  6174 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6175 | `				/* Reset the internal loop cursor */` |
|    11058 |  6176 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6177 | `				/* Mark the step */` |
|    11058 |  6178 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11058 |  6179 | `				pStep->xIter.pMap = pMap;` |
|    11058 |  6180 | `				pMap->iRef++;` |
|     5530 |  6181 | `			}else{` |
|       34 |  6182 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6183 | `				ph7_class *pIteratorClass;` |
|        - |  6184 | `				/* Check if the object implements Iterator */` |
|       34 |  6185 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6186 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6187 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6188 | `					ph7_class_method *pRewind;` |
|       24 |  6189 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6190 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6191 | `					pThis->iRef++;` |
|       24 |  6192 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6193 | `					if( pRewind ){` |
|       24 |  6194 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6195 | `					}` |
|       13 |  6196 | `				}else{` |
|        - |  6197 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6198 | `					ph7_class *pIterAggClass;` |
|       12 |  6199 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6200 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6201 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6202 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6203 | `						ph7_class_method *pGetIter;` |
|        3 |  6204 | `						int iterAggOk = 0;` |
|        3 |  6205 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6206 | `						if( pGetIter ){` |
|        - |  6207 | `							ph7_value sResult;` |
|        3 |  6208 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6209 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6210 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6211 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6212 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6213 | `									ph7_class_method *pRewind;` |
|        3 |  6214 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6215 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6216 | `									pIterObj->iRef++;` |
|        - |  6217 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6218 | `									pStep->pOwner = pThis;` |
|        3 |  6219 | `									pThis->iRef++;` |
|        3 |  6220 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6221 | `									if( pRewind ){` |
|        3 |  6222 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6223 | `									}` |
|        3 |  6224 | `									iterAggOk = 1;` |
|        1 |  6225 | `								}` |
|        1 |  6226 | `							}` |
|        3 |  6227 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6228 | `						}` |
|        3 |  6229 | `						if( !iterAggOk ){` |
|        - |  6230 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6231 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6232 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6233 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6234 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6235 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6236 | `						}` |
|        2 |  6237 | `					}else{` |
|        - |  6238 | `						/* Plain object iteration via hAttr */` |
|        9 |  6239 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6240 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6241 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6242 | `						pThis->iRef++;` |
|        - |  6243 | `					}` |
|        - |  6244 | `				}` |
|        - |  6245 | `			}` |
|        - |  6246 | `		}` |
|    11090 |  6247 | `		if( pStep ){` |
|    11090 |  6248 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6249 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6250 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6251 | `				/* Jump out of the loop */` |
|      ! 0 |  6252 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6253 | `			}` |
|     5544 |  6254 | `		}` |
|        - |  6255 | `	}` |
|    11090 |  6256 | `	VmPopOperand(&pTos,1);` |
|    11090 |  6257 | `	break;` |
|        - |  6258 | `						  }` |
|        - |  6259 | `/*` |
|        - |  6260 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6261 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6262 | ` */` |
|    90423 |  6263 | `case PH7_OP_FOREACH_STEP: {` |
|   180848 |  6264 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6265 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6266 | `	ph7_value *pValue;` |
|        - |  6267 | `	VmFrame *pFrameLocal;` |
|        - |  6268 | `	/* Peek the last step */` |
|   180848 |  6269 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   180848 |  6270 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   180848 |  6271 | `	pFrameLocal = pVm->pFrame;` |
|   180848 |  6272 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   180848 |  6273 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   180720 |  6274 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6275 | `		ph7_hashmap_node *pNode;` |
|        - |  6276 | `		/* Extract the current node value */` |
|   180720 |  6277 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   180720 |  6278 | `		if( pNode == 0 ){` |
|        - |  6279 | `			/* No more entry to process */` |
|    11056 |  6280 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11056 |  6281 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6282 | `				/* Break the reference with the last element */` |
|        7 |  6283 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6284 | `			}` |
|        - |  6285 | `			/* Automatically reset the loop cursor */` |
|    11056 |  6286 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6287 | `			/* Cleanup the mess left behind */` |
|    11056 |  6288 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11056 |  6289 | `			SySetPop(&pInfo->aStep);` |
|    11056 |  6290 | `			PH7_HashmapUnref(pMap);` |
|     5529 |  6291 | `		}else{` |
|   169666 |  6292 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6293 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6294 | `				if( pKey ){` |
|      426 |  6295 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6296 | `				}` |
|      212 |  6297 | `			}` |
|   169666 |  6298 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6299 | `				SyHashEntry *pEntry;` |
|        - |  6300 | `				/* Pass by reference */` |
|       23 |  6301 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6302 | `				if( pEntry ){` |
|       21 |  6303 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6304 | `				}else{` |
|        4 |  6305 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6306 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6307 | `				}` |
|       12 |  6308 | `			}else{` |
|        - |  6309 | `				/* Make a copy of the entry value */` |
|   169644 |  6310 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   169644 |  6311 | `				if( pValue ){` |
|   169644 |  6312 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    84821 |  6313 | `				}` |
|        - |  6314 | `			}` |
|        2 |  6315 | `		}` |
|    90489 |  6316 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6317 | `		/* Iterator-based iteration.` |
|        - |  6318 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6319 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6320 | `		 */` |
|      106 |  6321 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6322 | `		ph7_class_method *pMethod;` |
|        - |  6323 | `		ph7_value sResult;` |
|      106 |  6324 | `		int isValid = 0;` |
|        - |  6325 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6326 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6327 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6328 | `		}else{` |
|       82 |  6329 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6330 | `			if( pMethod ){` |
|       82 |  6331 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6332 | `			}` |
|        - |  6333 | `		}` |
|        - |  6334 | `		/* Call valid() */` |
|      106 |  6335 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6336 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6337 | `		if( pMethod ){` |
|      106 |  6338 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6339 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6340 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6341 | `		}` |
|      106 |  6342 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6343 | `		if( !isValid ){` |
|        - |  6344 | `			/* Iterator exhausted */` |
|       24 |  6345 | `			pc = pInstr->iP2 - 1;` |
|        - |  6346 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6347 | `			if( pStep->pOwner ){` |
|        3 |  6348 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6349 | `			}` |
|       24 |  6350 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6351 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6352 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6353 | `		}else{` |
|        - |  6354 | `			/* Call current() to get value */` |
|       84 |  6355 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6356 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6357 | `			if( pMethod ){` |
|       84 |  6358 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6359 | `			}` |
|       84 |  6360 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6361 | `			if( pValue ){` |
|       84 |  6362 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6363 | `			}` |
|       84 |  6364 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6365 | `			/* Call key() if needed */` |
|       84 |  6366 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6367 | `				ph7_value sKey;` |
|       35 |  6368 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6369 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6370 | `				if( pMethod ){` |
|       35 |  6371 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6372 | `				}` |
|       35 |  6373 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6374 | `				if( pValue ){` |
|       35 |  6375 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6376 | `				}` |
|       35 |  6377 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6378 | `			}` |
|        - |  6379 | `		}` |
|       54 |  6380 | `	}else{` |
|       25 |  6381 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6382 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6383 | `		SyHashEntry *pEntry;` |
|        - |  6384 | `		/* Point to the next attribute */` |
|       29 |  6385 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6386 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6387 | `			/* Check access permission */` |
|       31 |  6388 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6389 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6390 | `					break; /* Access is granted */` |
|        - |  6391 | `			}` |
|        1 |  6392 | `		}` |
|       25 |  6393 | `		if( pEntry == 0 ){` |
|        - |  6394 | `			/* Clean up the mess left behind */` |
|        9 |  6395 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6396 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6397 | `				/* Break the reference with the last element */` |
|        3 |  6398 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6399 | `			}` |
|        9 |  6400 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6401 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6402 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6403 | `		}else{` |
|       17 |  6404 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6405 | `			ph7_value *pAttrValue;` |
|       17 |  6406 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6407 | `				/* Fill with the current attribute name */` |
|       17 |  6408 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6409 | `				if( pKey ){` |
|       17 |  6410 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6411 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6412 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6413 | `				}` |
|        8 |  6414 | `			}` |
|        - |  6415 | `			/* Extract attribute value */` |
|       17 |  6416 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6417 | `			if( pAttrValue ){` |
|       17 |  6418 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6419 | `					/* Pass by reference */` |
|        3 |  6420 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6421 | `					if( pEntry ){` |
|        3 |  6422 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6423 | `					}else{` |
|      ! 0 |  6424 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6425 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6426 | `					}` |
|        2 |  6427 | `				}else{` |
|        - |  6428 | `					/* Make a copy of the attribute value */` |
|       15 |  6429 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6430 | `					if( pValue ){` |
|       15 |  6431 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6432 | `					}` |
|        - |  6433 | `				}` |
|        8 |  6434 | `			}` |
|        - |  6435 | `		}` |
|        - |  6436 | `	}` |
|   180848 |  6437 | `	break;` |
|        - |  6438 | `						  }` |
|        - |  6439 | `/*` |
|        - |  6440 | ` * OP_MEMBER P1 P2` |
|        - |  6441 | ` * Load class attribute/method on the stack.` |
|        - |  6442 | ` */` |
|     2819 |  6443 | `case PH7_OP_MEMBER: {` |
|        - |  6444 | `	ph7_class_instance *pThis;` |
|        - |  6445 | `	ph7_value *pNos;` |
|        - |  6446 | `	SyString sName;` |
|     5640 |  6447 | `	if( !pInstr->iP1 ){` |
|     5420 |  6448 | `		pNos = &pTos[-1];` |
|        - |  6449 | `#ifdef UNTRUST` |
|        - |  6450 | `		if( pNos < pStack ){` |
|        - |  6451 | `			goto Abort;` |
|        - |  6452 | `		}` |
|        - |  6453 | `#endif` |
|     5420 |  6454 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6455 | `			ph7_class *pClass;` |
|        - |  6456 | `			/* Class already instantiated */` |
|     5420 |  6457 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6458 | `			/* Point to the instantiated class */` |
|     5420 |  6459 | `			pClass = pThis->pClass;` |
|        - |  6460 | `			/* Extract attribute name first */` |
|     5420 |  6461 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     5420 |  6462 | `			if( pInstr->iP2 ){` |
|        - |  6463 | `				/* Method call */` |
|      566 |  6464 | `				ph7_class_method *pMeth = 0;` |
|      566 |  6465 | `				if( sName.nByte > 0 ){` |
|        - |  6466 | `					/* Extract the target method */` |
|      566 |  6467 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      282 |  6468 | `				}` |
|      566 |  6469 | `				if( pMeth == 0 ){` |
|      ! 0 |  6470 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6471 | `						&pClass->sName,&sName` |
|        - |  6472 | `						);` |
|        - |  6473 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6474 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6475 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6476 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6477 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6478 | `				}else{` |
|        - |  6479 | `					/* Push method name on the stack */` |
|      566 |  6480 | `					PH7_MemObjRelease(pTos);` |
|      566 |  6481 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      566 |  6482 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6483 | `				}` |
|      566 |  6484 | `				pTos->nIdx = SXU32_HIGH;` |
|      284 |  6485 | `			}else{` |
|        - |  6486 | `				/* Attribute access */` |
|     4856 |  6487 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6488 | `				SyHashEntry *pEntry;` |
|        - |  6489 | `				/* Extract the target attribute */` |
|     4856 |  6490 | `				if( sName.nByte > 0 ){` |
|     4856 |  6491 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4856 |  6492 | `					if( pEntry ){` |
|        - |  6493 | `						/* Point to the attribute value */` |
|     4854 |  6494 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2426 |  6495 | `					}` |
|     2427 |  6496 | `				}` |
|     4856 |  6497 | `				if( pObjAttr == 0 ){` |
|        - |  6498 | `					/* No such attribute,load null */` |
|        4 |  6499 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6500 | `						&pClass->sName,&sName);` |
|        - |  6501 | `					/* Call the __get magic method if available */` |
|        3 |  6502 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6503 | `				}` |
|     4856 |  6504 | `				VmPopOperand(&pTos,1);` |
|        - |  6505 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6506 | `				 * This is due to the following case:` |
|        - |  6507 | `				 *     (new TestClass())->foo;` |
|        - |  6508 | `				 */` |
|     4856 |  6509 | `				pThis->iRef++;` |
|     4856 |  6510 | `				PH7_MemObjRelease(pTos);` |
|     4856 |  6511 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4856 |  6512 | `				if( pObjAttr ){` |
|     4854 |  6513 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6514 | `					/* Check attribute access */` |
|     4854 |  6515 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6516 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6517 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6518 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6519 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6520 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     4852 |  6521 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2445 |  6522 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       36 |  6523 | `							VmInstr *pNext = pInstr + 1;` |
|       36 |  6524 | `							int bIsLhs = 0;` |
|       36 |  6525 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       34 |  6526 | `								bIsLhs = 1;` |
|       16 |  6527 | `							}` |
|       36 |  6528 | `							if( !bIsLhs ){` |
|        3 |  6529 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6530 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6531 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6532 | `									goto Abort;` |
|        - |  6533 | `								}` |
|        - |  6534 | `								{` |
|        3 |  6535 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6536 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6537 | `										pc = pFrm2->iExceptionJump - 1;` |
|     2819 |  6538 | `										break;` |
|        - |  6539 | `									}` |
|        - |  6540 | `								}` |
|      ! 0 |  6541 | `								goto Exception;` |
|        - |  6542 | `							}` |
|       16 |  6543 | `						}` |
|        - |  6544 | `						/* Load attribute */` |
|     4852 |  6545 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4852 |  6546 | `						if( pValue ){` |
|     4852 |  6547 | `							if( pThis->iRef < 2 ){` |
|        - |  6548 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6549 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6550 | `								 */` |
|        7 |  6551 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  6552 | `							}else{` |
|        - |  6553 | `								/* Simple load */` |
|     4846 |  6554 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6555 | `							}` |
|     4852 |  6556 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4850 |  6557 | `								if( pThis->iRef > 1 ){` |
|        - |  6558 | `									/* Load attribute index */` |
|     4844 |  6559 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2421 |  6560 | `								}` |
|     2424 |  6561 | `							}` |
|     2425 |  6562 | `						}` |
|     2427 |  6563 | `					}else{` |
|        - |  6564 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6565 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6566 | `						char zMsg[256];` |
|      ! 0 |  6567 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6568 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6569 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6570 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6571 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6572 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6573 | `						goto Abort;` |
|        - |  6574 | `					}` |
|     2425 |  6575 | `				}` |
|        - |  6576 | `				/* Safely unreference the object */` |
|     4854 |  6577 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6578 | `			}` |
|     2710 |  6579 | `		}else{` |
|      ! 0 |  6580 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  6581 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6582 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6583 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6584 | `		}` |
|     2710 |  6585 | `	}else{` |
|        - |  6586 | `		/* Static member access using class name */` |
|      222 |  6587 | `		pNos = pTos;` |
|      222 |  6588 | `		pThis = 0;` |
|      222 |  6589 | `		if( !pInstr->p3 ){` |
|      188 |  6590 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      188 |  6591 | `			pNos--;` |
|        - |  6592 | `#ifdef UNTRUST` |
|        - |  6593 | `			if( pNos < pStack ){` |
|        - |  6594 | `				goto Abort;` |
|        - |  6595 | `			}` |
|        - |  6596 | `#endif` |
|       95 |  6597 | `		}else{` |
|        - |  6598 | `			/* Attribute name already computed */` |
|       36 |  6599 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6600 | `		}` |
|      222 |  6601 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      222 |  6602 | `			ph7_class *pClass = 0;` |
|      222 |  6603 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6604 | `				/* Class already instantiated */` |
|        5 |  6605 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  6606 | `				pClass = pThis->pClass;` |
|        5 |  6607 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  6608 | `			}else{` |
|        - |  6609 | `				/* Try to extract the target class */` |
|      218 |  6610 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      218 |  6611 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      218 |  6612 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  6613 | `					/* Handle self/static/parent keywords */` |
|      218 |  6614 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  6615 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  6616 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  6617 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  6618 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  6619 | `						}` |
|      188 |  6620 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  6621 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      157 |  6622 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  6623 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  6624 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  6625 | `							pClass = pSelf->pBase;` |
|       12 |  6626 | `						}` |
|       14 |  6627 | `					}else{` |
|      108 |  6628 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  6629 | `					}` |
|      108 |  6630 | `				}` |
|        - |  6631 | `			}` |
|      222 |  6632 | `			if( pClass == 0 ){` |
|        - |  6633 | `				/* Undefined class */` |
|      ! 0 |  6634 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  6635 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  6636 | `					);` |
|      ! 0 |  6637 | `				if( !pInstr->p3 ){` |
|      ! 0 |  6638 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6639 | `				}` |
|      ! 0 |  6640 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6641 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  6642 | `			}else{` |
|      222 |  6643 | `				if( pInstr->iP2 ){` |
|        - |  6644 | `					/* Method call */` |
|       84 |  6645 | `					ph7_class_method *pMeth = 0;` |
|       84 |  6646 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  6647 | `						/* Extract the target method */` |
|       84 |  6648 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       41 |  6649 | `					}` |
|       84 |  6650 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  6651 | `						if( pMeth ){` |
|      ! 0 |  6652 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  6653 | `								&pClass->sName,&sName` |
|        - |  6654 | `								);` |
|      ! 0 |  6655 | `						}else{` |
|      ! 0 |  6656 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6657 | `								&pClass->sName,&sName` |
|        - |  6658 | `								);` |
|        - |  6659 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6660 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6661 | `						}` |
|        - |  6662 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6663 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6664 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6665 | `						}` |
|      ! 0 |  6666 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6667 | `					}else{` |
|        - |  6668 | `						/* Push method name on the stack */` |
|       84 |  6669 | `						PH7_MemObjRelease(pTos);` |
|       84 |  6670 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       84 |  6671 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6672 | `					}` |
|       84 |  6673 | `					pTos->nIdx = SXU32_HIGH;` |
|       43 |  6674 | `				}else{` |
|        - |  6675 | `					/* Attribute access */` |
|      140 |  6676 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6677 | `					/* Check for special ::class pseudo-constant */` |
|      186 |  6678 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  6679 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6680 | `						/* ::class returns the fully qualified class name */` |
|        - |  6681 | `						/* Pop the attribute name from the stack */` |
|       60 |  6682 | `						if( !pInstr->p3 ){` |
|       60 |  6683 | `							VmPopOperand(&pTos,1);` |
|       29 |  6684 | `						}` |
|       60 |  6685 | `						PH7_MemObjRelease(pTos);` |
|        - |  6686 | `						/* Load the class name */` |
|       60 |  6687 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6688 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6689 | `					}else{` |
|        - |  6690 | `						/* Extract the target attribute */` |
|       82 |  6691 | `						if( sName.nByte > 0 ){` |
|       82 |  6692 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       40 |  6693 | `						}` |
|       82 |  6694 | `						if( pAttr == 0 ){` |
|        - |  6695 | `							/* No such attribute,load null */` |
|      ! 0 |  6696 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6697 | `								&pClass->sName,&sName);` |
|        - |  6698 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6699 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6700 | `						}` |
|        - |  6701 | `						/* Pop the attribute name from the stack */` |
|       82 |  6702 | `						if( !pInstr->p3 ){` |
|       48 |  6703 | `							VmPopOperand(&pTos,1);` |
|       23 |  6704 | `						}` |
|       82 |  6705 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6706 | `						pTos->nIdx = SXU32_HIGH;` |
|       82 |  6707 | `						if( pAttr ){` |
|       82 |  6708 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6709 | `								/* Access to a non static attribute */` |
|      ! 0 |  6710 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6711 | `									&pClass->sName,&pAttr->sName` |
|        - |  6712 | `									);` |
|      ! 0 |  6713 | `							}else{` |
|        - |  6714 | `								ph7_value *pValue;` |
|        - |  6715 | `								/* Check if the access to the attribute is allowed */` |
|       82 |  6716 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6717 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  6718 | `									 * Same LHS-of-store peek as the instance path. */` |
|       76 |  6719 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       51 |  6720 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       35 |  6721 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       22 |  6722 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 |  6723 | `										if( pS ){` |
|       24 |  6724 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       24 |  6725 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        5 |  6726 | `												VmInstr *pNext = pInstr + 1;` |
|        5 |  6727 | `												int bIsLhs = 0;` |
|        5 |  6728 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        3 |  6729 | `													bIsLhs = 1;` |
|        1 |  6730 | `												}` |
|        5 |  6731 | `												if( !bIsLhs ){` |
|        3 |  6732 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  6733 | `													if( pThis ){` |
|      ! 0 |  6734 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6735 | `													}` |
|        3 |  6736 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  6737 | `														goto Abort;` |
|        - |  6738 | `													}` |
|        - |  6739 | `													{` |
|        3 |  6740 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6741 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6742 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  6743 | `															break;` |
|        - |  6744 | `														}` |
|        - |  6745 | `													}` |
|      ! 0 |  6746 | `													goto Exception;` |
|        - |  6747 | `												}` |
|        1 |  6748 | `											}` |
|       10 |  6749 | `										}` |
|       10 |  6750 | `									}` |
|        - |  6751 | `									/* Load the desired attribute */` |
|       76 |  6752 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       76 |  6753 | `									if( pValue ){` |
|       76 |  6754 | `										PH7_MemObjLoad(pValue,pTos);` |
|       76 |  6755 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6756 | `											/* Load index number */` |
|       34 |  6757 | `											pTos->nIdx = pAttr->nIdx;` |
|       16 |  6758 | `										}` |
|       37 |  6759 | `									}` |
|       39 |  6760 | `								}else{` |
|        - |  6761 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6762 | `									char zMsg[256];` |
|        5 |  6763 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6764 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6765 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6766 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6767 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6768 | `									}else{` |
|      ! 0 |  6769 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6770 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6771 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6772 | `									}` |
|        5 |  6773 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6774 | `									goto Abort;` |
|        - |  6775 | `								}` |
|        - |  6776 | `							}` |
|       37 |  6777 | `						}` |
|        - |  6778 | `					}` |
|        - |  6779 | `				}` |
|      216 |  6780 | `				if( pThis ){` |
|        - |  6781 | `					/* Safely unreference the object */` |
|        5 |  6782 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6783 | `				}` |
|        - |  6784 | `			}` |
|      109 |  6785 | `		}else{` |
|        - |  6786 | `			/* Pop operands */` |
|      ! 0 |  6787 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6788 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6789 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6790 | `			}` |
|      ! 0 |  6791 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6792 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6793 | `		}` |
|        - |  6794 | `	}` |
|     5632 |  6795 | `	break;` |
|        - |  6796 | `					}` |
|        - |  6797 | `/*` |
|        - |  6798 | ` * OP_NEW P1 * * *` |
|        - |  6799 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6800 | ` */` |
|      427 |  6801 | `case PH7_OP_NEW: {` |
|      856 |  6802 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      856 |  6803 | `	ph7_class *pClass = 0;` |
|        - |  6804 | `	ph7_class_instance *pNew;` |
|      856 |  6805 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6806 | `		/* Try to extract the desired class */` |
|     1283 |  6807 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      854 |  6808 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      427 |  6809 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6810 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6811 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6812 | `	}` |
|      856 |  6813 | `	if( pClass == 0 ){` |
|        - |  6814 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6815 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6816 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6817 | `			);` |
|        - |  6818 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6819 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6820 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6821 | `			/* Pop given arguments */` |
|      ! 0 |  6822 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6823 | `		}` |
|      ! 0 |  6824 | `		goto Abort;` |
|      ! 0 |  6825 | `	}else{` |
|        - |  6826 | `		ph7_class_method *pCons;` |
|        - |  6827 | `		/* Create a new class instance */` |
|      856 |  6828 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      856 |  6829 | `		if( pNew == 0 ){` |
|      ! 0 |  6830 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6831 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6832 | `				&pClass->sName` |
|        - |  6833 | `			);` |
|      ! 0 |  6834 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6835 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6836 | `				/* Pop given arguments */` |
|      ! 0 |  6837 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6838 | `			}` |
|      ! 0 |  6839 | `			break;` |
|        - |  6840 | `		}` |
|        - |  6841 | `		/* Check if a constructor is available */` |
|      856 |  6842 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      856 |  6843 | `		if( pCons == 0 ){` |
|      696 |  6844 | `			SyString *pName = &pClass->sName;` |
|        - |  6845 | `			/* Check for a constructor with the same base class name */` |
|      696 |  6846 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      347 |  6847 | `		}` |
|      856 |  6848 | `		if( pCons ){` |
|        - |  6849 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  6850 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  6851 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  6852 | `			 * (including variadic string-key packing). */` |
|      162 |  6853 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      162 |  6854 | `			SySetReset(&aArg);` |
|      330 |  6855 | `			while( pArg < pTos ){` |
|      170 |  6856 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      170 |  6857 | `				pArg++;` |
|        2 |  6858 | `			}` |
|      162 |  6859 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  6860 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6861 | `				sxu32 n;` |
|       57 |  6862 | `				n = SySetUsed(&aArg);` |
|        - |  6863 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  6864 | `				 * for named args the missing-arg check happens downstream` |
|        - |  6865 | `				 * after resolution). */` |
|      101 |  6866 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6867 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6868 | `					if( pFuncArg ){` |
|       45 |  6869 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6870 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6871 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6872 | `						}` |
|       22 |  6873 | `					}` |
|       45 |  6874 | `					n++;` |
|        1 |  6875 | `				}` |
|       28 |  6876 | `			}` |
|      162 |  6877 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  6878 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      162 |  6879 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6880 | `				pNew->iRef = 1;` |
|      ! 0 |  6881 | `			}` |
|       80 |  6882 | `		}` |
|      856 |  6883 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6884 | `			/* Pop given arguments */` |
|      144 |  6885 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       71 |  6886 | `		}` |
|      856 |  6887 | `		PH7_MemObjRelease(pTos);` |
|      856 |  6888 | `		pTos->x.pOther = pNew;` |
|      856 |  6889 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6890 | `	}` |
|      856 |  6891 | `	break;` |
|        - |  6892 | `				 }` |
|        - |  6893 | `/*` |
|        - |  6894 | ` * OP_CLONE * * *` |
|        - |  6895 | ` * Perfome a clone operation.` |
|        - |  6896 | ` */` |
|       23 |  6897 | `case PH7_OP_CLONE: {` |
|        - |  6898 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6899 | `#ifdef UNTRUST` |
|        - |  6900 | `	if( pTos < pStack ){` |
|        - |  6901 | `		goto Abort;` |
|        - |  6902 | `	}` |
|        - |  6903 | `#endif` |
|        - |  6904 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6905 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6906 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6907 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6908 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6909 | `		break;` |
|        - |  6910 | `	}` |
|        - |  6911 | `	/* Point to the source */` |
|       44 |  6912 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6913 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6914 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6915 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6916 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6917 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6918 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6919 | `		break;` |
|        - |  6920 | `	}` |
|        - |  6921 | `	/* Perform the clone operation */` |
|       44 |  6922 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6923 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6924 | `	if( pClone == 0 ){` |
|      ! 0 |  6925 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6926 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6927 | `	}else{` |
|        - |  6928 | `		/* Load the cloned object */` |
|       44 |  6929 | `		pTos->x.pOther = pClone;` |
|       44 |  6930 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6931 | `	}` |
|       44 |  6932 | `	break;` |
|        - |  6933 | `				   }` |
|        - |  6934 | `/*` |
|        - |  6935 | ` * OP_SWITCH * * P3` |
|        - |  6936 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6937 | ` */` |
|       26 |  6938 | `case PH7_OP_SWITCH: {` |
|       54 |  6939 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6940 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6941 | `	ph7_value sValue,sCaseValue;` |
|        - |  6942 | `	sxu32 n,nEntry;` |
|        - |  6943 | `#ifdef UNTRUST` |
|        - |  6944 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6945 | `		goto Abort;` |
|        - |  6946 | `	}` |
|        - |  6947 | `#endif` |
|        - |  6948 | `	/* Point to the case table  */` |
|       54 |  6949 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6950 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6951 | `	/* Select the appropriate case block to execute */` |
|       54 |  6952 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6953 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6954 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6955 | `		pCase = &aCase[n];` |
|      130 |  6956 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6957 | `		/* Execute the case expression first */` |
|      130 |  6958 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6959 | `		/* Compare the two expression */` |
|      130 |  6960 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6961 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6962 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6963 | `		if( rc == 0 ){` |
|        - |  6964 | `			/* Value match,jump to this block */` |
|       52 |  6965 | `			pc = pCase->nStart - 1;` |
|       52 |  6966 | `			break;` |
|        - |  6967 | `		}` |
|       41 |  6968 | `	}` |
|       54 |  6969 | `	VmPopOperand(&pTos,1);` |
|       54 |  6970 | `	if( n >= nEntry ){` |
|        - |  6971 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6972 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6973 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6974 | `		}else{` |
|        - |  6975 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6976 | `			pc = pSwitch->nOut - 1;` |
|        - |  6977 | `		}` |
|        1 |  6978 | `	}` |
|       54 |  6979 | `	break;` |
|        - |  6980 | `					}` |
|        - |  6981 | `/*` |
|        - |  6982 | ` * OP_MATCH * * P3` |
|        - |  6983 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  6984 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  6985 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  6986 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  6987 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  6988 | ` */` |
|       52 |  6989 | `case PH7_OP_MATCH: {` |
|      106 |  6990 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      106 |  6991 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  6992 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  6993 | `	sxu32 i,j,nArm,nCond;` |
|      106 |  6994 | `	int matched = 0;` |
|        - |  6995 | `#ifdef UNTRUST` |
|        - |  6996 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  6997 | `		goto Abort;` |
|        - |  6998 | `	}` |
|        - |  6999 | `#endif` |
|      106 |  7000 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      106 |  7001 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      106 |  7002 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      106 |  7003 | `	PH7_MemObjInit(pVm,&sCond);` |
|      106 |  7004 | `	PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7005 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      338 |  7006 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      234 |  7007 | `		pArm = &aArm[i];` |
|      234 |  7008 | `		if( pArm->bDefault ){` |
|       11 |  7009 | `			pDefault = pArm;` |
|       11 |  7010 | `			continue;` |
|        - |  7011 | `		}` |
|      224 |  7012 | `		nCond = SySetUsed(&pArm->aConds);` |
|      388 |  7013 | `		for( j = 0; j < nCond; ++j ){` |
|      256 |  7014 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      256 |  7015 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7016 | `				continue;` |
|        - |  7017 | `			}` |
|      256 |  7018 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      256 |  7019 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      256 |  7020 | `			PH7_MemObjRelease(&sCond);` |
|      256 |  7021 | `			if( rc == 0 ){` |
|       91 |  7022 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       91 |  7023 | `				matched = 1;` |
|       91 |  7024 | `				break;` |
|        - |  7025 | `			}` |
|       84 |  7026 | `		}` |
|      113 |  7027 | `	}` |
|      106 |  7028 | `	if( !matched && pDefault ){` |
|       11 |  7029 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       11 |  7030 | `		matched = 1;` |
|        5 |  7031 | `	}` |
|      106 |  7032 | `	if( !matched ){` |
|        5 |  7033 | `		const char *zType = "unknown";` |
|        - |  7034 | `		char zMsg[128];` |
|        - |  7035 | `		sxu32 nMsg;` |
|        5 |  7036 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7037 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7038 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7039 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7040 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7041 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7042 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7043 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7044 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7045 | `		default: break;` |
|        - |  7046 | `		}` |
|        7 |  7047 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7048 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7049 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7050 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7051 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7052 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7053 | `		goto Abort;` |
|        - |  7054 | `	}` |
|      101 |  7055 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7056 | `	/* Replace subject on TOS with the arm result */` |
|      101 |  7057 | `	PH7_MemObjStore(&sResult,pTos);` |
|      101 |  7058 | `	PH7_MemObjRelease(&sResult);` |
|      101 |  7059 | `	break;` |
|        - |  7060 | `					}` |
|        - |  7061 | `/*` |
|        - |  7062 | ` * OP_YIELD P1 P2 *` |
|        - |  7063 | ` *  Yield a value from a generator function.` |
|        - |  7064 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7065 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7066 | ` */` |
|       34 |  7067 | `case PH7_OP_YIELD: {` |
|        - |  7068 | `	ph7_generator *pGen;` |
|       70 |  7069 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7070 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7071 | `		goto Abort;` |
|        - |  7072 | `	}` |
|       70 |  7073 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7074 | `	if( pInstr->iP2 ){` |
|        - |  7075 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7076 | `#ifdef UNTRUST` |
|        - |  7077 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7078 | `#endif` |
|        7 |  7079 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7080 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7081 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7082 | `		VmPopOperand(&pTos, 1);` |
|        - |  7083 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7084 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7085 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7086 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7087 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7088 | `			}` |
|        1 |  7089 | `		}` |
|       67 |  7090 | `	}else if( pInstr->iP1 ){` |
|        - |  7091 | `		/* yield $value */` |
|        - |  7092 | `#ifdef UNTRUST` |
|        - |  7093 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7094 | `#endif` |
|       64 |  7095 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7096 | `		VmPopOperand(&pTos, 1);` |
|        - |  7097 | `		/* Auto-increment key */` |
|       64 |  7098 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7099 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7100 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7101 | `	}else{` |
|        - |  7102 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7103 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7104 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7105 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7106 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7107 | `	}` |
|        - |  7108 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7109 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7110 | `	goto Suspend;` |
|        - |  7111 |  |
|        - |  7112 | `/*` |
|        - |  7113 | ` * OP_CALL P1 * *` |
|        - |  7114 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7115 | ` *  function on the stack.` |
|        - |  7116 | ` */` |
|   322189 |  7117 | `case PH7_OP_CALL: {` |
|   644424 |  7118 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7119 | `	ph7_value *pArg;` |
|   644424 |  7120 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   644424 |  7121 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7122 | `	SyHashEntry *pEntry;` |
|        - |  7123 | `	SyString sName;` |
|        - |  7124 | `	/* Extract function name */` |
|   644424 |  7125 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  7126 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7127 | `			ph7_value sResult;` |
|      ! 0 |  7128 | `			SySetReset(&aArg);` |
|      ! 0 |  7129 | `			while( pArg < pTos ){` |
|      ! 0 |  7130 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7131 | `				pArg++;` |
|      ! 0 |  7132 | `			}` |
|      ! 0 |  7133 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7134 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7135 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7136 | `			SySetReset(&aArg);` |
|        - |  7137 | `			/* Pop given arguments */` |
|      ! 0 |  7138 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7139 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7140 | `			}` |
|        - |  7141 | `			/* Copy result */` |
|      ! 0 |  7142 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7143 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7144 | `		}else{` |
|        3 |  7145 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  7146 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7147 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  7148 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  7149 | `			}else{` |
|        - |  7150 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  7151 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7152 | `			}` |
|        - |  7153 | `			/* Pop given arguments */` |
|        3 |  7154 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7155 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7156 | `			}` |
|        - |  7157 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7158 | `			PH7_MemObjRelease(pTos);` |
|        - |  7159 | `		}` |
|   321908 |  7160 | `		break;` |
|        - |  7161 | `	}` |
|   644422 |  7162 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7163 | `	/* Check for a compiled function first.` |
|        - |  7164 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7165 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   644422 |  7166 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7167 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7168 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7169 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7170 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7171 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7172 | `	{` |
|   644422 |  7173 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   644422 |  7174 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7175 | `		const char *zFunc;` |
|        - |  7176 | `		const char *zEnd;` |
|        - |  7177 | `		const char *z;` |
|        - |  7178 | `		SyString sGlobal;` |
|       20 |  7179 | `		zFunc = sName.zString;` |
|       20 |  7180 | `		zEnd  = zFunc + sName.nByte;` |
|       20 |  7181 | `		z = zEnd;` |
|        - |  7182 | `		/* Find last namespace separator */` |
|      174 |  7183 | `		while( z > zFunc ){` |
|      174 |  7184 | `			if( z[-1] == '\\' ){` |
|       20 |  7185 | `				break;` |
|        - |  7186 | `			}` |
|      156 |  7187 | `			z--;` |
|        2 |  7188 | `		}` |
|       20 |  7189 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7190 | `			/* Retry lookup using the unqualified/global function name */` |
|       20 |  7191 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       20 |  7192 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        9 |  7193 | `		}` |
|        9 |  7194 | `	}` |
|        - |  7195 | `	} /* end VmCallArgMap namespace scope */` |
|   644422 |  7196 | `	if( pEntry ){` |
|        - |  7197 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7198 | `		ph7_class_instance *pThis;` |
|        - |  7199 | `		ph7_value *pFrameStack;` |
|        - |  7200 | `		ph7_vm_func *pVmFunc;` |
|        - |  7201 | `		ph7_class *pSelf;` |
|        - |  7202 | `		VmFrame *pFrame;` |
|        - |  7203 | `		ph7_value *pObj;` |
|        - |  7204 | `		VmSlot sArg;` |
|        - |  7205 | `		sxu32 n;` |
|        - |  7206 | `		/* initialize fields */` |
|    15222 |  7207 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    15222 |  7208 | `		pThis = 0;` |
|    15222 |  7209 | `		pSelf = 0;` |
|    15222 |  7210 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7211 | `			ph7_class_method *pMeth;` |
|        - |  7212 | `			/* Class method call */` |
|     2316 |  7213 | `			ph7_value *pTarget = &pTos[-1];` |
|     2316 |  7214 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7215 | `				/* Extract the 'this' pointer */` |
|     2316 |  7216 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7217 | `					/* Instance already loaded */` |
|     2228 |  7218 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2228 |  7219 | `					pThis->iRef++;` |
|     2228 |  7220 | `					pSelf = pThis->pClass;` |
|     1113 |  7221 | `				}` |
|     2316 |  7222 | `				if( pSelf == 0 ){` |
|       90 |  7223 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7224 | `						/* "Late Static Binding" class name */` |
|      125 |  7225 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       41 |  7226 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       41 |  7227 | `					}` |
|       90 |  7228 | `					if( pSelf == 0 ){` |
|       19 |  7229 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  7230 | `					}` |
|       44 |  7231 | `				}` |
|     2316 |  7232 | `				if( pThis == 0  ){` |
|       90 |  7233 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       90 |  7234 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       90 |  7235 | `					if( pFrameLocal->pParent ){` |
|        - |  7236 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  7237 | `						pThis = pFrameLocal->pThis;` |
|       64 |  7238 | `						if( pThis ){` |
|       19 |  7239 | `							pThis->iRef++;` |
|        9 |  7240 | `						}` |
|       31 |  7241 | `					}` |
|       44 |  7242 | `				}` |
|     2316 |  7243 | `				VmPopOperand(&pTos,1);` |
|     2316 |  7244 | `				PH7_MemObjRelease(pTos);` |
|        - |  7245 | `				/* Synchronize pointers */` |
|     2316 |  7246 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7247 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7248 | `				 * user have already computed the random generated unique class method name` |
|        - |  7249 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7250 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7251 | `				 */` |
|     2316 |  7252 | `				while( pArg < pStack ){` |
|      ! 0 |  7253 | `					pArg++;` |
|      ! 0 |  7254 | `				}` |
|     2316 |  7255 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7256 | `					/* Check if the call is allowed */` |
|     2316 |  7257 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2316 |  7258 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7259 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7260 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7261 | `							char zMsg[256];` |
|      ! 0 |  7262 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7263 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7264 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7265 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7266 | `							/* Pop given arguments */` |
|      ! 0 |  7267 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7268 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7269 | `							}` |
|      ! 0 |  7270 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7271 | `							goto Abort;` |
|        - |  7272 | `						}` |
|        6 |  7273 | `					}` |
|     1157 |  7274 | `				}` |
|     1157 |  7275 | `			}` |
|     1157 |  7276 | `		}` |
|        - |  7277 | `		/* Check The recursion limit */` |
|    15222 |  7278 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7279 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7280 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7281 | `				&pVmFunc->sName);` |
|        - |  7282 | `			/* Pop given arguments */` |
|        3 |  7283 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7284 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7285 | `			}` |
|        - |  7286 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7287 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7288 | `			break;` |
|        - |  7289 | `		}` |
|    15220 |  7290 | `		if( pVmFunc->pNextName ){` |
|        - |  7291 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7292 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7293 | `		}` |
|    15220 |  7294 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7295 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7296 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7297 | `			ph7_generator *pGenerator;` |
|        - |  7298 | `			ph7_class_instance *pGenObj;` |
|        - |  7299 | `			ph7_value *pCtxAttr;` |
|        - |  7300 | `			SyString sAttrName;` |
|        - |  7301 | `			ph7_value **apCallArgs;` |
|        - |  7302 | `			int nGenArgs, iArg;` |
|        - |  7303 | `			/* Collect arguments from the operand stack */` |
|       24 |  7304 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7305 | `			apCallArgs = 0;` |
|       24 |  7306 | `			if( nGenArgs > 0 ){` |
|       14 |  7307 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7308 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7309 | `				if( apCallArgs == 0 ){` |
|        - |  7310 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7311 | `					nGenArgs = 0;` |
|      ! 0 |  7312 | `				}else{` |
|       10 |  7313 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7314 | `					int didReorder = 0;` |
|       10 |  7315 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7316 | `						/* Named-argument reordering for generator */` |
|        5 |  7317 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7318 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7319 | `						sxu32 nNV = nF;` |
|        5 |  7320 | `						sxi32 iVIdx = -1;` |
|        - |  7321 | `						sxi32 *aGSlot;` |
|        - |  7322 | `						sxu8 *aGUsed;` |
|        - |  7323 | `						sxu32 gi;` |
|       13 |  7324 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7325 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7326 | `						}` |
|        7 |  7327 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7328 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7329 | `						if( aGSlot ){` |
|        5 |  7330 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7331 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7332 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7333 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7334 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7335 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7336 | `								goto Abort;` |
|        - |  7337 | `							}` |
|        - |  7338 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7339 | `							 * append overflow (variadic / positional beyond` |
|        - |  7340 | `							 * formals) so downstream sees every argument. */` |
|        - |  7341 | `							{` |
|        5 |  7342 | `								int nOut = 0;` |
|       13 |  7343 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7344 | `									sxu32 gj;` |
|       13 |  7345 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7346 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7347 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7348 | `											break;` |
|        - |  7349 | `										}` |
|        3 |  7350 | `									}` |
|        5 |  7351 | `								}` |
|       13 |  7352 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7353 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7354 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7355 | `									}` |
|        5 |  7356 | `								}` |
|        5 |  7357 | `								nGenArgs = nOut;` |
|        - |  7358 | `							}` |
|        5 |  7359 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7360 | `							didReorder = 1;` |
|        2 |  7361 | `						}` |
|        - |  7362 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7363 | `						 * positional fill below — preserves arg order rather` |
|        - |  7364 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7365 | `					}` |
|       10 |  7366 | `					if( !didReorder ){` |
|       12 |  7367 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7368 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7369 | `						}` |
|        2 |  7370 | `					}` |
|        - |  7371 | `				}` |
|        4 |  7372 | `			}` |
|        - |  7373 | `			/* Create execution context and generator wrapper */` |
|       24 |  7374 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7375 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7376 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7377 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7378 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7379 | `				break;` |
|        - |  7380 | `			}` |
|       24 |  7381 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  7382 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7383 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7384 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7385 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7386 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7387 | `				break;` |
|        - |  7388 | `			}` |
|        - |  7389 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  7390 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  7391 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  7392 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  7393 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  7394 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  7395 | `			if( apCallArgs ){` |
|       10 |  7396 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  7397 | `			}` |
|       24 |  7398 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7399 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7400 | `				if( pThis ){` |
|      ! 0 |  7401 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7402 | `				}` |
|      ! 0 |  7403 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7404 | `					goto Abort;` |
|        - |  7405 | `				}` |
|      ! 0 |  7406 | `				break;` |
|        - |  7407 | `			}` |
|        - |  7408 | `			/* Create Generator class instance */` |
|       24 |  7409 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  7410 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7411 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7412 | `				break;` |
|        - |  7413 | `			}` |
|        - |  7414 | `			/* Store generator in __ctx attribute */` |
|       24 |  7415 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  7416 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  7417 | `			if( pCtxAttr ){` |
|       24 |  7418 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  7419 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  7420 | `			}` |
|        - |  7421 | `			/* Pop args and function name, push Generator object */` |
|       24 |  7422 | `			PH7_MemObjRelease(pTos);` |
|       24 |  7423 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  7424 | `			pTos->x.pOther = pGenObj;` |
|       24 |  7425 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  7426 | `			pGenObj->iRef++;` |
|       24 |  7427 | `			if( pThis ){` |
|      ! 0 |  7428 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7429 | `			}` |
|       24 |  7430 | `			break;` |
|        - |  7431 | `		}` |
|        - |  7432 | `		/* Extract the formal argument set */` |
|    15198 |  7433 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7434 | `		/* Create a new VM frame  */` |
|    15198 |  7435 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    15198 |  7436 | `		if( rc != SXRET_OK ){` |
|        - |  7437 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7438 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7439 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7440 | `				&pVmFunc->sName);` |
|        - |  7441 | `			/* Pop given arguments */` |
|      ! 0 |  7442 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7443 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7444 | `			}` |
|        - |  7445 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7446 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7447 | `			break;` |
|        - |  7448 | `		}` |
|    15198 |  7449 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7450 | `			/* Install the '$this' variable */` |
|        - |  7451 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2244 |  7452 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2244 |  7453 | `			if( pObj ){` |
|        - |  7454 | `				/* Reflect the change */` |
|     2244 |  7455 | `				pObj->x.pOther = pThis;` |
|     2244 |  7456 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1121 |  7457 | `			}` |
|     1121 |  7458 | `		}` |
|    15198 |  7459 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7460 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7461 | `			/* Install static variables */` |
|      ! 0 |  7462 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7463 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7464 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7465 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7466 | `					/* Initialize the static variables */` |
|      ! 0 |  7467 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7468 | `					if( pObj ){` |
|        - |  7469 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7470 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7471 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7472 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7473 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7474 | `						}` |
|      ! 0 |  7475 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7476 | `					}else{` |
|      ! 0 |  7477 | `						continue;` |
|        - |  7478 | `					}` |
|      ! 0 |  7479 | `				}` |
|        - |  7480 | `				/* Install in the current frame */` |
|      ! 0 |  7481 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7482 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7483 | `			}` |
|      ! 0 |  7484 | `		}` |
|        - |  7485 | `		/* Push arguments in the local frame */` |
|        - |  7486 | `		{` |
|    15198 |  7487 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|    15198 |  7488 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  7489 | `			/* ============================================================` |
|        - |  7490 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  7491 | `			 *` |
|        - |  7492 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  7493 | `			 * or position, then install them in the frame.` |
|        - |  7494 | `			 * ============================================================ */` |
|       90 |  7495 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       90 |  7496 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       90 |  7497 | `			sxi32 iVariadicIdx = -1;` |
|        - |  7498 | `			sxu32 nNonVariadic;` |
|        - |  7499 | `			sxi32 *aSlot;` |
|        - |  7500 | `			sxu8  *aUsed;` |
|        - |  7501 | `			sxu32 i;` |
|        - |  7502 | `			/* Find variadic parameter index */` |
|      274 |  7503 | `			for( i = 0; i < nFormal; i++ ){` |
|      194 |  7504 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  7505 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  7506 | `					break;` |
|        - |  7507 | `				}` |
|       94 |  7508 | `			}` |
|       90 |  7509 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  7510 | `			/* Allocate mapping arrays */` |
|      134 |  7511 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       88 |  7512 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       90 |  7513 | `			if( aSlot == 0 ){` |
|      ! 0 |  7514 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  7515 | `				goto Abort;` |
|        - |  7516 | `			}` |
|       90 |  7517 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  7518 | `			/* Resolve named arguments to formal parameters */` |
|      134 |  7519 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       44 |  7520 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       90 |  7521 | `			if( rc == PH7_ABORT ){` |
|        7 |  7522 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  7523 | `				goto Abort;` |
|        - |  7524 | `			}` |
|        - |  7525 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      257 |  7526 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  7527 | `				/* Find the stack arg mapped to formal n */` |
|      175 |  7528 | `				sxi32 iSrc = -1;` |
|      291 |  7529 | `				for( i = 0; i < nActual; i++ ){` |
|      273 |  7530 | `					if( aSlot[i] == (sxi32)n ){` |
|      157 |  7531 | `						iSrc = (sxi32)i;` |
|      157 |  7532 | `						break;` |
|        - |  7533 | `					}` |
|       59 |  7534 | `				}` |
|      175 |  7535 | `				if( iSrc >= 0 ){` |
|        - |  7536 | `					/* Argument was provided — install with type checking */` |
|      157 |  7537 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  7538 | `					/* NULL-to-default redirect (existing behavior) */` |
|      156 |  7539 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  7540 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  7541 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  7542 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7543 | `					}` |
|        - |  7544 | `					/* Type checking: union types */` |
|      157 |  7545 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  7546 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  7547 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       13 |  7548 | `						if( rcU != SXRET_OK ){` |
|        - |  7549 | `							const char *zGiven;` |
|        - |  7550 | `							char zBuf[128];` |
|      ! 0 |  7551 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7552 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  7553 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7554 | `								zGiven = "null";` |
|      ! 0 |  7555 | `							}else{` |
|      ! 0 |  7556 | `								zGiven = ph7_type_name(pVal);` |
|        - |  7557 | `							}` |
|      ! 0 |  7558 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7559 | `								&aFormalArg[n].sName,` |
|      ! 0 |  7560 | `								SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|      ! 0 |  7561 | `									? aFormalArg[n].sTypeName.zString : "union",` |
|      ! 0 |  7562 | `								zGiven);` |
|      ! 0 |  7563 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7564 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7565 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  7566 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7567 | `							pFrameStack = 0;` |
|      ! 0 |  7568 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  7569 | `							goto SkipFuncBody;` |
|        - |  7570 | `						}` |
|      159 |  7571 | `					}else if( aFormalArg[n].nType > 0` |
|       85 |  7572 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  7573 | `						/* Scalar/class type checking */` |
|       17 |  7574 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  7575 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  7576 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  7577 | `							if( pClass ){` |
|      ! 0 |  7578 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7579 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7580 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7581 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7582 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7583 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7584 | `									}` |
|      ! 0 |  7585 | `								}else{` |
|      ! 0 |  7586 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7587 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  7588 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7589 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7590 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7591 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7592 | `									}` |
|        - |  7593 | `								}` |
|      ! 0 |  7594 | `							}` |
|       17 |  7595 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  7596 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  7597 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7598 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  7599 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7600 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7601 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  7602 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7603 | `								pFrameStack = 0;` |
|      ! 0 |  7604 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  7605 | `								goto SkipFuncBody;` |
|      ! 0 |  7606 | `							}else{` |
|        7 |  7607 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        7 |  7608 | `								if( xCast ) xCast(pVal);` |
|        - |  7609 | `							}` |
|        3 |  7610 | `						}` |
|        8 |  7611 | `					}` |
|        - |  7612 | `					/* Install: by reference or by value */` |
|      157 |  7613 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  7614 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7615 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  7616 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7617 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7618 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7619 | `							}` |
|      ! 0 |  7620 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7621 | `						}else{` |
|        7 |  7622 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  7623 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  7624 | `							if( pRefEntry == 0 ){` |
|        7 |  7625 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  7626 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  7627 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  7628 | `								sArg.pUserData = 0;` |
|        5 |  7629 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  7630 | `							}` |
|        5 |  7631 | `							pObj = 0;` |
|        - |  7632 | `						}` |
|        3 |  7633 | `					}else{` |
|      153 |  7634 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7635 | `					}` |
|      157 |  7636 | `					if( pObj ){` |
|      153 |  7637 | `						PH7_MemObjStore(pVal,pObj);` |
|      153 |  7638 | `						sArg.nIdx = pObj->nIdx;` |
|      153 |  7639 | `						sArg.pUserData = 0;` |
|      153 |  7640 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       76 |  7641 | `					}` |
|       79 |  7642 | `				}else{` |
|        - |  7643 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  7644 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7645 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  7646 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  7647 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  7648 | `						if( pObj ){` |
|       19 |  7649 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  7650 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  7651 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  7652 | `							sArg.pUserData = 0;` |
|       19 |  7653 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  7654 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  7655 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7656 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7657 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  7658 | `							}` |
|        9 |  7659 | `						}` |
|        9 |  7660 | `					}` |
|        - |  7661 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  7662 | `				}` |
|       88 |  7663 | `			}` |
|        - |  7664 | `			/* Handle variadic parameter */` |
|       83 |  7665 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  7666 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  7667 | `				if( pObj ){` |
|        9 |  7668 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7669 | `					{` |
|        9 |  7670 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  7671 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  7672 | `							if( aSlot[i] == -1 ){` |
|       16 |  7673 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  7674 | `									/* Named variadic entry: insert with string key */` |
|        - |  7675 | `									ph7_value sKey;` |
|       11 |  7676 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  7677 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  7678 | `										pCallMap3->aNames[i].zString,` |
|       10 |  7679 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  7680 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  7681 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  7682 | `								}else{` |
|        - |  7683 | `									/* Positional variadic entry */` |
|      ! 0 |  7684 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  7685 | `								}` |
|        5 |  7686 | `							}` |
|       12 |  7687 | `						}` |
|        - |  7688 | `					}` |
|        9 |  7689 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  7690 | `					sArg.pUserData = 0;` |
|        9 |  7691 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  7692 | `				}` |
|        5 |  7693 | `			}else{` |
|        - |  7694 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  7695 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  7696 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  7697 | `				 * the positional-only path's behavior. */` |
|       75 |  7698 | `				sxu32 nAnon = nNonVariadic;` |
|      219 |  7699 | `				for( i = 0; i < nActual; i++ ){` |
|      145 |  7700 | `					if( aSlot[i] == -2 ){` |
|        - |  7701 | `						char zAnonBuf[32];` |
|        - |  7702 | `						SyString sAnonName;` |
|      ! 0 |  7703 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  7704 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  7705 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  7706 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  7707 | `						if( pObj ){` |
|      ! 0 |  7708 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  7709 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  7710 | `							sArg.pUserData = 0;` |
|      ! 0 |  7711 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  7712 | `						}` |
|      ! 0 |  7713 | `						nAnon++;` |
|      ! 0 |  7714 | `					}` |
|       73 |  7715 | `				}` |
|        - |  7716 | `			}` |
|        - |  7717 | `			/* Release all stack arguments */` |
|      249 |  7718 | `			for( i = 0; i < nActual; i++ ){` |
|      167 |  7719 | `				PH7_MemObjRelease(&pArg[i]);` |
|       84 |  7720 | `			}` |
|       83 |  7721 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  7722 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       83 |  7723 | `			n = nFormal;` |
|       42 |  7724 | `		}else{` |
|        - |  7725 | `		/* ============================================================` |
|        - |  7726 | `		 * Positional-only matching path (original)` |
|        - |  7727 | `		 * ============================================================ */` |
|    15110 |  7728 | `		n = 0;` |
|    40586 |  7729 | `		while( pArg < pTos ){` |
|    25540 |  7730 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  7731 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       36 |  7732 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       36 |  7733 | `				if( pObj ){` |
|        - |  7734 | `					/* Initialize as empty array */` |
|       36 |  7735 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7736 | `					{` |
|       36 |  7737 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      136 |  7738 | `						while( pArg < pTos ){` |
|        - |  7739 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  7740 | `							 *` |
|        - |  7741 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  7742 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  7743 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  7744 | `							 * non-union variadic path below has the same limitation;` |
|        - |  7745 | `							 * fixing both wants a separate counter for elements` |
|        - |  7746 | `							 * already packed into the variadic array. */` |
|      104 |  7747 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  7748 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  7749 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       16 |  7750 | `								if( rcU != SXRET_OK ){` |
|        - |  7751 | `									const char *zGiven;` |
|        - |  7752 | `									char zBuf[128];` |
|        3 |  7753 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7754 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  7755 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7756 | `										zGiven = "null";` |
|      ! 0 |  7757 | `									}else{` |
|        3 |  7758 | `										zGiven = ph7_type_name(pArg);` |
|        - |  7759 | `									}` |
|        3 |  7760 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  7761 | `										&aFormalArg[n].sName,` |
|        2 |  7762 | `										SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|        2 |  7763 | `											? aFormalArg[n].sTypeName.zString : "union",` |
|        1 |  7764 | `										zGiven);` |
|        3 |  7765 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7766 | `										goto Abort;` |
|        - |  7767 | `									}` |
|        3 |  7768 | `									PH7_MemObjRelease(pTos);` |
|        3 |  7769 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  7770 | `									pFrameStack = 0;` |
|        3 |  7771 | `									rc = PH7_EXCEPTION;` |
|        3 |  7772 | `									goto SkipFuncBody;` |
|        - |  7773 | `								}` |
|       14 |  7774 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  7775 | `								pArg++;` |
|       14 |  7776 | `								continue;` |
|        - |  7777 | `							}` |
|        - |  7778 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  7779 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      104 |  7780 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  7781 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  7782 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  7783 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7784 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  7785 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7786 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  7787 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7788 | `										goto Abort;` |
|        - |  7789 | `									}` |
|        - |  7790 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  7791 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  7792 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7793 | `									pFrameStack = 0;` |
|      ! 0 |  7794 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  7795 | `									goto SkipFuncBody;` |
|      ! 0 |  7796 | `								}else{` |
|       13 |  7797 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  7798 | `									if( xCast ){` |
|       13 |  7799 | `										xCast(pArg);` |
|        6 |  7800 | `									}` |
|        - |  7801 | `								}` |
|        6 |  7802 | `							}` |
|       90 |  7803 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       90 |  7804 | `							pArg++;` |
|        2 |  7805 | `						}` |
|        - |  7806 | `					}` |
|       34 |  7807 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  7808 | `					sArg.pUserData = 0;` |
|       34 |  7809 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  7810 | `				}` |
|       34 |  7811 | `				break; /* All remaining args consumed */` |
|        - |  7812 | `			}` |
|    25506 |  7813 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    25350 |  7814 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       25 |  7815 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  7816 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  7817 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  7818 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7819 | `						goto Abort;` |
|        - |  7820 | `					}` |
|      ! 0 |  7821 | `				}` |
|        - |  7822 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    25352 |  7823 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       77 |  7824 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       50 |  7825 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       52 |  7826 | `					if( rcU != SXRET_OK ){` |
|        - |  7827 | `						const char *zGiven;` |
|        - |  7828 | `						char zBuf[128];` |
|       19 |  7829 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  7830 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  7831 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  7832 | `							zGiven = "null";` |
|        5 |  7833 | `						}else{` |
|        5 |  7834 | `							zGiven = ph7_type_name(pArg);` |
|        - |  7835 | `						}` |
|       19 |  7836 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  7837 | `							&aFormalArg[n].sName,` |
|       18 |  7838 | `							SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|       18 |  7839 | `								? aFormalArg[n].sTypeName.zString : "union",` |
|        9 |  7840 | `							zGiven);` |
|       19 |  7841 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  7842 | `							goto Abort;` |
|        - |  7843 | `						}` |
|       19 |  7844 | `						PH7_MemObjRelease(pTos);` |
|       19 |  7845 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  7846 | `						pFrameStack = 0;` |
|       19 |  7847 | `						rc = PH7_EXCEPTION;` |
|       19 |  7848 | `						goto SkipFuncBody;` |
|        - |  7849 | `					}` |
|       17 |  7850 | `				}else` |
|        - |  7851 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  7852 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    25322 |  7853 | `				if( aFormalArg[n].nType > 0` |
|    13271 |  7854 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1218 |  7855 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  7856 | `						/* Argument must be a class instance [i.e: object] */` |
|       20 |  7857 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  7858 | `						ph7_class *pClass;` |
|        - |  7859 | `						/* Try to extract the desired class */` |
|       20 |  7860 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  7861 | `						if( pClass ){` |
|       20 |  7862 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7863 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7864 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7865 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7866 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7867 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7868 | `								}` |
|      ! 0 |  7869 | `							}else{` |
|        - |  7870 | `								/* reuse pThis declared in outer scope */` |
|       20 |  7871 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  7872 | `								/* Make sure the object is an instance of the given class */` |
|       20 |  7873 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  7874 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7875 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7876 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7877 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7878 | `								}` |
|        - |  7879 | `							}` |
|       11 |  7880 | `						}` |
|     1209 |  7881 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  7882 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7883 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  7884 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  7885 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  7886 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7887 | `								goto Abort;` |
|        - |  7888 | `							}` |
|        - |  7889 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  7890 | `							PH7_MemObjRelease(pTos);` |
|       11 |  7891 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  7892 | `							pFrameStack = 0;` |
|       11 |  7893 | `							rc = PH7_EXCEPTION;` |
|       11 |  7894 | `							goto SkipFuncBody;` |
|      ! 0 |  7895 | `						}else{` |
|      ! 0 |  7896 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7897 | `							/* Cast to the desired type */` |
|      ! 0 |  7898 | `							xCast(pArg);` |
|        - |  7899 | `						}` |
|      ! 0 |  7900 | `					}` |
|      603 |  7901 | `				}` |
|    25324 |  7902 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  7903 | `					/* Pass by reference */` |
|       54 |  7904 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  7905 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  7906 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  7907 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7908 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7909 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7910 | `						}` |
|        - |  7911 | `						/* Switch to pass by value */` |
|      ! 0 |  7912 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7913 | `					}else{` |
|        - |  7914 | `						SyHashEntry *pRefEntry;` |
|        - |  7915 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  7916 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  7917 | `						if( pRefEntry == 0 ){` |
|       80 |  7918 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  7919 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  7920 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  7921 | `							sArg.pUserData = 0;` |
|       54 |  7922 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  7923 | `						}` |
|       54 |  7924 | `						pObj = 0;` |
|        - |  7925 | `					}` |
|       28 |  7926 | `				}else{` |
|        - |  7927 | `					/* Pass by value,make a copy of the given argument */` |
|    25272 |  7928 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7929 | `				}` |
|    12663 |  7930 | `			}else{` |
|        - |  7931 | `				char zName[32];` |
|        - |  7932 | `				SyString sArgName;` |
|        - |  7933 | `				/* Set a dummy name */` |
|      156 |  7934 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  7935 | `				sArgName.zString = zName;` |
|        - |  7936 | `				/* Annonymous argument */` |
|      156 |  7937 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  7938 | `			}` |
|    25478 |  7939 | `			if( pObj ){` |
|    25426 |  7940 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  7941 | `				/* Insert argument index  */` |
|    25426 |  7942 | `				sArg.nIdx = pObj->nIdx;` |
|    25426 |  7943 | `				sArg.pUserData = 0;` |
|    25426 |  7944 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    12712 |  7945 | `			}` |
|    25478 |  7946 | `			PH7_MemObjRelease(pArg);` |
|    25478 |  7947 | `			pArg++;` |
|    25478 |  7948 | `			++n;` |
|        2 |  7949 | `		}` |
|        - |  7950 | `		} /* end named vs positional branch */` |
|        - |  7951 | `		/* Set up closure environment */` |
|    15162 |  7952 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7953 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  7954 | `			ph7_value *pValue;` |
|        - |  7955 | `			sxu32 iEnv;` |
|      111 |  7956 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      287 |  7957 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      177 |  7958 | `				pEnv = &aEnv[iEnv];` |
|      177 |  7959 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  7960 | `					/* Do not install null value */` |
|      105 |  7961 | `					continue;` |
|        - |  7962 | `				}` |
|       73 |  7963 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  7964 | `				if( pValue == 0 ){` |
|      ! 0 |  7965 | `					continue;` |
|        - |  7966 | `				}` |
|        - |  7967 | `				/* Invalidate any prior representation */` |
|       73 |  7968 | `				PH7_MemObjRelease(pValue);` |
|        - |  7969 | `				/* Duplicate bound variable value */` |
|       73 |  7970 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  7971 | `			}` |
|       55 |  7972 | `		}` |
|        - |  7973 | `		/* Process default values for remaining formal parameters */` |
|    17330 |  7974 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2210 |  7975 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7976 | `				/* Variadic parameter with no extra args — create empty array */` |
|       42 |  7977 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  7978 | `				if( pObj ){` |
|       42 |  7979 | `					PH7_MemObjToHashmap(pObj);` |
|       42 |  7980 | `					sArg.nIdx = pObj->nIdx;` |
|       42 |  7981 | `					sArg.pUserData = 0;` |
|       42 |  7982 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  7983 | `				}` |
|       42 |  7984 | `				n++;` |
|       42 |  7985 | `				break; /* Variadic is always last */` |
|        - |  7986 | `			}` |
|     2170 |  7987 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2164 |  7988 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2164 |  7989 | `				if( pObj ){` |
|        - |  7990 | `					/* Evaluate the default value and extract it's result */` |
|     2164 |  7991 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2164 |  7992 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7993 | `						goto Abort;` |
|        - |  7994 | `					}` |
|        - |  7995 | `					/* Insert argument index */` |
|     2164 |  7996 | `					sArg.nIdx = pObj->nIdx;` |
|     2164 |  7997 | `					sArg.pUserData = 0;` |
|     2164 |  7998 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  7999 | `					/* Make sure the default argument is of the correct type */` |
|     2162 |  8000 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1504 |  8001 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  8002 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8003 | `						/* Cast to the desired type */` |
|      ! 0 |  8004 | `						xCast(pObj);` |
|      ! 0 |  8005 | `					}` |
|     1081 |  8006 | `				}` |
|     1081 |  8007 | `			}` |
|     2170 |  8008 | `			++n;` |
|        2 |  8009 | `		}` |
|        - |  8010 | `		} /* end VmCallArgMap scope */` |
|        - |  8011 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8012 | `		 * does not return anything.` |
|        - |  8013 | `		 */` |
|    15162 |  8014 | `		PH7_MemObjRelease(pTos);` |
|    15162 |  8015 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8016 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    15162 |  8017 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    15162 |  8018 | `		if( pFrameStack == 0 ){` |
|        - |  8019 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8020 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8021 | `				&pVmFunc->sName);` |
|      ! 0 |  8022 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8023 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8024 | `			}` |
|      ! 0 |  8025 | `			break;` |
|        - |  8026 | `		}` |
|     7580 |  8027 | `SkipFuncBody:` |
|    15192 |  8028 | `		if( pSelf ){` |
|        - |  8029 | `			/* Push class name */` |
|     2314 |  8030 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1156 |  8031 | `		}` |
|        - |  8032 | `		/* Increment nesting level */` |
|    15192 |  8033 | `		pVm->nRecursionDepth++;` |
|    15192 |  8034 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8035 | `			/* Execute function body */` |
|    15162 |  8036 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     7580 |  8037 | `		}` |
|        - |  8038 | `		/* Decrement nesting level */` |
|    15192 |  8039 | `		pVm->nRecursionDepth--;` |
|    15192 |  8040 | `		if( pSelf ){` |
|        - |  8041 | `			/* Pop class name */` |
|     2314 |  8042 | `			(void)SySetPop(&pVm->aSelf);` |
|     1156 |  8043 | `		}` |
|        - |  8044 | `		/* Cleanup the mess left behind */` |
|    15192 |  8045 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8046 | `			/* Return by reference,reflect that */` |
|        9 |  8047 | `			if( n != SXU32_HIGH ){` |
|        9 |  8048 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8049 | `				sxu32 i;` |
|        - |  8050 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8051 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8052 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8053 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8054 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8055 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8056 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8057 | `								&pVmFunc->sName);` |
|      ! 0 |  8058 | `						}` |
|      ! 0 |  8059 | `						n = SXU32_HIGH;` |
|      ! 0 |  8060 | `						break;` |
|        - |  8061 | `					}` |
|        3 |  8062 | `				}` |
|        5 |  8063 | `			}else{` |
|      ! 0 |  8064 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8065 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8066 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8067 | `						&pVmFunc->sName);` |
|      ! 0 |  8068 | `				}` |
|        - |  8069 | `			}` |
|        9 |  8070 | `			pTos->nIdx = n;` |
|        4 |  8071 | `		}` |
|        - |  8072 | `		/* Cleanup the mess left behind */` |
|    15192 |  8073 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8074 | `			/* An exception was throw in this frame */` |
|       42 |  8075 | `			pFrame = pFrame->pParent;` |
|       42 |  8076 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8077 | `				/* Pop the resutlt */` |
|       40 |  8078 | `				VmPopOperand(&pTos,1);` |
|        - |  8079 | `				/* Jump to this destination */` |
|       40 |  8080 | `				pc = pFrame->iExceptionJump - 1;` |
|       40 |  8081 | `				rc = PH7_OK;` |
|       21 |  8082 | `			}else{` |
|        3 |  8083 | `				if( pFrame->pParent ){` |
|        3 |  8084 | `					rc = PH7_EXCEPTION;` |
|        2 |  8085 | `				}else{` |
|        - |  8086 | `					/* Continue normal execution */` |
|      ! 0 |  8087 | `					rc = PH7_OK;` |
|        - |  8088 | `				}` |
|        - |  8089 | `			}` |
|       20 |  8090 | `		}` |
|        - |  8091 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    15192 |  8092 | `		if( pFrameStack ){` |
|    15162 |  8093 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     7580 |  8094 | `		}` |
|        - |  8095 | `		/* Leave the frame */` |
|    15192 |  8096 | `		VmLeaveFrame(&(*pVm));` |
|    15192 |  8097 | `		if( rc == PH7_ABORT ){` |
|        - |  8098 | `			/* Abort processing immeditaley */` |
|        9 |  8099 | `			goto Abort;` |
|    15184 |  8100 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8101 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8102 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8103 | `			 * overwriting the state saved by the inner level.` |
|        - |  8104 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8105 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8106 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8107 | `			goto Suspend;` |
|    15146 |  8108 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8109 | `			goto Exception;` |
|        - |  8110 | `		}` |
|     7573 |  8111 | `	}else{` |
|        - |  8112 | `		ph7_user_func *pFunc;` |
|        - |  8113 | `		ph7_context sCtx;` |
|        - |  8114 | `		ph7_value sRet;` |
|        - |  8115 | `		/* Look for an installed foreign function.` |
|        - |  8116 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8117 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8118 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8119 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   629202 |  8120 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8121 | `		{` |
|   629202 |  8122 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   629202 |  8123 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8124 | `			/* Compiler-qualified: try short name as global fallback */` |
|       20 |  8125 | `			const char *zShort = sName.zString;` |
|        - |  8126 | `			sxu32 i;` |
|      296 |  8127 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      278 |  8128 | `				if( sName.zString[i] == '\\' ){` |
|       24 |  8129 | `					zShort = &sName.zString[i + 1];` |
|       11 |  8130 | `				}` |
|      140 |  8131 | `			}` |
|       20 |  8132 | `			if( zShort != sName.zString ){` |
|       20 |  8133 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       20 |  8134 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        9 |  8135 | `			}` |
|        9 |  8136 | `		}` |
|        - |  8137 | `		} /* end VmCallArgMap namespace scope */` |
|   629202 |  8138 | `		if( pEntry == 0 ){` |
|        - |  8139 | `			/* Call to undefined function */` |
|        5 |  8140 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8141 | `			/* Pop given arguments */` |
|        5 |  8142 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8143 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8144 | `			}` |
|        - |  8145 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8146 | `			PH7_MemObjRelease(pTos);` |
|        8 |  8147 | `			break;` |
|        - |  8148 | `		}` |
|   629198 |  8149 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8150 | `		/* Start collecting function arguments */` |
|   629198 |  8151 | `		SySetReset(&aArg);` |
|  1692732 |  8152 | `		while( pArg < pTos ){` |
|  1063536 |  8153 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1063536 |  8154 | `			pArg++;` |
|        2 |  8155 | `		}` |
|        - |  8156 | `		/* Assume a null return value */` |
|   629198 |  8157 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8158 | `		/* Init the call context */` |
|   629198 |  8159 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8160 | `		/* Call the foreign function */` |
|   629198 |  8161 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8162 | `		/* Release the call context */` |
|   629198 |  8163 | `		VmReleaseCallContext(&sCtx);` |
|   629198 |  8164 | `		if( rc == PH7_ABORT ){` |
|      471 |  8165 | `			goto Abort;` |
|   628728 |  8166 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  8167 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  8168 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  8169 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8170 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8171 | `				goto Exception;` |
|        - |  8172 | `			}` |
|        - |  8173 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  8174 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  8175 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  8176 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8177 | `			}` |
|        - |  8178 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  8179 | `			VmPopOperand(&pTos,1);` |
|        - |  8180 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  8181 | `			pFrm = pVm->pFrame;` |
|        7 |  8182 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  8183 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  8184 | `			}` |
|        7 |  8185 | `			break;` |
|        - |  8186 | `		}` |
|   628718 |  8187 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8188 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8189 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8190 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8191 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8192 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8193 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8194 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8195 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8196 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8197 | `			}` |
|        - |  8198 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8199 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8200 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8201 | `			goto Suspend;` |
|        - |  8202 | `		}` |
|   628680 |  8203 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8204 | `			/* Pop function name and arguments */` |
|   608804 |  8205 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   304423 |  8206 | `		}` |
|        - |  8207 | `		/* Save foreign function return value */` |
|   628680 |  8208 | `		PH7_MemObjStore(&sRet,pTos);` |
|   628680 |  8209 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8210 | `	}` |
|   643822 |  8211 | `	break;` |
|        - |  8212 | `				  }` |
|        - |  8213 | `/*` |
|        - |  8214 | ` * OP_CONSUME: P1 * *` |
|        - |  8215 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8216 | ` */` |
|    13245 |  8217 | `case PH7_OP_CONSUME: {` |
|    26492 |  8218 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    26492 |  8219 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8220 |  |
|    26492 |  8221 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    26492 |  8222 | `	pCur = pOut;` |
|        - |  8223 | `	/* Start the consume process  */` |
|    52982 |  8224 | `	while( pOut <= pTos ){` |
|        - |  8225 | `		/* Force a string cast */` |
|    26492 |  8226 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      474 |  8227 | `			PH7_MemObjToString(pOut);` |
|      236 |  8228 | `		}` |
|    26492 |  8229 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8230 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8231 | `			/* Invoke the output consumer callback */` |
|    15242 |  8232 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    15242 |  8233 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    15242 |  8234 | `			SyBlobRelease(&pOut->sBlob);` |
|    15242 |  8235 | `			if( rc == SXERR_ABORT ){` |
|        - |  8236 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8237 | `				goto Abort;` |
|        - |  8238 | `			}` |
|     7620 |  8239 | `		}` |
|    26492 |  8240 | `		pOut++;` |
|        2 |  8241 | `	}` |
|    26492 |  8242 | `	pTos = &pCur[-1];` |
|    26490 |  8243 | `	break;` |
|        - |  8244 | `					 }` |
|        - |  8245 |  |
|        - |  8246 | `		} /* Switch() */` |
| 10779326 |  8247 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8248 | `	} /* For(;;) */` |
|    18370 |  8249 | `Done:` |
|    36742 |  8250 | `	SySetRelease(&aArg);` |
|    36742 |  8251 | `	return SXRET_OK;` |
|       72 |  8252 | `Suspend:` |
|      146 |  8253 | `	SySetRelease(&aArg);` |
|      146 |  8254 | `	return PH7_SUSPEND;` |
|      250 |  8255 | `Abort:` |
|      501 |  8256 | `	SySetRelease(&aArg);` |
|     1727 |  8257 | `	while( pTos >= pStack ){` |
|     1227 |  8258 | `		PH7_MemObjRelease(pTos);` |
|     1227 |  8259 | `		pTos--;` |
|        1 |  8260 | `	}` |
|      501 |  8261 | `	return PH7_ABORT;` |
|        3 |  8262 | `Exception:` |
|        8 |  8263 | `	SySetRelease(&aArg);` |
|       22 |  8264 | `	while( pTos >= pStack ){` |
|       16 |  8265 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8266 | `		pTos--;` |
|        2 |  8267 | `	}` |
|        8 |  8268 | `	return PH7_EXCEPTION;` |
|    18697 |  8269 |  |
|        - |  8270 | `/*` |
|        - |  8271 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8272 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8273 | ` * See block-comment on that function for additional information.` |
|        - |  8274 | ` */` |
|    17394 |  8275 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8276 |  |
|        - |  8277 | `	ph7_value *pStack;` |
|        - |  8278 | `	sxi32 rc;` |
|        - |  8279 | `	/* Allocate a new operand stack */` |
|    17396 |  8280 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    17396 |  8281 | `	if( pStack == 0 ){` |
|      ! 0 |  8282 | `		return SXERR_MEM;` |
|        - |  8283 | `	}` |
|        - |  8284 | `	/* Execute the program */` |
|    17396 |  8285 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  8286 | `	/* Free the operand stack */` |
|    17396 |  8287 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8288 | `	/* Execution result */` |
|    17396 |  8289 | `	return rc;` |
|     8699 |  8290 |  |
|        - |  8291 | `/*` |
|        - |  8292 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8293 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8294 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8295 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8296 | ` * execution ends.` |
|        - |  8297 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8298 | ` * additional information.` |
|        - |  8299 | ` */` |
|     2546 |  8300 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8301 |  |
|        - |  8302 | `	VmShutdownCB *pEntry;` |
|        - |  8303 | `	ph7_value *apArg[10];` |
|        - |  8304 | `	sxu32 n,nEntry;` |
|        - |  8305 | `	int i;` |
|        - |  8306 | `	/* Point to the stack of registered callbacks */` |
|     2548 |  8307 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    28008 |  8308 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25462 |  8309 | `		apArg[i] = 0;` |
|    12732 |  8310 | `	}` |
|     2550 |  8311 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8312 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8313 | `		if( pEntry ){` |
|        - |  8314 | `			/* Prepare callback arguments if any */` |
|        3 |  8315 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8316 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8317 | `					break;` |
|        - |  8318 | `				}` |
|      ! 0 |  8319 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8320 | `			}` |
|        - |  8321 | `			/* Invoke the callback */` |
|        3 |  8322 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8323 | `			/*` |
|        - |  8324 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8325 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8326 | `			 */` |
|        3 |  8327 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8328 | `			if( pEntry ){` |
|        3 |  8329 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8330 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8331 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8332 | `				}` |
|        1 |  8333 | `			}` |
|        1 |  8334 | `		}` |
|        2 |  8335 | `	}` |
|     2548 |  8336 | `	SySetReset(&pVm->aShutdown);` |
|     2548 |  8337 |  |
|        - |  8338 | `/*` |
|        - |  8339 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8340 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8341 | ` * See block-comment on that function for additional information.` |
|        - |  8342 | ` */` |
|     2554 |  8343 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  8344 |  |
|        - |  8345 | `	/* Make sure we are ready to execute this program */` |
|     2556 |  8346 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8347 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8348 | `	}` |
|        - |  8349 | `	/* Set the execution magic number  */` |
|     2556 |  8350 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8351 | `	/* Execute the program */` |
|     2556 |  8352 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  8353 | `	/* Invoke any shutdown callbacks */` |
|     2552 |  8354 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8355 | `	/*` |
|        - |  8356 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8357 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8358 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8359 | `	 */` |
|     2552 |  8360 | `	return SXRET_OK;` |
|     1279 |  8361 |  |
|        - |  8362 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8363 | `/*` |
|        - |  8364 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  8365 | ` * The context is in CREATED state and ready to be started.` |
|        - |  8366 | ` */` |
|       46 |  8367 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  8368 |  |
|        - |  8369 | `	ph7_exec_ctx *pCtx;` |
|        - |  8370 | `	ph7_value *pStack;` |
|        - |  8371 | `	VmFrame *pFrame;` |
|       48 |  8372 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  8373 | `	if( pCtx == 0 ){` |
|      ! 0 |  8374 | `		return 0;` |
|        - |  8375 | `	}` |
|       48 |  8376 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  8377 | `	pCtx->pVm = pVm;` |
|       48 |  8378 | `	pCtx->pFunc = pFunc;` |
|       48 |  8379 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  8380 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  8381 | `	pCtx->pc = 0;` |
|       48 |  8382 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  8383 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  8384 | `	/* Allocate a private operand stack */` |
|       48 |  8385 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  8386 | `	if( pStack == 0 ){` |
|      ! 0 |  8387 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8388 | `		return 0;` |
|        - |  8389 | `	}` |
|       48 |  8390 | `	pCtx->pStack = pStack;` |
|        - |  8391 | `	/* Create a detached frame for the fiber */` |
|       48 |  8392 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  8393 | `	if( pFrame == 0 ){` |
|      ! 0 |  8394 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  8395 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8396 | `		return 0;` |
|        - |  8397 | `	}` |
|       48 |  8398 | `	pCtx->pFrame = pFrame;` |
|       48 |  8399 | `	return pCtx;` |
|       25 |  8400 |  |
|        - |  8401 | `/*` |
|        - |  8402 | ` * Start executing a fiber context for the first time.` |
|        - |  8403 | ` */` |
|       46 |  8404 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  8405 |  |
|        - |  8406 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8407 | `	sxi32 rc;` |
|       48 |  8408 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8409 | `		return SXERR_INVALID;` |
|        - |  8410 | `	}` |
|        - |  8411 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  8412 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  8413 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8414 | `	/* Save and set the active context */` |
|       48 |  8415 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  8416 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  8417 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  8418 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  8419 | `	pVm->nRecursionDepth++;` |
|        - |  8420 | `	/* Execute from the beginning */` |
|       71 |  8421 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  8422 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       48 |  8423 | `	pVm->nRecursionDepth--;` |
|        - |  8424 | `	/* Restore the previous context */` |
|       48 |  8425 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  8426 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8427 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  8428 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  8429 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  8430 | `		if( pResult ){` |
|       24 |  8431 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  8432 | `		}` |
|       46 |  8433 | `		return SXRET_OK;` |
|        - |  8434 | `	}` |
|        - |  8435 | `	/* Detach frame */` |
|        3 |  8436 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  8437 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  8438 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  8439 | `	}` |
|        3 |  8440 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8441 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8442 | `		return PH7_ABORT;` |
|        - |  8443 | `	}` |
|        3 |  8444 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8445 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8446 | `		return PH7_EXCEPTION;` |
|        - |  8447 | `	}` |
|        - |  8448 | `	/* Normal completion */` |
|        3 |  8449 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  8450 | `	if( pResult ){` |
|        3 |  8451 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  8452 | `	}` |
|        3 |  8453 | `	return SXRET_OK;` |
|       25 |  8454 |  |
|        - |  8455 | `/*` |
|        - |  8456 | ` * Resume a suspended fiber context.` |
|        - |  8457 | ` */` |
|       98 |  8458 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  8459 |  |
|        - |  8460 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8461 | `	sxi32 rc;` |
|      100 |  8462 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  8463 | `		return SXERR_INVALID;` |
|        - |  8464 | `	}` |
|        - |  8465 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  8466 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  8467 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  8468 | `	if( pResumeValue ){` |
|       40 |  8469 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  8470 | `	}else{` |
|       62 |  8471 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  8472 | `	}` |
|      100 |  8473 | `	pCtx->nTos++;` |
|        - |  8474 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  8475 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  8476 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8477 | `	/* Save and set the active context */` |
|      100 |  8478 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  8479 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  8480 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  8481 | `	pVm->nRecursionDepth++;` |
|        - |  8482 | `	/* Resume execution from saved PC */` |
|      149 |  8483 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  8484 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|      100 |  8485 | `	pVm->nRecursionDepth--;` |
|        - |  8486 | `	/* Restore the previous context */` |
|      100 |  8487 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  8488 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8489 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  8490 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  8491 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  8492 | `		if( pResult ){` |
|       18 |  8493 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  8494 | `		}` |
|       64 |  8495 | `		return SXRET_OK;` |
|        - |  8496 | `	}` |
|        - |  8497 | `	/* Detach frame */` |
|       38 |  8498 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  8499 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  8500 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  8501 | `	}` |
|       38 |  8502 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8503 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8504 | `		return PH7_ABORT;` |
|        - |  8505 | `	}` |
|       38 |  8506 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8507 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8508 | `		return PH7_EXCEPTION;` |
|        - |  8509 | `	}` |
|        - |  8510 | `	/* Normal completion */` |
|       38 |  8511 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  8512 | `	if( pResult ){` |
|       20 |  8513 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  8514 | `	}` |
|       38 |  8515 | `	return SXRET_OK;` |
|       51 |  8516 |  |
|        - |  8517 | `/*` |
|        - |  8518 | ` * Release an execution context and all its resources.` |
|        - |  8519 | ` */` |
|        4 |  8520 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  8521 |  |
|        5 |  8522 | `	if( pCtx == 0 ){` |
|      ! 0 |  8523 | `		return;` |
|        - |  8524 | `	}` |
|        5 |  8525 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  8526 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  8527 | `		return;` |
|        - |  8528 | `	}` |
|        5 |  8529 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  8530 | `	/* Release values */` |
|        5 |  8531 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  8532 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  8533 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  8534 | `	if( pCtx->pFrame ){` |
|        - |  8535 | `		VmSlot *aSlot;` |
|        - |  8536 | `		sxu32 n;` |
|        - |  8537 | `		/* Free local variables */` |
|        5 |  8538 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  8539 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  8540 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  8541 | `		}` |
|        - |  8542 | `		/* Remove local references */` |
|        5 |  8543 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  8544 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  8545 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  8546 | `		}` |
|        5 |  8547 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  8548 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  8549 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  8550 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  8551 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  8552 | `		pCtx->pFrame = 0;` |
|        2 |  8553 | `	}` |
|        - |  8554 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  8555 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  8556 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  8557 | `	if( pCtx->pStack ){` |
|        5 |  8558 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  8559 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  8560 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  8561 | `				PH7_MemObjRelease(pTos);` |
|        5 |  8562 | `				pTos--;` |
|        1 |  8563 | `			}` |
|        2 |  8564 | `		}` |
|        5 |  8565 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  8566 | `		pCtx->pStack = 0;` |
|        2 |  8567 | `	}` |
|        - |  8568 | `	/* Free the context itself */` |
|        5 |  8569 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  8570 |  |
|        - |  8571 | `/*` |
|        - |  8572 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  8573 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  8574 | ` */` |
|       90 |  8575 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  8576 |  |
|        - |  8577 | `	ph7_class_instance *pThis;` |
|        - |  8578 | `	SyString sAttr;` |
|        - |  8579 | `	ph7_value *pAttr;` |
|       92 |  8580 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8581 | `		return 0;` |
|        - |  8582 | `	}` |
|       92 |  8583 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  8584 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  8585 | `		return 0;` |
|        - |  8586 | `	}` |
|       92 |  8587 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  8588 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  8589 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  8590 | `		return 0;` |
|        - |  8591 | `	}` |
|       62 |  8592 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  8593 |  |
|        - |  8594 | `/*` |
|        - |  8595 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  8596 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  8597 | ` */` |
|       38 |  8598 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8599 |  |
|       40 |  8600 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  8601 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  8602 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8603 | `			"Cannot suspend outside of a fiber");` |
|        - |  8604 | `	}` |
|       40 |  8605 | `	if( nArg > 0 ){` |
|       40 |  8606 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  8607 | `	}else{` |
|      ! 0 |  8608 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  8609 | `	}` |
|       40 |  8610 | `	return PH7_SUSPEND;` |
|       21 |  8611 |  |
|        - |  8612 | `/*` |
|        - |  8613 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  8614 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  8615 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  8616 | ` */` |
|       24 |  8617 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8618 |  |
|        - |  8619 | `	ph7_class_instance *pThis;` |
|        - |  8620 | `	ph7_value *pAttr;` |
|        - |  8621 | `	SyString sAttrName;` |
|       26 |  8622 | `	if( nArg < 2 ){` |
|      ! 0 |  8623 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8624 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  8625 | `	}` |
|       26 |  8626 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8627 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8628 | `			"Fiber::__construct(): invalid $this");` |
|        - |  8629 | `	}` |
|       26 |  8630 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  8631 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  8632 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8633 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  8634 | `	}` |
|        - |  8635 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  8636 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8637 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8638 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  8639 | `	}` |
|        - |  8640 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  8641 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8642 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8643 | `	if( pAttr ){` |
|       26 |  8644 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  8645 | `	}` |
|       26 |  8646 | `	return PH7_OK;` |
|       14 |  8647 |  |
|        - |  8648 | `/*` |
|        - |  8649 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  8650 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  8651 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  8652 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  8653 | ` */` |
|       24 |  8654 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  8655 | `	ph7_class_instance **ppThis)` |
|        2 |  8656 |  |
|       26 |  8657 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8658 | `	ph7_value *pCallable;` |
|        - |  8659 | `	SyString sAttrName;` |
|       26 |  8660 | `	*ppThis = 0;` |
|       26 |  8661 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8662 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  8663 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8664 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  8665 | `		return 0;` |
|        - |  8666 | `	}` |
|       26 |  8667 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8668 | `		/* String callable — look up in user functions with overload support */` |
|        - |  8669 | `		SyString sName;` |
|        - |  8670 | `		SyHashEntry *pEntry;` |
|        - |  8671 | `		ph7_vm_func *pFunc;` |
|       26 |  8672 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  8673 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  8674 | `		if( pEntry == 0 ){` |
|      ! 0 |  8675 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  8676 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  8677 | `			return 0;` |
|        - |  8678 | `		}` |
|       26 |  8679 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  8680 | `		return pFunc;` |
|      ! 0 |  8681 | `	}else{` |
|        - |  8682 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  8683 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8684 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8685 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8686 | `		if( pMethod == 0 ){` |
|      ! 0 |  8687 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8688 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  8689 | `			return 0;` |
|        - |  8690 | `		}` |
|      ! 0 |  8691 | `		*ppThis = pClosure;` |
|      ! 0 |  8692 | `		return &pMethod->sFunc;` |
|        - |  8693 | `	}` |
|       14 |  8694 |  |
|        - |  8695 | `/*` |
|        - |  8696 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  8697 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  8698 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  8699 | ` */` |
|       46 |  8700 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  8701 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  8702 |  |
|       48 |  8703 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  8704 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  8705 | `	sxu32 nFormal, n;` |
|        - |  8706 | `	VmSlot sSlot;` |
|        - |  8707 | `	sxi32 rc;` |
|        - |  8708 | `	/* Install $this for closure/method callables */` |
|       48 |  8709 | `	if( pClosureThis ){` |
|        - |  8710 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  8711 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  8712 | `		if( pObj ){` |
|      ! 0 |  8713 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  8714 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  8715 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  8716 | `		}` |
|      ! 0 |  8717 | `	}` |
|        - |  8718 | `	/* Install static variables */` |
|       48 |  8719 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  8720 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  8721 | `		ph7_value *pVal;` |
|      ! 0 |  8722 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  8723 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  8724 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  8725 | `			if( pVal ){` |
|      ! 0 |  8726 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8727 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  8728 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  8729 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  8730 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  8731 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  8732 | `				}` |
|      ! 0 |  8733 | `			}` |
|      ! 0 |  8734 | `		}` |
|      ! 0 |  8735 | `	}` |
|        - |  8736 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  8737 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  8738 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  8739 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  8740 | `		ph7_value *pObj;` |
|       20 |  8741 | `		if( n < (sxu32)nArg ){` |
|        - |  8742 | `			/* Argument provided — install with type casting */` |
|       20 |  8743 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  8744 | `			if( pObj ){` |
|       20 |  8745 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  8746 | `				/* Type casting */` |
|       20 |  8747 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8748 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8749 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8750 | `						if( xCast ){` |
|      ! 0 |  8751 | `							xCast(pObj);` |
|      ! 0 |  8752 | `						}` |
|      ! 0 |  8753 | `					}` |
|      ! 0 |  8754 | `				}` |
|       20 |  8755 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  8756 | `				sSlot.pUserData = 0;` |
|       20 |  8757 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  8758 | `			}` |
|        9 |  8759 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  8760 | `			/* Default value */` |
|      ! 0 |  8761 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  8762 | `			if( pObj ){` |
|      ! 0 |  8763 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  8764 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8765 | `					return rc;` |
|        - |  8766 | `				}` |
|      ! 0 |  8767 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8768 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8769 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8770 | `						if( xCast ){` |
|      ! 0 |  8771 | `							xCast(pObj);` |
|      ! 0 |  8772 | `						}` |
|      ! 0 |  8773 | `					}` |
|      ! 0 |  8774 | `				}` |
|      ! 0 |  8775 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  8776 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8777 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  8778 | `			}` |
|      ! 0 |  8779 | `		}` |
|       11 |  8780 | `	}` |
|        - |  8781 | `	/* Install closure environment (captured variables) */` |
|       48 |  8782 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8783 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  8784 | `		ph7_value *pValue;` |
|        - |  8785 | `		sxu32 iEnv;` |
|        3 |  8786 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  8787 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  8788 | `			pEnv = &aEnv[iEnv];` |
|        7 |  8789 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  8790 | `				continue;` |
|        - |  8791 | `			}` |
|        5 |  8792 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  8793 | `			if( pValue == 0 ){` |
|      ! 0 |  8794 | `				continue;` |
|        - |  8795 | `			}` |
|        5 |  8796 | `			PH7_MemObjRelease(pValue);` |
|        5 |  8797 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  8798 | `		}` |
|        1 |  8799 | `	}` |
|       48 |  8800 | `	return SXRET_OK;` |
|       25 |  8801 |  |
|        - |  8802 | `/*` |
|        - |  8803 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  8804 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  8805 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  8806 | ` */` |
|       26 |  8807 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8808 |  |
|       28 |  8809 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8810 | `	ph7_class_instance *pThis;` |
|        - |  8811 | `	ph7_class_instance *pClosureThis;` |
|        - |  8812 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8813 | `	ph7_vm_func *pFunc;` |
|        - |  8814 | `	ph7_value sResult;` |
|        - |  8815 | `	ph7_value *pCtxAttr;` |
|        - |  8816 | `	SyString sAttrName;` |
|        - |  8817 | `	sxi32 rc;` |
|       28 |  8818 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8819 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  8820 | `	}` |
|       28 |  8821 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8822 | `	/* Check if already started (has a __ctx) */` |
|       28 |  8823 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  8824 | `	if( pExecCtx != 0 ){` |
|        3 |  8825 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8826 | `			"Cannot start a fiber that has already been started");` |
|        - |  8827 | `	}` |
|        - |  8828 | `	/* Resolve callable */` |
|       26 |  8829 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  8830 | `	if( pFunc == 0 ){` |
|      ! 0 |  8831 | `		return PH7_EXCEPTION;` |
|        - |  8832 | `	}` |
|        - |  8833 | `	/* Create execution context now that we know the function */` |
|       26 |  8834 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  8835 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8836 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8837 | `			"Fiber::start(): out of memory");` |
|        - |  8838 | `	}` |
|        - |  8839 | `	/* Store context in $this->__ctx */` |
|       26 |  8840 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  8841 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8842 | `	if( pCtxAttr ){` |
|       26 |  8843 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  8844 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  8845 | `	}` |
|        - |  8846 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  8847 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  8848 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  8849 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  8850 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  8851 | `	/* Unpack the args array and install into the frame */` |
|        - |  8852 | `	{` |
|       26 |  8853 | `		ph7_value **apValues = 0;` |
|       26 |  8854 | `		int nActual = 0;` |
|       26 |  8855 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  8856 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  8857 | `			ph7_hashmap_node *pNode;` |
|       26 |  8858 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  8859 | `			if( nCount > 0 ){` |
|        3 |  8860 | `				sxu32 idx = 0;` |
|        4 |  8861 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  8862 | `					nCount * sizeof(ph7_value *));` |
|        3 |  8863 | `				if( apValues ){` |
|        3 |  8864 | `					pNode = pMap->pFirst;` |
|        7 |  8865 | `					while( pNode && idx < nCount ){` |
|        5 |  8866 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  8867 | `						idx++;` |
|        5 |  8868 | `						pNode = pNode->pPrev;` |
|        1 |  8869 | `					}` |
|        3 |  8870 | `					nActual = (int)idx;` |
|        1 |  8871 | `				}` |
|        1 |  8872 | `			}` |
|       12 |  8873 | `		}` |
|       26 |  8874 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  8875 | `		if( apValues ){` |
|        3 |  8876 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  8877 | `		}` |
|        - |  8878 | `	}` |
|        - |  8879 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  8880 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  8881 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  8882 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8883 | `		return PH7_ABORT;` |
|        - |  8884 | `	}` |
|       26 |  8885 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  8886 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  8887 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8888 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8889 | `		return PH7_ABORT;` |
|        - |  8890 | `	}` |
|       26 |  8891 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8892 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8893 | `		return PH7_EXCEPTION;` |
|        - |  8894 | `	}` |
|       26 |  8895 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  8896 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  8897 | `	return PH7_OK;` |
|       15 |  8898 |  |
|        - |  8899 | `/*` |
|        - |  8900 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  8901 | ` */` |
|       36 |  8902 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8903 |  |
|       38 |  8904 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8905 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8906 | `	ph7_value sResult;` |
|        - |  8907 | `	ph7_value *pResumeVal;` |
|        - |  8908 | `	sxi32 rc;` |
|       38 |  8909 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8910 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  8911 | `		return PH7_OK;` |
|        - |  8912 | `	}` |
|       38 |  8913 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  8914 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8915 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  8916 | `		return PH7_OK;` |
|        - |  8917 | `	}` |
|       38 |  8918 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8919 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8920 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  8921 | `	}` |
|       36 |  8922 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  8923 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  8924 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  8925 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8926 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8927 | `		return PH7_ABORT;` |
|        - |  8928 | `	}` |
|       36 |  8929 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8930 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8931 | `		return PH7_EXCEPTION;` |
|        - |  8932 | `	}` |
|       36 |  8933 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  8934 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  8935 | `	return PH7_OK;` |
|       20 |  8936 |  |
|        - |  8937 | `/*` |
|        - |  8938 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  8939 | ` */` |
|        6 |  8940 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8941 |  |
|        8 |  8942 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8943 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  8944 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8945 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8946 | `		return PH7_OK;` |
|        - |  8947 | `	}` |
|        8 |  8948 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  8949 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8950 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8951 | `		return PH7_OK;` |
|        - |  8952 | `	}` |
|        8 |  8953 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8954 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8955 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8956 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  8957 | `		}` |
|      ! 0 |  8958 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8959 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  8960 | `	}` |
|        8 |  8961 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  8962 | `	return PH7_OK;` |
|        5 |  8963 |  |
|        - |  8964 | `/*` |
|        - |  8965 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  8966 | ` */` |
|        6 |  8967 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8968 |  |
|        - |  8969 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8970 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8971 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8972 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  8973 | `	return PH7_OK;` |
|        4 |  8974 |  |
|      ! 0 |  8975 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8976 |  |
|        - |  8977 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  8978 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  8979 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8980 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  8981 | `	return PH7_OK;` |
|      ! 0 |  8982 |  |
|        6 |  8983 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8984 |  |
|        - |  8985 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8986 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8987 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8988 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  8989 | `	return PH7_OK;` |
|        4 |  8990 |  |
|        6 |  8991 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8992 |  |
|        - |  8993 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8994 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8995 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8996 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  8997 | `	return PH7_OK;` |
|        4 |  8998 |  |
|        - |  8999 | `/*` |
|        - |  9000 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9001 | ` */` |
|        4 |  9002 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9003 |  |
|        5 |  9004 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9005 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9006 | `	if( nArg < 1 ){` |
|      ! 0 |  9007 | `		return PH7_OK;` |
|        - |  9008 | `	}` |
|        5 |  9009 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9010 | `	if( pExecCtx ){` |
|        5 |  9011 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9012 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9013 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9014 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9015 | `			SyString sAttrName;` |
|        - |  9016 | `			ph7_value *pAttr;` |
|        5 |  9017 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9018 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9019 | `			if( pAttr ){` |
|        5 |  9020 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9021 | `			}` |
|        2 |  9022 | `		}` |
|        2 |  9023 | `	}` |
|        5 |  9024 | `	return PH7_OK;` |
|        3 |  9025 |  |
|        - |  9026 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9027 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9028 |  |
|        - |  9029 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9030 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9031 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9032 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9033 |  |
|      ! 0 |  9034 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9035 |  |
|        - |  9036 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9037 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9038 | `	ph7_exec_ctx *pCtx;` |
|        - |  9039 | `	ph7_vm_func *pFunc;` |
|        - |  9040 | `	ph7_value *pCallable;` |
|        - |  9041 | `	ph7_value *pCtxAttr;` |
|        - |  9042 | `	SyString sAttrName;` |
|        - |  9043 | `	/* Must not already be started */` |
|      ! 0 |  9044 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9045 | `	if( pCtx != 0 ){` |
|      ! 0 |  9046 | `		return SXERR_INVALID;` |
|        - |  9047 | `	}` |
|      ! 0 |  9048 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9049 | `		return SXERR_INVALID;` |
|        - |  9050 | `	}` |
|      ! 0 |  9051 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9052 | `	/* Get the callable */` |
|      ! 0 |  9053 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9054 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9055 | `	if( pCallable == 0 ){` |
|      ! 0 |  9056 | `		return SXERR_INVALID;` |
|        - |  9057 | `	}` |
|        - |  9058 | `	/* Resolve callable */` |
|      ! 0 |  9059 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9060 | `		SyString sName;` |
|        - |  9061 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9062 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9063 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9064 | `		if( pEntry == 0 ){` |
|      ! 0 |  9065 | `			return SXERR_NOTFOUND;` |
|        - |  9066 | `		}` |
|      ! 0 |  9067 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9068 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9069 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9070 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9071 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9072 | `		if( pMethod == 0 ){` |
|      ! 0 |  9073 | `			return SXERR_INVALID;` |
|        - |  9074 | `		}` |
|      ! 0 |  9075 | `		pClosureThis = pClosure;` |
|      ! 0 |  9076 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9077 | `	}else{` |
|      ! 0 |  9078 | `		return SXERR_INVALID;` |
|        - |  9079 | `	}` |
|        - |  9080 | `	/* Create context */` |
|      ! 0 |  9081 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9082 | `	if( pCtx == 0 ){` |
|      ! 0 |  9083 | `		return SXERR_MEM;` |
|        - |  9084 | `	}` |
|        - |  9085 | `	/* Store in __ctx */` |
|      ! 0 |  9086 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9087 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9088 | `	if( pCtxAttr ){` |
|      ! 0 |  9089 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9090 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9091 | `	}` |
|        - |  9092 | `	/* Set up frame with args */` |
|      ! 0 |  9093 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9094 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9095 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9096 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9097 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9098 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9099 |  |
|      ! 0 |  9100 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9101 |  |
|      ! 0 |  9102 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9103 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9104 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9105 |  |
|      ! 0 |  9106 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9107 |  |
|      ! 0 |  9108 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9109 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9110 |  |
|      ! 0 |  9111 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9112 |  |
|      ! 0 |  9113 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9114 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9115 |  |
|      ! 0 |  9116 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9117 |  |
|      ! 0 |  9118 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9119 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9120 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9121 |  |
|        - |  9122 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9123 | `/*` |
|        - |  9124 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9125 | ` */` |
|       22 |  9126 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9127 |  |
|        - |  9128 | `	ph7_generator *pGen;` |
|       24 |  9129 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9130 | `	if( pGen == 0 ){` |
|      ! 0 |  9131 | `		return 0;` |
|        - |  9132 | `	}` |
|       24 |  9133 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9134 | `	pGen->pCtx = pCtx;` |
|       24 |  9135 | `	pGen->iImplicitKey = 0;` |
|       24 |  9136 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9137 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9138 | `	/* Link the generator back to the exec context */` |
|       24 |  9139 | `	pCtx->pPrivate = pGen;` |
|       24 |  9140 | `	return pGen;` |
|       13 |  9141 |  |
|        - |  9142 | `/*` |
|        - |  9143 | ` * Release a generator and its execution context.` |
|        - |  9144 | ` */` |
|      ! 0 |  9145 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9146 |  |
|      ! 0 |  9147 | `	if( pGen == 0 ){` |
|      ! 0 |  9148 | `		return;` |
|        - |  9149 | `	}` |
|      ! 0 |  9150 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9151 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9152 | `	if( pGen->pCtx ){` |
|      ! 0 |  9153 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9154 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9155 | `		pGen->pCtx = 0;` |
|      ! 0 |  9156 | `	}` |
|      ! 0 |  9157 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9158 |  |
|        - |  9159 | `/*` |
|        - |  9160 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9161 | ` */` |
|      236 |  9162 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9163 |  |
|        - |  9164 | `	ph7_class_instance *pThis;` |
|        - |  9165 | `	SyString sAttr;` |
|        - |  9166 | `	ph7_value *pAttr;` |
|      238 |  9167 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9168 | `		return 0;` |
|        - |  9169 | `	}` |
|      238 |  9170 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9171 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9172 | `		return 0;` |
|        - |  9173 | `	}` |
|      238 |  9174 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9175 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9176 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9177 | `		return 0;` |
|        - |  9178 | `	}` |
|      238 |  9179 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9180 |  |
|        - |  9181 | `/*` |
|        - |  9182 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9183 | ` */` |
|       22 |  9184 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9185 |  |
|        - |  9186 | `	ph7_generator *pGen;` |
|        - |  9187 | `	sxi32 rc;` |
|       24 |  9188 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9189 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9190 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9191 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9192 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9193 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9194 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9195 | `	}` |
|       24 |  9196 | `	return PH7_OK;` |
|       13 |  9197 |  |
|        - |  9198 | `/*` |
|        - |  9199 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9200 | ` */` |
|       68 |  9201 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9202 |  |
|        - |  9203 | `	ph7_generator *pGen;` |
|       70 |  9204 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9205 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9206 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9207 | `	return PH7_OK;` |
|       36 |  9208 |  |
|        - |  9209 | `/*` |
|        - |  9210 | ` * Generator::current() — return the last yielded value.` |
|        - |  9211 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9212 | ` */` |
|       68 |  9213 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9214 |  |
|        - |  9215 | `	ph7_generator *pGen;` |
|        - |  9216 | `	sxi32 rc;` |
|       70 |  9217 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9218 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9219 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9220 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9221 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9222 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9223 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9224 | `	}` |
|       70 |  9225 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9226 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9227 | `	}else{` |
|      ! 0 |  9228 | `		ph7_result_null(pCtx);` |
|        - |  9229 | `	}` |
|       70 |  9230 | `	return PH7_OK;` |
|       36 |  9231 |  |
|        - |  9232 | `/*` |
|        - |  9233 | ` * Generator::key() — return the last yielded key.` |
|        - |  9234 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9235 | ` */` |
|       12 |  9236 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9237 |  |
|        - |  9238 | `	ph7_generator *pGen;` |
|        - |  9239 | `	sxi32 rc;` |
|       13 |  9240 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9241 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9242 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9243 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9244 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9245 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9246 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9247 | `	}` |
|       13 |  9248 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9249 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9250 | `	}else{` |
|      ! 0 |  9251 | `		ph7_result_null(pCtx);` |
|        - |  9252 | `	}` |
|       13 |  9253 | `	return PH7_OK;` |
|        7 |  9254 |  |
|        - |  9255 | `/*` |
|        - |  9256 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9257 | ` */` |
|       60 |  9258 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9259 |  |
|        - |  9260 | `	ph7_generator *pGen;` |
|        - |  9261 | `	sxi32 rc;` |
|       62 |  9262 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9263 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9264 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9265 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9266 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9267 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9268 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9269 | `	}else{` |
|      ! 0 |  9270 | `		return PH7_OK;` |
|        - |  9271 | `	}` |
|       62 |  9272 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9273 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9274 | `	return PH7_OK;` |
|       32 |  9275 |  |
|        - |  9276 | `/*` |
|        - |  9277 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9278 | ` */` |
|        4 |  9279 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9280 |  |
|        - |  9281 | `	ph7_generator *pGen;` |
|        - |  9282 | `	ph7_value *pSendVal;` |
|        - |  9283 | `	sxi32 rc;` |
|        5 |  9284 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9285 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9286 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9287 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9288 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9289 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9290 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9291 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9292 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9293 | `	}else{` |
|      ! 0 |  9294 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9295 | `		return PH7_OK;` |
|        - |  9296 | `	}` |
|        5 |  9297 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9298 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9299 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9300 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9301 | `	}else{` |
|        3 |  9302 | `		ph7_result_null(pCtx);` |
|        - |  9303 | `	}` |
|        5 |  9304 | `	return PH7_OK;` |
|        3 |  9305 |  |
|        - |  9306 | `/*` |
|        - |  9307 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9308 | ` *` |
|        - |  9309 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9310 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9311 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9312 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9313 | ` * the exception to the caller.` |
|        - |  9314 | ` */` |
|      ! 0 |  9315 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9316 |  |
|        - |  9317 | `	ph7_generator *pGen;` |
|        - |  9318 | `	const char *zMsg;` |
|        - |  9319 | `	int nLen;` |
|      ! 0 |  9320 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9321 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9322 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9323 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9324 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9325 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9326 | `			"Cannot throw into a closed generator");` |
|        - |  9327 | `	}` |
|        - |  9328 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9329 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9330 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9331 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9332 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9333 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  9334 | `	nLen = 0;` |
|      ! 0 |  9335 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  9336 | `		/* Try to get the exception's message */` |
|        - |  9337 | `		SyString sAttr;` |
|        - |  9338 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  9339 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  9340 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  9341 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  9342 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  9343 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  9344 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  9345 | `		}` |
|      ! 0 |  9346 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  9347 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  9348 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  9349 | `	}` |
|      ! 0 |  9350 | `	(void)nLen;` |
|      ! 0 |  9351 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  9352 |  |
|        - |  9353 | `/*` |
|        - |  9354 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  9355 | ` */` |
|        2 |  9356 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9357 |  |
|        - |  9358 | `	ph7_generator *pGen;` |
|        3 |  9359 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9360 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  9361 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9362 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9363 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9364 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  9365 | `	}` |
|        3 |  9366 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  9367 | `	return PH7_OK;` |
|        2 |  9368 |  |
|        - |  9369 | `/*` |
|        - |  9370 | ` * Generator::__destruct() — clean up.` |
|        - |  9371 | ` */` |
|      ! 0 |  9372 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9373 |  |
|        - |  9374 | `	ph7_generator *pGen;` |
|      ! 0 |  9375 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  9376 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9377 | `	if( pGen ){` |
|      ! 0 |  9378 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  9379 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9380 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9381 | `			SyString sAttrName;` |
|        - |  9382 | `			ph7_value *pAttr;` |
|      ! 0 |  9383 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9384 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9385 | `			if( pAttr ){` |
|      ! 0 |  9386 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  9387 | `			}` |
|      ! 0 |  9388 | `		}` |
|      ! 0 |  9389 | `	}` |
|      ! 0 |  9390 | `	return PH7_OK;` |
|      ! 0 |  9391 |  |
|        - |  9392 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  9393 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  9394 | `/*` |
|        - |  9395 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  9396 | ` * the desired message.` |
|        - |  9397 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  9398 | ` * in 'api.c' for additional information.` |
|        - |  9399 | ` */` |
|      370 |  9400 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  9401 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  9402 | `	SyString *pString /* Message to output */` |
|        - |  9403 | `	)` |
|        2 |  9404 |  |
|      372 |  9405 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  9406 | `	sxi32 rc = SXRET_OK;` |
|        - |  9407 | `	/* Call the output consumer */` |
|      372 |  9408 | `	if( pString->nByte > 0 ){` |
|      372 |  9409 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  9410 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  9411 | `	}` |
|      372 |  9412 | `	return rc;` |
|        2 |  9413 |  |
|        - |  9414 | `/*` |
|        - |  9415 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  9416 | ` * callback to consume the formatted message.` |
|        - |  9417 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  9418 | ` * in 'api.c' for additional information.` |
|        - |  9419 | ` */` |
|        2 |  9420 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  9421 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  9422 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  9423 | `	va_list ap           /* Variable list of arguments */` |
|        - |  9424 | `	)` |
|        1 |  9425 |  |
|        3 |  9426 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  9427 | `	sxi32 rc = SXRET_OK;` |
|        - |  9428 | `	SyBlob sWorker;` |
|        - |  9429 | `	/* Format the message and call the output consumer */` |
|        3 |  9430 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  9431 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  9432 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  9433 | `		/* Consume the formatted message */` |
|        3 |  9434 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  9435 | `	}` |
|        3 |  9436 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  9437 | `	/* Release the working buffer */` |
|        3 |  9438 | `	SyBlobRelease(&sWorker);` |
|        3 |  9439 | `	return rc;` |
|        1 |  9440 |  |
|        - |  9441 | `/*` |
|        - |  9442 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  9443 | ` * This function never fail and always return a pointer` |
|        - |  9444 | ` * to a null terminated string.` |
|        - |  9445 | ` */` |
|       12 |  9446 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  9447 |  |
|       13 |  9448 | `	const char *zOp = "Unknown     ";` |
|       13 |  9449 | `	switch(nOp){` |
|        3 |  9450 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  9451 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  9452 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  9453 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  9454 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  9455 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  9456 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  9457 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  9458 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  9459 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  9460 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  9461 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  9462 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  9463 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  9464 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  9465 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  9466 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  9467 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  9468 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  9469 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  9470 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  9471 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  9472 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  9473 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  9474 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  9475 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  9476 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  9477 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  9478 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  9479 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  9480 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  9481 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  9482 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  9483 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  9484 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  9485 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  9486 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  9487 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  9488 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  9489 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  9490 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  9491 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  9492 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  9493 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  9494 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  9495 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9496 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9497 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9498 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9499 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9500 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  9501 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  9502 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  9503 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9504 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9505 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9506 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9507 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9508 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9509 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9510 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9511 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9512 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9513 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9514 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9515 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9516 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  9517 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  9518 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9519 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9520 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9521 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9522 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9523 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9524 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9525 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9526 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9527 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9528 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9529 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9530 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9531 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9532 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9533 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9534 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9535 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9536 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9537 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9538 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9539 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 |  9540 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  9541 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  9542 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  9543 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  9544 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  9545 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  9546 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  9547 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  9548 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  9549 | `	default:` |
|      ! 0 |  9550 | `		break;` |
|        - |  9551 | `	}` |
|       13 |  9552 | `	return zOp;` |
|        1 |  9553 |  |
|        - |  9554 | `/*` |
|        - |  9555 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  9556 | ` * The xConsumer() callback which is an used defined function` |
|        - |  9557 | ` * is responsible of consuming the generated dump.` |
|        - |  9558 | ` */` |
|        2 |  9559 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  9560 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  9561 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  9562 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  9563 | `	)` |
|        1 |  9564 |  |
|        - |  9565 | `	sxi32 rc;` |
|        3 |  9566 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  9567 | `	return rc;` |
|        1 |  9568 |  |
|        - |  9569 | `/*` |
|        - |  9570 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  9571 | ` * outside a class body [i.e: global or function scope].` |
|        - |  9572 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  9573 | ` * in 'compile.c' for additional information.` |
|        - |  9574 | ` */` |
|       14 |  9575 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  9576 |  |
|       15 |  9577 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  9578 | `	/* Evaluate and expand constant value */` |
|       15 |  9579 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  9580 |  |
|        - |  9581 | `/*` |
|        - |  9582 | ` * Section:` |
|        - |  9583 | ` *  Function handling functions.` |
|        - |  9584 | ` * Status:` |
|        - |  9585 | ` *    Stable.` |
|        - |  9586 | ` */` |
|        - |  9587 | `/*` |
|        - |  9588 | ` * int func_num_args(void)` |
|        - |  9589 | ` *   Returns the number of arguments passed to the function.` |
|        - |  9590 | ` * Parameters` |
|        - |  9591 | ` *   None.` |
|        - |  9592 | ` * Return` |
|        - |  9593 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  9594 | ` *  or -1 if called from the globe scope.` |
|        - |  9595 | ` */` |
|      944 |  9596 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9597 |  |
|        - |  9598 | `	VmFrame *pFrame;` |
|        - |  9599 | `	ph7_vm *pVm;` |
|        - |  9600 | `	/* Point to the target VM */` |
|      946 |  9601 | `	pVm = pCtx->pVm;` |
|        - |  9602 | `	/* Current frame */` |
|      946 |  9603 | `	pFrame = pVm->pFrame;` |
|      946 |  9604 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  9605 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  9606 | `		SXUNUSED(nArg);` |
|      ! 0 |  9607 | `		SXUNUSED(apArg);` |
|        - |  9608 | `		/* Global frame,return -1 */` |
|      ! 0 |  9609 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  9610 | `		return SXRET_OK;` |
|        - |  9611 | `	}` |
|        - |  9612 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  9613 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  9614 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  9615 | `	return SXRET_OK;` |
|      474 |  9616 |  |
|        - |  9617 | `/*` |
|        - |  9618 | ` * value func_get_arg(int $arg_num)` |
|        - |  9619 | ` *   Return an item from the argument list.` |
|        - |  9620 | ` * Parameters` |
|        - |  9621 | ` *  Argument number(index start from zero).` |
|        - |  9622 | ` * Return` |
|        - |  9623 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  9624 | ` */` |
|       22 |  9625 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9626 |  |
|       24 |  9627 | `	ph7_value *pObj = 0;` |
|       24 |  9628 | `	VmSlot *pSlot = 0;` |
|        - |  9629 | `	VmFrame *pFrame;` |
|        - |  9630 | `	ph7_vm *pVm;` |
|        - |  9631 | `	/* Point to the target VM */` |
|       24 |  9632 | `	pVm = pCtx->pVm;` |
|        - |  9633 | `	/* Current frame */` |
|       24 |  9634 | `	pFrame = pVm->pFrame;` |
|       24 |  9635 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  9636 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  9637 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  9638 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  9639 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9640 | `		return SXRET_OK;` |
|        - |  9641 | `	}` |
|        - |  9642 | `	/* Extract the desired index */` |
|       21 |  9643 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  9644 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  9645 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  9646 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9647 | `		return SXRET_OK;` |
|        - |  9648 | `	}` |
|        - |  9649 | `	/* Extract the desired argument */` |
|       21 |  9650 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  9651 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  9652 | `			/* Return the desired argument */` |
|       21 |  9653 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  9654 | `		}else{` |
|        - |  9655 | `			/* No such argument,return false */` |
|      ! 0 |  9656 | `			ph7_result_bool(pCtx,0);` |
|        - |  9657 | `		}` |
|       11 |  9658 | `	}else{` |
|        - |  9659 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  9660 | `		ph7_result_bool(pCtx,0);` |
|        - |  9661 | `	}` |
|       21 |  9662 | `	return SXRET_OK;` |
|       13 |  9663 |  |
|        - |  9664 | `/*` |
|        - |  9665 | ` * array func_get_args_byref(void)` |
|        - |  9666 | ` *   Returns an array comprising a function's argument list.` |
|        - |  9667 | ` * Parameters` |
|        - |  9668 | ` *  None.` |
|        - |  9669 | ` * Return` |
|        - |  9670 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  9671 | ` *  member of the current user-defined function's argument list.` |
|        - |  9672 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9673 | ` * NOTE:` |
|        - |  9674 | ` *  Arguments are returned to the array by reference.` |
|        - |  9675 | ` */` |
|        2 |  9676 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9677 |  |
|        - |  9678 | `	ph7_value *pArray;` |
|        - |  9679 | `	VmFrame *pFrame;` |
|        - |  9680 | `	VmSlot *aSlot;` |
|        - |  9681 | `	sxu32 n;` |
|        - |  9682 | `	/* Point to the current frame */` |
|        3 |  9683 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  9684 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  9685 | `	if( pFrame->pParent == 0 ){` |
|        - |  9686 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9687 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9688 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9689 | `		return SXRET_OK;` |
|        - |  9690 | `	}` |
|        - |  9691 | `	/* Create a new array */` |
|        3 |  9692 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9693 | `	if( pArray == 0 ){` |
|      ! 0 |  9694 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9695 | `		SXUNUSED(apArg);` |
|      ! 0 |  9696 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9697 | `		return SXRET_OK;` |
|        - |  9698 | `	}` |
|        - |  9699 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  9700 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  9701 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  9702 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  9703 | `	}` |
|        - |  9704 | `	/* Return the freshly created array */` |
|        3 |  9705 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9706 | `	return SXRET_OK;` |
|        2 |  9707 |  |
|        - |  9708 | `/*` |
|        - |  9709 | ` * array func_get_args(void)` |
|        - |  9710 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  9711 | ` * Parameters` |
|        - |  9712 | ` *  None.` |
|        - |  9713 | ` * Return` |
|        - |  9714 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  9715 | ` *  member of the current user-defined function's argument list.` |
|        - |  9716 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9717 | ` */` |
|       88 |  9718 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9719 |  |
|       90 |  9720 | `	ph7_value *pObj = 0;` |
|        - |  9721 | `	ph7_value *pArray;` |
|        - |  9722 | `	VmFrame *pFrame;` |
|        - |  9723 | `	VmSlot *aSlot;` |
|        - |  9724 | `	sxu32 n;` |
|        - |  9725 | `	/* Point to the current frame */` |
|       90 |  9726 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  9727 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  9728 | `	if( pFrame->pParent == 0 ){` |
|        - |  9729 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9730 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9731 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9732 | `		return SXRET_OK;` |
|        - |  9733 | `	}` |
|        - |  9734 | `	/* Create a new array */` |
|       90 |  9735 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  9736 | `	if( pArray == 0 ){` |
|      ! 0 |  9737 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9738 | `		SXUNUSED(apArg);` |
|      ! 0 |  9739 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9740 | `		return SXRET_OK;` |
|        - |  9741 | `	}` |
|        - |  9742 | `	/* Start filling the array with the given arguments */` |
|       90 |  9743 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  9744 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  9745 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  9746 | `		if( pObj ){` |
|      134 |  9747 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  9748 | `		}` |
|       68 |  9749 | `	}` |
|        - |  9750 | `	/* Return the freshly created array */` |
|       90 |  9751 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  9752 | `	return SXRET_OK;` |
|       46 |  9753 |  |
|        - |  9754 | `/*` |
|        - |  9755 | ` * bool function_exists(string $name)` |
|        - |  9756 | ` *  Return TRUE if the given function has been defined.` |
|        - |  9757 | ` * Parameters` |
|        - |  9758 | ` *  The name of the desired function.` |
|        - |  9759 | ` * Return` |
|        - |  9760 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  9761 | ` */` |
|     1680 |  9762 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9763 |  |
|        - |  9764 | `	const char *zName;` |
|        - |  9765 | `	ph7_vm *pVm;` |
|        - |  9766 | `	int nLen;` |
|        - |  9767 | `	int res;` |
|     1682 |  9768 | `	if( nArg < 1 ){` |
|        - |  9769 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  9770 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9771 | `		return SXRET_OK;` |
|        - |  9772 | `	}` |
|        - |  9773 | `	/* Point to the target VM */` |
|     1682 |  9774 | `	pVm = pCtx->pVm;` |
|        - |  9775 | `	/* Extract the function name */` |
|     1682 |  9776 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9777 | `	/* Assume the function is not defined */` |
|     1682 |  9778 | `	res = 0;` |
|        - |  9779 | `	/* Perform the lookup */` |
|     2520 |  9780 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 |  9781 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9782 | `			/* Function is defined */` |
|      206 |  9783 | `			res = 1;` |
|      102 |  9784 | `	}` |
|     1682 |  9785 | `	ph7_result_bool(pCtx,res);` |
|     1682 |  9786 | `	return SXRET_OK;` |
|      842 |  9787 |  |
|        - |  9788 | `/*` |
|        - |  9789 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9790 | ` * [i.e: Whether it is callable or not].` |
|        - |  9791 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  9792 | ` */` |
|    19638 |  9793 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  9794 |  |
|    19640 |  9795 | `	int res = 0;` |
|    19640 |  9796 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9797 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  9798 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  9799 | `		ph7_class_method *pMethod;` |
|      ! 0 |  9800 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  9801 | `		if( pMethod && CallInvoke ){` |
|        - |  9802 | `			ph7_value sResult;` |
|        - |  9803 | `			sxi32 rc;` |
|        - |  9804 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  9805 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  9806 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  9807 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  9808 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  9809 | `			}` |
|      ! 0 |  9810 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9811 | `		}` |
|    19640 |  9812 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  9813 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  9814 | `		if( pMap->nEntry == 2 ){` |
|        - |  9815 | `			ph7_class *pClass;` |
|        - |  9816 | `			ph7_value *pV;` |
|        - |  9817 | `			/* Extract the target class */` |
|       12 |  9818 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  9819 | `			if( pV ){` |
|       12 |  9820 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  9821 | `				if( pClass ){` |
|        - |  9822 | `					ph7_class_method *pMethod;` |
|        - |  9823 | `					/* Extract the target method */` |
|       10 |  9824 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  9825 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  9826 | `						/* Perform the lookup */` |
|       10 |  9827 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  9828 | `						if( pMethod ){` |
|        - |  9829 | `							/* Method is callable */` |
|        5 |  9830 | `							res = 1;` |
|        2 |  9831 | `						}` |
|        4 |  9832 | `					}` |
|        4 |  9833 | `				}` |
|        5 |  9834 | `			}` |
|        7 |  9835 | `		}` |
|    19627 |  9836 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  9837 | `		const char *zName;` |
|        - |  9838 | `		int nLen;` |
|        - |  9839 | `		/* Extract the name */` |
|     5308 |  9840 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  9841 | `		/* Perform the lookup */` |
|     5323 |  9842 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  9843 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9844 | `				/* Function is callable */` |
|     5290 |  9845 | `				res = 1;` |
|     2644 |  9846 | `		}` |
|     2653 |  9847 | `	}` |
|    19640 |  9848 | `	return res;` |
|        2 |  9849 |  |
|        - |  9850 | `/*` |
|        - |  9851 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  9852 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9853 | ` * Parameters` |
|        - |  9854 | ` * $name` |
|        - |  9855 | ` *    The callback function to check` |
|        - |  9856 | ` * $syntax_only` |
|        - |  9857 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  9858 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  9859 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  9860 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  9861 | ` *    a string.` |
|        - |  9862 | ` * Return` |
|        - |  9863 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  9864 | ` */` |
|       14 |  9865 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9866 |  |
|        - |  9867 | `	ph7_vm *pVm;` |
|        - |  9868 | `	int res;` |
|       15 |  9869 | `	if( nArg < 1 ){` |
|        - |  9870 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9871 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9872 | `		return SXRET_OK;` |
|        - |  9873 | `	}` |
|        - |  9874 | `	/* Point to the target VM */` |
|       15 |  9875 | `	pVm = pCtx->pVm;` |
|        - |  9876 | `	/* Perform the requested operation */` |
|       15 |  9877 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  9878 | `	ph7_result_bool(pCtx,res);` |
|       15 |  9879 | `	return SXRET_OK;` |
|        8 |  9880 |  |
|        - |  9881 | `/*` |
|        - |  9882 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  9883 | ` * defined below.` |
|        - |  9884 | ` */` |
|     1200 |  9885 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9886 |  |
|     1201 |  9887 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9888 | `	ph7_value sName;` |
|        - |  9889 | `	sxi32 rc;` |
|        - |  9890 | `	/* Prepare the function name for insertion */` |
|     1201 |  9891 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  9892 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9893 | `	/* Perform the insertion */` |
|     1201 |  9894 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  9895 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  9896 | `	return rc;` |
|        1 |  9897 |  |
|        - |  9898 | `/*` |
|        - |  9899 | ` * array get_defined_functions(void)` |
|        - |  9900 | ` *  Returns an array of all defined functions.` |
|        - |  9901 | ` * Parameter` |
|        - |  9902 | ` *  None.` |
|        - |  9903 | ` * Return` |
|        - |  9904 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  9905 | ` *  both built-in (internal) and user-defined.` |
|        - |  9906 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  9907 | ` *  defined ones using $arr["user"].` |
|        - |  9908 | ` * Note:` |
|        - |  9909 | ` *  NULL is returned on failure.` |
|        - |  9910 | ` */` |
|        2 |  9911 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9912 |  |
|        - |  9913 | `	ph7_value *pArray,*pEntry;` |
|        - |  9914 | `	/* NOTE:` |
|        - |  9915 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  9916 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  9917 | `	 */` |
|        3 |  9918 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9919 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9920 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9921 | `		SXUNUSED(apArg);` |
|        - |  9922 | `		/* Return NULL */` |
|      ! 0 |  9923 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9924 | `		return SXRET_OK;` |
|        - |  9925 | `	}` |
|        3 |  9926 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9927 | `	if( pEntry == 0 ){` |
|        - |  9928 | `		/* Return NULL */` |
|      ! 0 |  9929 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9930 | `		return SXRET_OK;` |
|        - |  9931 | `	}` |
|        - |  9932 | `	/* Fill with the appropriate information */` |
|        3 |  9933 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  9934 | `	/* Create the 'internal' index */` |
|        3 |  9935 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  9936 | `	/* Create the user-func array */` |
|        3 |  9937 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9938 | `	if( pEntry == 0 ){` |
|        - |  9939 | `		/* Return NULL */` |
|      ! 0 |  9940 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9941 | `		return SXRET_OK;` |
|        - |  9942 | `	}` |
|        - |  9943 | `	/* Fill with the appropriate information */` |
|        3 |  9944 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  9945 | `	/* Create the 'user' index */` |
|        3 |  9946 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  9947 | `	/* Return the multi-dimensional array */` |
|        3 |  9948 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9949 | `	return SXRET_OK;` |
|        2 |  9950 |  |
|        - |  9951 | `/*` |
|        - |  9952 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  9953 | ` *  Register a function for execution on shutdown.` |
|        - |  9954 | ` * Note` |
|        - |  9955 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  9956 | ` *  be called in the same order as they were registered.` |
|        - |  9957 | ` * Parameters` |
|        - |  9958 | ` *  $callback` |
|        - |  9959 | ` *   The shutdown callback to register.` |
|        - |  9960 | ` * $param` |
|        - |  9961 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  9962 | ` * Return` |
|        - |  9963 | ` *  Nothing.` |
|        - |  9964 | ` */` |
|        2 |  9965 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9966 |  |
|        - |  9967 | `	VmShutdownCB sEntry;` |
|        - |  9968 | `	int i,j;` |
|        3 |  9969 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9970 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  9971 | `		return PH7_OK;` |
|        - |  9972 | `	}` |
|        - |  9973 | `	/* Zero the Entry */` |
|        3 |  9974 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  9975 | `	/* Initialize fields */` |
|        3 |  9976 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  9977 | `	/* Save the callback name for later invocation name */` |
|        3 |  9978 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  9979 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  9980 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  9981 | `	}` |
|        - |  9982 | `	/* Copy arguments */` |
|        3 |  9983 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  9984 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  9985 | `			/* Limit reached */` |
|      ! 0 |  9986 | `			break;` |
|        - |  9987 | `		}` |
|      ! 0 |  9988 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  9989 | `	}` |
|        3 |  9990 | `	sEntry.nArg = j;` |
|        - |  9991 | `	/* Install the callback */` |
|        3 |  9992 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  9993 | `	return PH7_OK;` |
|        2 |  9994 |  |
|        - |  9995 | `/*` |
|        - |  9996 | ` * Section:` |
|        - |  9997 | ` *  Class handling functions.` |
|        - |  9998 | ` * Status:` |
|        - |  9999 | ` *    Stable.` |
|        - | 10000 | ` */` |
|        - | 10001 | `/*` |
|        - | 10002 | ` * Extract the top active class. NULL is returned` |
|        - | 10003 | ` * if the class stack is empty.` |
|        - | 10004 | ` */` |
|      672 | 10005 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10006 |  |
|      674 | 10007 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10008 | `	ph7_class **apClass;` |
|      674 | 10009 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10010 | `		/* Empty stack,return NULL */` |
|       15 | 10011 | `		return 0;` |
|        - | 10012 | `	}` |
|        - | 10013 | `	/* Peek the last entry */` |
|      660 | 10014 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      660 | 10015 | `	return apClass[pSet->nUsed - 1];` |
|      338 | 10016 |  |
|        - | 10017 | `/*` |
|        - | 10018 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10019 | ` *   Get the class that declared the currently executing method.` |
|        - | 10020 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10021 | ` *` |
|        - | 10022 | ` * Parameters` |
|        - | 10023 | ` *   pVm: Target VM` |
|        - | 10024 | ` *` |
|        - | 10025 | ` * Return` |
|        - | 10026 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10027 | ` *   - Not executing within a class method` |
|        - | 10028 | ` *` |
|        - | 10029 | ` * Note` |
|        - | 10030 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10031 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10032 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10033 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10034 | ` *   declaring class.` |
|        - | 10035 | ` */` |
|       96 | 10036 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10037 |  |
|       98 | 10038 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10039 | `	ph7_vm_func *pVmFunc;` |
|        - | 10040 |  |
|        - | 10041 | `	/* Skip exception frames to find the actual method frame */` |
|       98 | 10042 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10043 |  |
|        - | 10044 | `	/* Check if we're in a method context */` |
|       98 | 10045 | `	if( pFrame->pParent ){` |
|       94 | 10046 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       94 | 10047 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10048 | `			/* Return the declaring class */` |
|       94 | 10049 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10050 | `		}` |
|      ! 0 | 10051 | `	}` |
|        - | 10052 |  |
|        5 | 10053 | `	return 0;` |
|       50 | 10054 |  |
|        - | 10055 |  |
|        - | 10056 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10057 | `/*` |
|        - | 10058 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10059 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10060 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10061 | ` * return value indicates failure.` |
|        - | 10062 | ` */` |
|        - | 10063 | `/*` |
|        - | 10064 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10065 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10066 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10067 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10068 | ` */` |
|     1668 | 10069 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10070 | `	ph7_vm *pVm,` |
|        - | 10071 | `	ph7_class_instance *pThis,` |
|        - | 10072 | `	ph7_class_method *pMethod,` |
|        - | 10073 | `	ph7_value *pResult,` |
|        - | 10074 | `	int nArg,` |
|        - | 10075 | `	ph7_value **apArg,` |
|        - | 10076 | `	VmCallArgMap *pMap` |
|        - | 10077 | `	)` |
|        2 | 10078 |  |
|        - | 10079 | `	ph7_value *aStack;` |
|        - | 10080 | `	VmInstr aInstr[2];` |
|        - | 10081 | `	int iCursor;` |
|        - | 10082 | `	int i;` |
|     1670 | 10083 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     1670 | 10084 | `	if( aStack == 0 ){` |
|      ! 0 | 10085 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10086 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10087 | `		return SXERR_MEM;` |
|        - | 10088 | `	}` |
|     2418 | 10089 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      750 | 10090 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|      750 | 10091 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      376 | 10092 | `	}` |
|     1670 | 10093 | `	iCursor = nArg + 1;` |
|     1670 | 10094 | `	if( pThis ){` |
|     1664 | 10095 | `		pThis->iRef++;` |
|     1664 | 10096 | `		aStack[i].x.pOther = pThis;` |
|     1664 | 10097 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      831 | 10098 | `	}` |
|     1670 | 10099 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1670 | 10100 | `	i++;` |
|     1670 | 10101 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1670 | 10102 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1670 | 10103 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1670 | 10104 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1670 | 10105 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1670 | 10106 | `	aInstr[0].iP1 = nArg;` |
|     1670 | 10107 | `	aInstr[0].iP2 = 0;` |
|     1670 | 10108 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     1670 | 10109 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1670 | 10110 | `	aInstr[1].iP1 = 1;` |
|     1670 | 10111 | `	aInstr[1].iP2 = 0;` |
|     1670 | 10112 | `	aInstr[1].p3  = 0;` |
|     1670 | 10113 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|     1670 | 10114 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1670 | 10115 | `	return PH7_OK;` |
|      836 | 10116 |  |
|     1508 | 10117 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10118 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10119 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10120 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10121 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10122 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10123 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10124 | `	)` |
|        2 | 10125 |  |
|     1510 | 10126 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10127 |  |
|        - | 10128 | `/*` |
|        - | 10129 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10130 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10131 | ` * in the apArg[] array.` |
|        - | 10132 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10133 | ` * return value indicates failure.` |
|        - | 10134 | ` */` |
|      966 | 10135 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10136 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10137 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10138 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10139 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10140 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10141 | `	)` |
|        2 | 10142 |  |
|        - | 10143 | `	ph7_value *aStack;` |
|        - | 10144 | `	VmInstr aInstr[2];` |
|        - | 10145 | `	int i;` |
|      968 | 10146 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10147 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 | 10148 | `		if( pResult ){` |
|        - | 10149 | `			/* Assume a null return value */` |
|      ! 0 | 10150 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10151 | `		}` |
|      479 | 10152 | `		return SXERR_INVALID;` |
|        - | 10153 | `	}` |
|      490 | 10154 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10155 | `		/* Class method */` |
|       11 | 10156 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10157 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10158 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10159 | `		ph7_class *pClass = 0;` |
|        - | 10160 | `		ph7_value *pValue;` |
|        - | 10161 | `		sxi32 rc;` |
|       11 | 10162 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10163 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10164 | `			if( pResult ){` |
|        - | 10165 | `				/* Assume a null return value */` |
|      ! 0 | 10166 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10167 | `			}` |
|      ! 0 | 10168 | `			return SXRET_OK;` |
|        - | 10169 | `		}` |
|        - | 10170 | `		/* Extract the class name or an instance of it */` |
|       11 | 10171 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10172 | `		if( pValue ){` |
|       11 | 10173 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10174 | `		}` |
|       11 | 10175 | `		if( pClass == 0 ){` |
|        - | 10176 | `			/* No such class,return NULL */` |
|      ! 0 | 10177 | `			if( pResult ){` |
|      ! 0 | 10178 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10179 | `			}` |
|      ! 0 | 10180 | `			return SXRET_OK;` |
|        - | 10181 | `		}` |
|       11 | 10182 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10183 | `			/* Point to the class instance */` |
|        5 | 10184 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10185 | `		}` |
|        - | 10186 | `		/* Try to extract the method */` |
|       11 | 10187 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10188 | `		if( pValue ){` |
|       11 | 10189 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10190 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10191 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10192 | `			}` |
|        5 | 10193 | `		}` |
|       11 | 10194 | `		if( pMethod == 0 ){` |
|        - | 10195 | `			/* No such method,return NULL */` |
|      ! 0 | 10196 | `			if( pResult ){` |
|      ! 0 | 10197 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10198 | `			}` |
|      ! 0 | 10199 | `			return SXRET_OK;` |
|        - | 10200 | `		}` |
|        - | 10201 | `		/* Call the class method */` |
|       11 | 10202 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10203 | `		return rc;` |
|        - | 10204 | `	}` |
|        - | 10205 | `	/* Create a new operand stack */` |
|      480 | 10206 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      480 | 10207 | `	if( aStack == 0 ){` |
|      ! 0 | 10208 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10209 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10210 | `		if( pResult ){` |
|        - | 10211 | `			/* Assume a null return value */` |
|      ! 0 | 10212 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10213 | `		}` |
|      ! 0 | 10214 | `		return SXERR_MEM;` |
|        - | 10215 | `	}` |
|        - | 10216 | `	/* Fill the operand stack with the given arguments */` |
|     1534 | 10217 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1056 | 10218 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10219 | `		/*` |
|        - | 10220 | `		 * Symisc eXtension:` |
|        - | 10221 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10222 | `		 */` |
|     1056 | 10223 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      529 | 10224 | `	}` |
|        - | 10225 | `	/* Push the function name */` |
|      480 | 10226 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      480 | 10227 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10228 | `	/* Emit the CALL istruction */` |
|      480 | 10229 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      480 | 10230 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      480 | 10231 | `	aInstr[0].iP2 = 0;` |
|      480 | 10232 | `	aInstr[0].p3  = 0;` |
|        - | 10233 | `	/* Emit the DONE instruction */` |
|      480 | 10234 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      480 | 10235 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      480 | 10236 | `	aInstr[1].iP2 = 0;` |
|      480 | 10237 | `	aInstr[1].p3  = 0;` |
|        - | 10238 | `	/* Execute the function body (if available) */` |
|      480 | 10239 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - | 10240 | `	/* Clean up the mess left behind */` |
|      480 | 10241 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      480 | 10242 | `	return PH7_OK;` |
|      485 | 10243 |  |
|        - | 10244 | `/*` |
|        - | 10245 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 10246 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 10247 | ` * parameter.` |
|        - | 10248 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10249 | ` * return value indicates failure.` |
|        - | 10250 | ` */` |
|      236 | 10251 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 10252 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10253 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10254 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 10255 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 10256 | `	)` |
|        1 | 10257 |  |
|        - | 10258 | `	ph7_value *pArg;` |
|        - | 10259 | `	SySet aArg;` |
|        - | 10260 | `	va_list ap;` |
|        - | 10261 | `	sxi32 rc;` |
|      237 | 10262 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 10263 | `	/* Copy arguments one after one */` |
|      237 | 10264 | `	va_start(ap,pResult);` |
|      393 | 10265 | `	for(;;){` |
|      787 | 10266 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 10267 | `		if( pArg == 0 ){` |
|      237 | 10268 | `			break;` |
|        - | 10269 | `		}` |
|      551 | 10270 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 10271 | `	}` |
|        - | 10272 | `	/* Call the core routine */` |
|      237 | 10273 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 10274 | `	/* Cleanup */` |
|      237 | 10275 | `	SySetRelease(&aArg);` |
|      237 | 10276 | `	return rc;` |
|        1 | 10277 |  |
|        - | 10278 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 10279 | `/*` |
|        - | 10280 | ` * bool defined(string $name)` |
|        - | 10281 | ` *  Checks whether a given named constant exists.` |
|        - | 10282 | ` * Parameter:` |
|        - | 10283 | ` *  Name of the desired constant.` |
|        - | 10284 | ` * Return` |
|        - | 10285 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 10286 | ` */` |
|       14 | 10287 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10288 |  |
|        - | 10289 | `	const char *zName;` |
|       16 | 10290 | `	int nLen = 0;` |
|       16 | 10291 | `	int res = 0;` |
|       16 | 10292 | `	if( nArg < 1 ){` |
|        - | 10293 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 10294 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 10295 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10296 | `		return SXRET_OK;` |
|        - | 10297 | `	}` |
|        - | 10298 | `	/* Extract constant name */` |
|       16 | 10299 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10300 | `	/* Perform the lookup */` |
|       16 | 10301 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10302 | `		/* Already defined */` |
|       10 | 10303 | `		res = 1;` |
|        4 | 10304 | `	}` |
|       16 | 10305 | `	ph7_result_bool(pCtx,res);` |
|       16 | 10306 | `	return SXRET_OK;` |
|        9 | 10307 |  |
|        - | 10308 | `/*` |
|        - | 10309 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 10310 | ` * below.` |
|        - | 10311 | ` */` |
|       10 | 10312 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 10313 |  |
|       12 | 10314 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 10315 | `	/* Expand constant value */` |
|       12 | 10316 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 10317 |  |
|        - | 10318 | `/*` |
|        - | 10319 | ` * bool define(string $constant_name,expression value)` |
|        - | 10320 | ` *  Defines a named constant at runtime.` |
|        - | 10321 | ` * Parameter:` |
|        - | 10322 | ` *  $constant_name` |
|        - | 10323 | ` *   The name of the constant` |
|        - | 10324 | ` *  $value` |
|        - | 10325 | ` *   Constant value` |
|        - | 10326 | ` * Return:` |
|        - | 10327 | ` *   TRUE on success,FALSE on failure.` |
|        - | 10328 | ` */` |
|       12 | 10329 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10330 |  |
|        - | 10331 | `	const char *zName;  /* Constant name */` |
|        - | 10332 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 10333 | `	int nLen = 0;       /* Name length */` |
|        - | 10334 | `	sxi32 rc;` |
|       14 | 10335 | `	if( nArg < 2 ){` |
|        - | 10336 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 10337 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 10338 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10339 | `		return SXRET_OK;` |
|        - | 10340 | `	}` |
|       14 | 10341 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 10342 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 10343 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10344 | `		return SXRET_OK;` |
|        - | 10345 | `	}` |
|        - | 10346 | `	/* Extract constant name */` |
|       14 | 10347 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 10348 | `	if( nLen < 1 ){` |
|      ! 0 | 10349 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 10350 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10351 | `		return SXRET_OK;` |
|        - | 10352 | `	}` |
|        - | 10353 | `	/* Duplicate constant value */` |
|       14 | 10354 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 10355 | `	if( pValue == 0 ){` |
|      ! 0 | 10356 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10357 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10358 | `		return SXRET_OK;` |
|        - | 10359 | `	}` |
|        - | 10360 | `	/* Initialize the memory object */` |
|       14 | 10361 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 10362 | `	/* Register the constant */` |
|       14 | 10363 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 10364 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10365 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 10366 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10367 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10368 | `		return SXRET_OK;` |
|        - | 10369 | `	}` |
|        - | 10370 | `	/* Duplicate constant value */` |
|       14 | 10371 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 10372 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 10373 | `		/* Lower case the constant name */` |
|      ! 0 | 10374 | `		char *zCur = (char *)zName;` |
|      ! 0 | 10375 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 10376 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 10377 | `				/* UTF-8 stream */` |
|      ! 0 | 10378 | `				zCur++;` |
|      ! 0 | 10379 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 10380 | `					zCur++;` |
|      ! 0 | 10381 | `				}` |
|      ! 0 | 10382 | `				continue;` |
|        - | 10383 | `			}` |
|      ! 0 | 10384 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 10385 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 10386 | `				zCur[0] = (char)c;` |
|      ! 0 | 10387 | `			}` |
|      ! 0 | 10388 | `			zCur++;` |
|      ! 0 | 10389 | `		}` |
|        - | 10390 | `		/* Finally,register the constant */` |
|      ! 0 | 10391 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 10392 | `	}` |
|        - | 10393 | `	/* All done,return TRUE */` |
|       14 | 10394 | `	ph7_result_bool(pCtx,1);` |
|       14 | 10395 | `	return SXRET_OK;` |
|        8 | 10396 |  |
|        - | 10397 | `/*` |
|        - | 10398 | ` * value constant(string $name)` |
|        - | 10399 | ` *  Returns the value of a constant` |
|        - | 10400 | ` * Parameter` |
|        - | 10401 | ` *  $name` |
|        - | 10402 | ` *    Name of the constant.` |
|        - | 10403 | ` * Return` |
|        - | 10404 | ` *  Constant value or NULL if not defined.` |
|        - | 10405 | ` */` |
|        8 | 10406 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10407 |  |
|        - | 10408 | `	SyHashEntry *pEntry;` |
|        - | 10409 | `	ph7_constant *pCons;` |
|        - | 10410 | `	const char *zName; /* Constant name */` |
|        - | 10411 | `	ph7_value sVal;    /* Constant value */` |
|        - | 10412 | `	int nLen;` |
|       10 | 10413 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10414 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 10415 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 10416 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10417 | `		return SXRET_OK;` |
|        - | 10418 | `	}` |
|        - | 10419 | `	/* Extract the constant name */` |
|       10 | 10420 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10421 | `	/* Perform the query */` |
|       10 | 10422 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 10423 | `	if( pEntry == 0 ){` |
|        3 | 10424 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 10425 | `		ph7_result_null(pCtx);` |
|        3 | 10426 | `		return SXRET_OK;` |
|        - | 10427 | `	}` |
|        8 | 10428 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 10429 | `	/* Point to the structure that describe the constant */` |
|        8 | 10430 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 10431 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 10432 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 10433 | `	/* Return that value */` |
|        8 | 10434 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 10435 | `	/* Cleanup */` |
|        8 | 10436 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 10437 | `	return SXRET_OK;` |
|        6 | 10438 |  |
|        - | 10439 | `/*` |
|        - | 10440 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 10441 | ` * defined below.` |
|        - | 10442 | ` */` |
|      452 | 10443 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10444 |  |
|      453 | 10445 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10446 | `	ph7_value sName;` |
|        - | 10447 | `	sxi32 rc;` |
|        - | 10448 | `	/* Prepare the constant name for insertion */` |
|      453 | 10449 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 10450 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10451 | `	/* Perform the insertion */` |
|      453 | 10452 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 10453 | `	PH7_MemObjRelease(&sName);` |
|      453 | 10454 | `	return rc;` |
|        1 | 10455 |  |
|        - | 10456 | `/*` |
|        - | 10457 | ` * array get_defined_constants(void)` |
|        - | 10458 | ` *  Returns an associative array with the names of all defined` |
|        - | 10459 | ` *  constants.` |
|        - | 10460 | ` * Parameters` |
|        - | 10461 | ` *  NONE.` |
|        - | 10462 | ` * Returns` |
|        - | 10463 | ` *  Returns the names of all the constants currently defined.` |
|        - | 10464 | ` */` |
|        2 | 10465 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10466 |  |
|        - | 10467 | `	ph7_value *pArray;` |
|        - | 10468 | `	/* Create the array first*/` |
|        3 | 10469 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10470 | `	if( pArray == 0 ){` |
|      ! 0 | 10471 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10472 | `		SXUNUSED(apArg);` |
|        - | 10473 | `		/* Return NULL */` |
|      ! 0 | 10474 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10475 | `		return SXRET_OK;` |
|        - | 10476 | `	}` |
|        - | 10477 | `	/* Fill the array with the defined constants */` |
|        3 | 10478 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 10479 | `	/* Return the created array */` |
|        3 | 10480 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10481 | `	return SXRET_OK;` |
|        2 | 10482 |  |
|        - | 10483 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 10484 | `/*` |
|        - | 10485 | ` * Section:` |
|        - | 10486 | ` *  Random numbers/string generators.` |
|        - | 10487 | ` * Status:` |
|        - | 10488 | ` *    Stable.` |
|        - | 10489 | ` */` |
|        - | 10490 | `/*` |
|        - | 10491 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 10492 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 10493 | ` * used by te SQLite3 library.` |
|        - | 10494 | ` */` |
|     2625 | 10495 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 10496 |  |
|        - | 10497 | `	sxu32 iNum;` |
|     2627 | 10498 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2627 | 10499 | `	return iNum;` |
|        2 | 10500 |  |
|        - | 10501 | `/*` |
|        - | 10502 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 10503 | ` * Note that the generated string is NOT null terminated.` |
|        - | 10504 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 10505 | ` * by te SQLite3 library.` |
|        - | 10506 | ` */` |
|   136874 | 10507 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 10508 |  |
|        - | 10509 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 10510 | `	int i;` |
|        - | 10511 | `	/* Generate a binary string first */` |
|   136876 | 10512 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 10513 | `	/* Turn the binary string into english based alphabet */` |
|  1505784 | 10514 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1368910 | 10515 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   684456 | 10516 | `	 }` |
|   136876 | 10517 |  |
|        - | 10518 | `/*` |
|        - | 10519 | ` * int rand()` |
|        - | 10520 | ` * int mt_rand()` |
|        - | 10521 | ` * int rand(int $min,int $max)` |
|        - | 10522 | ` * int mt_rand(int $min,int $max)` |
|        - | 10523 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 10524 | ` * Parameter` |
|        - | 10525 | ` *  $min` |
|        - | 10526 | ` *    The lowest value to return (default: 0)` |
|        - | 10527 | ` *  $max` |
|        - | 10528 | ` *   The highest value to return (default: getrandmax())` |
|        - | 10529 | ` * Return` |
|        - | 10530 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 10531 | ` * Note:` |
|        - | 10532 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10533 | ` *  by te SQLite3 library.` |
|        - | 10534 | ` */` |
|       20 | 10535 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10536 |  |
|        - | 10537 | `	sxu32 iNum;` |
|        - | 10538 | `	/* Generate the random number */` |
|       21 | 10539 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 10540 | `	if( nArg > 1 ){` |
|        - | 10541 | `		sxu32 iMin,iMax;` |
|        3 | 10542 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 10543 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 10544 | `		if( iMin < iMax ){` |
|        3 | 10545 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 10546 | `			if( iDiv > 0 ){` |
|        3 | 10547 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 10548 | `			}` |
|        1 | 10549 | `		}else if(iMax > 0 ){` |
|      ! 0 | 10550 | `			iNum %= iMax;` |
|      ! 0 | 10551 | `		}` |
|        1 | 10552 | `	}` |
|        - | 10553 | `	/* Return the number */` |
|       21 | 10554 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 10555 | `	return SXRET_OK;` |
|        1 | 10556 |  |
|        - | 10557 | `/*` |
|        - | 10558 | ` * int getrandmax(void)` |
|        - | 10559 | ` * int mt_getrandmax(void)` |
|        - | 10560 | ` * int rc4_getrandmax(void)` |
|        - | 10561 | ` *   Show largest possible random value` |
|        - | 10562 | ` * Return` |
|        - | 10563 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 10564 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 10565 | ` * Note:` |
|        - | 10566 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10567 | ` *  by te SQLite3 library.` |
|        - | 10568 | ` */` |
|        4 | 10569 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10570 |  |
|        2 | 10571 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 10572 | `	SXUNUSED(apArg);` |
|        5 | 10573 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 10574 | `	return SXRET_OK;` |
|        1 | 10575 |  |
|        - | 10576 | `/*` |
|        - | 10577 | ` * string rand_str()` |
|        - | 10578 | ` * string rand_str(int $len)` |
|        - | 10579 | ` *  Generate a random string (English alphabet).` |
|        - | 10580 | ` * Parameter` |
|        - | 10581 | ` *  $len` |
|        - | 10582 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 10583 | ` * Return` |
|        - | 10584 | ` *   A pseudo random string.` |
|        - | 10585 | ` * Note:` |
|        - | 10586 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10587 | ` *  by te SQLite3 library.` |
|        - | 10588 | ` *  This function is a symisc extension.` |
|        - | 10589 | ` */` |
|      120 | 10590 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10591 |  |
|        - | 10592 | `	char zString[1024];` |
|      122 | 10593 | `	int iLen = 0x10;` |
|      122 | 10594 | `	if( nArg > 0 ){` |
|        - | 10595 | `		/* Get the desired length */` |
|      122 | 10596 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 10597 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 10598 | `			/* Default length */` |
|        3 | 10599 | `			iLen = 0x10;` |
|        1 | 10600 | `		}` |
|       60 | 10601 | `	}` |
|        - | 10602 | `	/* Generate the random string */` |
|      122 | 10603 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 10604 | `	/* Return the generated string */` |
|      122 | 10605 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 10606 | `	return SXRET_OK;` |
|        2 | 10607 |  |
|        - | 10608 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10609 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10610 | `/* Unique ID private data */` |
|        - | 10611 | `struct unique_id_data` |
|        - | 10612 |  |
|        - | 10613 | `	ph7_context *pCtx; /* Call context */` |
|        - | 10614 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 10615 | `};` |
|        - | 10616 | `/*` |
|        - | 10617 | ` * Binary to hex consumer callback.` |
|        - | 10618 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 10619 | ` * defined below.` |
|        - | 10620 | ` */` |
|      192 | 10621 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 10622 |  |
|      193 | 10623 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 10624 | `	sxu32 nBuflen;` |
|        - | 10625 | `	/* Extract result buffer length */` |
|      193 | 10626 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 10627 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 10628 | `			/*` |
|        - | 10629 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 10630 | `			 * string will be 13 characters long` |
|        - | 10631 | `			 */` |
|       25 | 10632 | `		return SXERR_ABORT;` |
|        - | 10633 | `	}` |
|      169 | 10634 | `	if( nBuflen > 22 ){` |
|      ! 0 | 10635 | `		return SXERR_ABORT;` |
|        - | 10636 | `	}` |
|        - | 10637 | `	/* Safely Consume the hex stream */` |
|      169 | 10638 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 10639 | `	return SXRET_OK;` |
|       97 | 10640 |  |
|        - | 10641 | `/*` |
|        - | 10642 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 10643 | ` *  Generate a unique ID` |
|        - | 10644 | ` * Parameter` |
|        - | 10645 | ` * $prefix` |
|        - | 10646 | ` *  Append this prefix to the generated unique ID.` |
|        - | 10647 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 10648 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 10649 | ` * $more_entropy` |
|        - | 10650 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 10651 | ` *  that the result will be unique.` |
|        - | 10652 | ` * Return` |
|        - | 10653 | ` *  Returns the unique identifier, as a string.` |
|        - | 10654 | ` */` |
|       24 | 10655 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10656 |  |
|        - | 10657 | `	struct unique_id_data sUniq;` |
|        - | 10658 | `	unsigned char zDigest[20];` |
|       25 | 10659 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10660 | `	const char *zPrefix;` |
|        - | 10661 | `	SHA1Context sCtx;` |
|        - | 10662 | `	char zRandom[7];` |
|        - | 10663 | `	int nPrefix;` |
|        - | 10664 | `	int entropy;` |
|        - | 10665 | `	/* Generate a random string first */` |
|       25 | 10666 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 10667 | `	/* Initialize fields */` |
|       25 | 10668 | `	zPrefix = 0;` |
|       25 | 10669 | `	nPrefix = 0;` |
|       25 | 10670 | `	entropy = 0;` |
|       25 | 10671 | `	if( nArg > 0 ){` |
|        - | 10672 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 10673 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 10674 | `		if( nArg > 1 ){` |
|      ! 0 | 10675 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 10676 | `		}` |
|      ! 0 | 10677 | `	}` |
|       25 | 10678 | `	SHA1Init(&sCtx);` |
|        - | 10679 | `	/* Generate the random ID */` |
|       25 | 10680 | `	if( nPrefix > 0 ){` |
|      ! 0 | 10681 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 10682 | `	}` |
|        - | 10683 | `	/* Append the random ID */` |
|       25 | 10684 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 10685 | `	/* Append the random string */` |
|       25 | 10686 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 10687 | `	/* Increment the number */` |
|       25 | 10688 | `	pVm->unique_id++;` |
|       25 | 10689 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 10690 | `	/* Hexify the digest */` |
|       25 | 10691 | `	sUniq.pCtx = pCtx;` |
|       25 | 10692 | `	sUniq.entropy = entropy;` |
|       25 | 10693 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 10694 | `	/* All done */` |
|       25 | 10695 | `	return PH7_OK;` |
|        1 | 10696 |  |
|        - | 10697 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10698 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10699 | `/*` |
|        - | 10700 | ` * Section:` |
|        - | 10701 | ` *  Language construct implementation as foreign functions.` |
|        - | 10702 | ` * Status:` |
|        - | 10703 | ` *    Stable.` |
|        - | 10704 | ` */` |
|        - | 10705 | `/*` |
|        - | 10706 | ` * void echo($string...)` |
|        - | 10707 | ` *  Output one or more messages.` |
|        - | 10708 | ` * Parameters` |
|        - | 10709 | ` *  $string` |
|        - | 10710 | ` *   Message to output.` |
|        - | 10711 | ` * Return` |
|        - | 10712 | ` *  NULL.` |
|        - | 10713 | ` */` |
|      ! 0 | 10714 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10715 |  |
|        - | 10716 | `	const char *zData;` |
|      ! 0 | 10717 | `	int nDataLen = 0;` |
|        - | 10718 | `	ph7_vm *pVm;` |
|        - | 10719 | `	int i,rc;` |
|        - | 10720 | `	/* Point to the target VM */` |
|      ! 0 | 10721 | `	pVm = pCtx->pVm;` |
|        - | 10722 | `	/* Output */` |
|      ! 0 | 10723 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 10724 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 10725 | `		if( nDataLen > 0 ){` |
|      ! 0 | 10726 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 10727 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 10728 | `			if( rc == SXERR_ABORT ){` |
|        - | 10729 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10730 | `				return PH7_ABORT;` |
|        - | 10731 | `			}` |
|      ! 0 | 10732 | `		}` |
|      ! 0 | 10733 | `	}` |
|      ! 0 | 10734 | `	return SXRET_OK;` |
|      ! 0 | 10735 |  |
|        - | 10736 | `/*` |
|        - | 10737 | ` * int print($string...)` |
|        - | 10738 | ` *  Output one or more messages.` |
|        - | 10739 | ` * Parameters` |
|        - | 10740 | ` *  $string` |
|        - | 10741 | ` *   Message to output.` |
|        - | 10742 | ` * Return` |
|        - | 10743 | ` *  1 always.` |
|        - | 10744 | ` */` |
|        2 | 10745 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10746 |  |
|        - | 10747 | `	const char *zData;` |
|        3 | 10748 | `	int nDataLen = 0;` |
|        - | 10749 | `	ph7_vm *pVm;` |
|        - | 10750 | `	int i,rc;` |
|        - | 10751 | `	/* Point to the target VM */` |
|        3 | 10752 | `	pVm = pCtx->pVm;` |
|        - | 10753 | `	/* Output */` |
|        5 | 10754 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 10755 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 10756 | `		if( nDataLen > 0 ){` |
|        3 | 10757 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 10758 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 10759 | `			if( rc == SXERR_ABORT ){` |
|        - | 10760 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10761 | `				return PH7_ABORT;` |
|        - | 10762 | `			}` |
|        1 | 10763 | `		}` |
|        2 | 10764 | `	}` |
|        - | 10765 | `	/* Return 1 */` |
|        3 | 10766 | `	ph7_result_int(pCtx,1);` |
|        3 | 10767 | `	return SXRET_OK;` |
|        2 | 10768 |  |
|        - | 10769 | `/*` |
|        - | 10770 | ` * void exit(string $msg)` |
|        - | 10771 | ` * void exit(int $status)` |
|        - | 10772 | ` * void die(string $ms)` |
|        - | 10773 | ` * void die(int $status)` |
|        - | 10774 | ` *   Output a message and terminate program execution.` |
|        - | 10775 | ` * Parameter` |
|        - | 10776 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 10777 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 10778 | ` *  and not printed` |
|        - | 10779 | ` * Return` |
|        - | 10780 | ` *  NULL` |
|        - | 10781 | ` */` |
|      ! 0 | 10782 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10783 |  |
|      ! 0 | 10784 | `	if( nArg > 0 ){` |
|      ! 0 | 10785 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 10786 | `			const char *zData;` |
|      ! 0 | 10787 | `			int iLen = 0;` |
|        - | 10788 | `			/* Print exit message */` |
|      ! 0 | 10789 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 10790 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 10791 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 10792 | `			sxi32 iExitStatus;` |
|        - | 10793 | `			/* Record exit status code */` |
|      ! 0 | 10794 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 10795 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 10796 | `		}` |
|      ! 0 | 10797 | `	}` |
|        - | 10798 | `	/* Check if we are in an included file */` |
|      ! 0 | 10799 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 10800 | `		/* Exit the entire process */` |
|      ! 0 | 10801 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 10802 | `	}` |
|        - | 10803 | `	/* Abort processing immediately */` |
|      ! 0 | 10804 | `	return PH7_ABORT;` |
|      ! 0 | 10805 |  |
|        - | 10806 | `/*` |
|        - | 10807 | ` * bool isset($var,...)` |
|        - | 10808 | ` *  Finds out whether a variable is set.` |
|        - | 10809 | ` * Parameters` |
|        - | 10810 | ` *  One or more variable to check.` |
|        - | 10811 | ` * Return` |
|        - | 10812 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 10813 | ` */` |
|    82654 | 10814 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10815 |  |
|        - | 10816 | `	ph7_value *pObj;` |
|    82656 | 10817 | `	int res = 0;` |
|        - | 10818 | `	int i;` |
|    82656 | 10819 | `	if( nArg < 1 ){` |
|        - | 10820 | `		/* Missing arguments,return false */` |
|      ! 0 | 10821 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 10822 | `		return SXRET_OK;` |
|        - | 10823 | `	}` |
|        - | 10824 | `	/* Iterate over available arguments */` |
|   108464 | 10825 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    82656 | 10826 | `		pObj = apArg[i];` |
|    82656 | 10827 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    56228 | 10828 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10829 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 10830 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 10831 | `			}` |
|    28113 | 10832 | `		}` |
|    82656 | 10833 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    82656 | 10834 | `		if( !res ){` |
|        - | 10835 | `			/* Variable not set,return FALSE */` |
|    56848 | 10836 | `			ph7_result_bool(pCtx,0);` |
|    56848 | 10837 | `			return SXRET_OK;` |
|        - | 10838 | `		}` |
|    12906 | 10839 | `	}` |
|        - | 10840 | `	/* All given variable are set,return TRUE */` |
|    25810 | 10841 | `	ph7_result_bool(pCtx,1);` |
|    25810 | 10842 | `	return SXRET_OK;` |
|    41329 | 10843 |  |
|        - | 10844 | `/*` |
|        - | 10845 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 10846 | ` * frame,the reference table and discard it's contents.` |
|        - | 10847 | ` * This function never fail and always return SXRET_OK.` |
|        - | 10848 | ` */` |
|  3066480 | 10849 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 10850 |  |
|        - | 10851 | `	ph7_value *pObj;` |
|        - | 10852 | `	VmRefObj *pRef;` |
|  3066482 | 10853 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3066482 | 10854 | `	if( pObj ){` |
|        - | 10855 | `		/* Release the object */` |
|  3066482 | 10856 | `		PH7_MemObjRelease(pObj);` |
|  1533240 | 10857 | `	}` |
|        - | 10858 | `	/* Remove old reference links */` |
|  3066482 | 10859 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3066482 | 10860 | `	if( pRef ){` |
|  3066476 | 10861 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 10862 | `		/* Unlink from the reference table */` |
|  3066476 | 10863 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3066476 | 10864 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10865 | `			VmSlot sFree;` |
|        - | 10866 | `			/* Restore to the free list */` |
|  3066470 | 10867 | `			sFree.nIdx = nObjIdx;` |
|  3066470 | 10868 | `			sFree.pUserData = 0;` |
|  3066470 | 10869 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1533234 | 10870 | `		}` |
|  1533237 | 10871 | `	}` |
|  3066482 | 10872 | `	return SXRET_OK;` |
|        2 | 10873 |  |
|        - | 10874 | `/*` |
|        - | 10875 | ` * void unset($var,...)` |
|        - | 10876 | ` *   Unset one or more given variable.` |
|        - | 10877 | ` * Parameters` |
|        - | 10878 | ` *  One or more variable to unset.` |
|        - | 10879 | ` * Return` |
|        - | 10880 | ` *  Nothing.` |
|        - | 10881 | ` */` |
|     7154 | 10882 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10883 |  |
|        - | 10884 | `	ph7_value *pObj;` |
|        - | 10885 | `	ph7_vm *pVm;` |
|        - | 10886 | `	int i;` |
|        - | 10887 | `	/* Point to the target VM */` |
|     7156 | 10888 | `	pVm = pCtx->pVm;` |
|        - | 10889 | `	/* Iterate and unset */` |
|    14310 | 10890 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7156 | 10891 | `		pObj = apArg[i];` |
|     7156 | 10892 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 10893 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10894 | `				/* Throw an error */` |
|      ! 0 | 10895 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 10896 | `			}` |
|      ! 0 | 10897 | `		}else{` |
|     7156 | 10898 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 10899 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7156 | 10900 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7150 | 10901 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3574 | 10902 | `			}` |
|        - | 10903 | `		}` |
|     3579 | 10904 | `	}` |
|     7156 | 10905 | `	return SXRET_OK;` |
|        2 | 10906 |  |
|        - | 10907 | `/*` |
|        - | 10908 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 10909 | ` */` |
|      110 | 10910 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10911 |  |
|      111 | 10912 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 10913 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10914 | `	ph7_value *pObj;` |
|        - | 10915 | `	sxu32 nIdx;` |
|        - | 10916 | `	/* Extract the memory object */` |
|      111 | 10917 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 10918 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 10919 | `	if( pObj ){` |
|      111 | 10920 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 10921 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 10922 | `				SyString sName;` |
|        - | 10923 | `				ph7_value sKey;` |
|        - | 10924 | `				/* Perform the insertion */` |
|      109 | 10925 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 10926 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 10927 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 10928 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 10929 | `			}` |
|       54 | 10930 | `		}` |
|       55 | 10931 | `	}` |
|      111 | 10932 | `	return SXRET_OK;` |
|        1 | 10933 |  |
|        - | 10934 | `/*` |
|        - | 10935 | ` * array get_defined_vars(void)` |
|        - | 10936 | ` *  Returns an array of all defined variables.` |
|        - | 10937 | ` * Parameter` |
|        - | 10938 | ` *  None` |
|        - | 10939 | ` * Return` |
|        - | 10940 | ` *  An array with all the variables defined in the current scope.` |
|        - | 10941 | ` */` |
|        2 | 10942 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10943 |  |
|        3 | 10944 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10945 | `	ph7_value *pArray;` |
|        - | 10946 | `	/* Create a new array */` |
|        3 | 10947 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10948 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10949 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10950 | `		SXUNUSED(apArg);` |
|        - | 10951 | `		/* Return NULL */` |
|      ! 0 | 10952 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10953 | `		return SXRET_OK;` |
|        - | 10954 | `	}` |
|        - | 10955 | `	/* Superglobals first */` |
|        3 | 10956 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 10957 | `	/* Then variable defined in the current frame */` |
|        3 | 10958 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 10959 | `	/* Finally,return the created array */` |
|        3 | 10960 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10961 | `	return SXRET_OK;` |
|        2 | 10962 |  |
|        - | 10963 | `/*` |
|        - | 10964 | ` * bool gettype($var)` |
|        - | 10965 | ` *  Get the type of a variable` |
|        - | 10966 | ` * Parameters` |
|        - | 10967 | ` *   $var` |
|        - | 10968 | ` *    The variable being type checked.` |
|        - | 10969 | ` * Return` |
|        - | 10970 | ` *   String representation of the given variable type.` |
|        - | 10971 | ` */` |
|       32 | 10972 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10973 |  |
|       34 | 10974 | `	const char *zType = "Empty";` |
|       34 | 10975 | `	if( nArg > 0 ){` |
|       34 | 10976 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 10977 | `	}` |
|        - | 10978 | `	/* Return the variable type */` |
|       34 | 10979 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 10980 | `	return SXRET_OK;` |
|        2 | 10981 |  |
|        - | 10982 | `/*` |
|        - | 10983 | ` * string get_resource_type(resource $handle)` |
|        - | 10984 | ` *  This function gets the type of the given resource.` |
|        - | 10985 | ` * Parameters` |
|        - | 10986 | ` *  $handle` |
|        - | 10987 | ` *  The evaluated resource handle.` |
|        - | 10988 | ` * Return` |
|        - | 10989 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 10990 | ` *  representing its type. If the type is not identified by this function` |
|        - | 10991 | ` *  the return value will be the string Unknown.` |
|        - | 10992 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 10993 | ` *  is not a resource.` |
|        - | 10994 | ` */` |
|        2 | 10995 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10996 |  |
|        3 | 10997 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 10998 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 10999 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11000 | `		return PH7_OK;` |
|        - | 11001 | `	}` |
|        3 | 11002 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11003 | `	return SXRET_OK;` |
|        2 | 11004 |  |
|        - | 11005 | `/*` |
|        - | 11006 | ` * void var_dump(expression,....)` |
|        - | 11007 | ` *   var_dump � Dumps information about a variable` |
|        - | 11008 | ` * Parameters` |
|        - | 11009 | ` *   One or more expression to dump.` |
|        - | 11010 | ` * Returns` |
|        - | 11011 | ` *  Nothing.` |
|        - | 11012 | ` */` |
|      218 | 11013 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11014 |  |
|        - | 11015 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 11016 | `	int i;` |
|      220 | 11017 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 11018 | `	/* Dump one or more expressions */` |
|      444 | 11019 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 11020 | `		ph7_value *pObj = apArg[i];` |
|        - | 11021 | `		/* Reset the working buffer */` |
|      226 | 11022 | `		SyBlobReset(&sDump);` |
|        - | 11023 | `		/* Dump the given expression */` |
|      226 | 11024 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 11025 | `		/* Output */` |
|      226 | 11026 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 11027 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 11028 | `		}` |
|      114 | 11029 | `	}` |
|        - | 11030 | `	/* Release the working buffer */` |
|      220 | 11031 | `	SyBlobRelease(&sDump);` |
|      220 | 11032 | `	return SXRET_OK;` |
|        2 | 11033 |  |
|        - | 11034 | `/*` |
|        - | 11035 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 11036 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 11037 | ` * Parameters` |
|        - | 11038 | ` *   expression: Expression to dump` |
|        - | 11039 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 11040 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 11041 | ` *            print_r() will return the information rather than print it.` |
|        - | 11042 | ` * Return` |
|        - | 11043 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 11044 | ` *  Otherwise, the return value is TRUE.` |
|        - | 11045 | ` */` |
|       16 | 11046 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11047 |  |
|       17 | 11048 | `	int ret_string = 0;` |
|        - | 11049 | `	SyBlob sDump;` |
|       17 | 11050 | `	if( nArg < 1 ){` |
|        - | 11051 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11052 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11053 | `		return SXRET_OK;` |
|        - | 11054 | `	}` |
|       17 | 11055 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 11056 | `	if ( nArg > 1 ){` |
|        - | 11057 | `		/* Where to redirect output */` |
|       11 | 11058 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 11059 | `	}` |
|        - | 11060 | `	/* Generate dump */` |
|       17 | 11061 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 11062 | `	if( !ret_string ){` |
|        - | 11063 | `		/* Output dump */` |
|        7 | 11064 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11065 | `		/* Return true */` |
|        7 | 11066 | `		ph7_result_bool(pCtx,1);` |
|        4 | 11067 | `	}else{` |
|        - | 11068 | `		/* Generated dump as return value */` |
|       11 | 11069 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11070 | `	}` |
|        - | 11071 | `	/* Release the working buffer */` |
|       17 | 11072 | `	SyBlobRelease(&sDump);` |
|       17 | 11073 | `	return SXRET_OK;` |
|        9 | 11074 |  |
|        - | 11075 | `/*` |
|        - | 11076 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 11077 | ` * Same job as print_r. (see coment above)` |
|        - | 11078 | ` */` |
|        2 | 11079 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11080 |  |
|        3 | 11081 | `	int ret_string = 0;` |
|        - | 11082 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 11083 | `	if( nArg < 1 ){` |
|        - | 11084 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11085 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11086 | `		return SXRET_OK;` |
|        - | 11087 | `	}` |
|        3 | 11088 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 11089 | `	if ( nArg > 1 ){` |
|        - | 11090 | `		/* Where to redirect output */` |
|        3 | 11091 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 11092 | `	}` |
|        - | 11093 | `	/* Generate dump */` |
|        3 | 11094 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 11095 | `	if( !ret_string ){` |
|        - | 11096 | `		/* Output dump */` |
|      ! 0 | 11097 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11098 | `		/* Return NULL */` |
|      ! 0 | 11099 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11100 | `	}else{` |
|        - | 11101 | `		/* Generated dump as return value */` |
|        3 | 11102 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11103 | `	}` |
|        - | 11104 | `	/* Release the working buffer */` |
|        3 | 11105 | `	SyBlobRelease(&sDump);` |
|        3 | 11106 | `	return SXRET_OK;` |
|        2 | 11107 |  |
|        - | 11108 | `/*` |
|        - | 11109 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 11110 | ` *  Set/get the various assert flags.` |
|        - | 11111 | ` * Parameter` |
|        - | 11112 | ` * $what` |
|        - | 11113 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 11114 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 11115 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 11116 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 11117 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 11118 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 11119 | ` * $value` |
|        - | 11120 | ` *   An optional new value for the option.` |
|        - | 11121 | ` * Return` |
|        - | 11122 | ` *  Old setting on success or FALSE on failure.` |
|        - | 11123 | ` */` |
|       28 | 11124 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11125 |  |
|       30 | 11126 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11127 | `	int iOption;` |
|        - | 11128 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 11129 | `	if( nArg < 1 ){` |
|        3 | 11130 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11131 | `			"ArgumentCountError",` |
|        - | 11132 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 11133 | `			);` |
|        - | 11134 | `	}` |
|        - | 11135 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 11136 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 11137 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 11138 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11139 | `			"TypeError",` |
|        - | 11140 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 11141 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 11142 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 11143 | `			);` |
|        - | 11144 | `	}` |
|       28 | 11145 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 11146 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 11147 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 11148 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 11149 | `	switch( iOption ){` |
|        5 | 11150 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 11151 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 11152 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 11153 | `		if( nArg > 1 ){` |
|        5 | 11154 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11155 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 11156 | `			}else{` |
|        3 | 11157 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 11158 | `			}` |
|        2 | 11159 | `		}` |
|       12 | 11160 | `		break;` |
|        1 | 11161 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 11162 | `		/* Return old callback or null */` |
|        3 | 11163 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 11164 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 11165 | `		}else{` |
|        3 | 11166 | `			ph7_result_null(pCtx);` |
|        - | 11167 | `		}` |
|        3 | 11168 | `		if( nArg > 1 ){` |
|      ! 0 | 11169 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 11170 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 11171 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 11172 | `			}else{` |
|      ! 0 | 11173 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 11174 | `			}` |
|      ! 0 | 11175 | `		}` |
|        3 | 11176 | `		break;` |
|        5 | 11177 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 11178 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 11179 | `		if( nArg > 1 ){` |
|        5 | 11180 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11181 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 11182 | `			}else{` |
|        3 | 11183 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 11184 | `			}` |
|        2 | 11185 | `		}` |
|       11 | 11186 | `		break;` |
|      ! 0 | 11187 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 11188 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11189 | `		break;` |
|        1 | 11190 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 11191 | `		ph7_result_int(pCtx, 1);` |
|        3 | 11192 | `		break;` |
|      ! 0 | 11193 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 11194 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11195 | `		break;` |
|        1 | 11196 | `	default:` |
|        - | 11197 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 11198 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11199 | `			"ValueError",` |
|        - | 11200 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 11201 | `			);` |
|        - | 11202 | `	}` |
|       26 | 11203 | `	return PH7_OK;` |
|       16 | 11204 |  |
|        - | 11205 | `/*` |
|        - | 11206 | ` * bool assert(mixed $assertion)` |
|        - | 11207 | ` *  Checks if assertion is FALSE.` |
|        - | 11208 | ` * Parameter` |
|        - | 11209 | ` *  $assertion` |
|        - | 11210 | ` *    The assertion to test.` |
|        - | 11211 | ` * Return` |
|        - | 11212 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 11213 | ` */` |
|       24 | 11214 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11215 |  |
|       26 | 11216 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11217 | `	int iFlags,iResult;` |
|        - | 11218 | `	const char *zDesc;` |
|        - | 11219 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 11220 | `	if( nArg < 1 ){` |
|        3 | 11221 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11222 | `			"ArgumentCountError",` |
|        - | 11223 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 11224 | `			);` |
|        - | 11225 | `	}` |
|       24 | 11226 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 11227 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 11228 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 11229 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 11230 | `		return PH7_OK;` |
|        - | 11231 | `	}` |
|        - | 11232 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 11233 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 11234 | `	if( !iResult ){` |
|        - | 11235 | `		/* Assertion failed */` |
|        - | 11236 | `		/* Extract optional description */` |
|       13 | 11237 | `		zDesc = 0;` |
|       13 | 11238 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11239 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 11240 | `		}` |
|       13 | 11241 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 11242 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 11243 | `			ph7_value sFile,sLine;` |
|        - | 11244 | `			ph7_value *apCbArg[3];` |
|        - | 11245 | `			SyString *pFile;` |
|        - | 11246 | `			/* Extract the processed script */` |
|      ! 0 | 11247 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 11248 | `			if( pFile == 0 ){` |
|      ! 0 | 11249 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 11250 | `			}` |
|        - | 11251 | `			/* Invoke the callback */` |
|      ! 0 | 11252 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 11253 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 11254 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 11255 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 11256 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 11257 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 11258 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 11259 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 11260 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 11261 | `		}` |
|       13 | 11262 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 11263 | `			/* Abort VM execution immediately */` |
|      ! 0 | 11264 | `			return PH7_ABORT;` |
|        - | 11265 | `		}` |
|        - | 11266 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 11267 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 11268 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11269 | `				"AssertionError",` |
|        - | 11270 | `				"%s",` |
|        1 | 11271 | `				zDesc` |
|        - | 11272 | `				);` |
|      ! 0 | 11273 | `		}else{` |
|       11 | 11274 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11275 | `				"AssertionError",` |
|        - | 11276 | `				"assert(false)"` |
|        - | 11277 | `				);` |
|        - | 11278 | `		}` |
|        - | 11279 | `	}` |
|        - | 11280 | `	/* Assertion passed */` |
|       11 | 11281 | `	ph7_result_bool(pCtx,1);` |
|       11 | 11282 | `	return PH7_OK;` |
|       14 | 11283 |  |
|        - | 11284 | `/*` |
|        - | 11285 | ` * Section:` |
|        - | 11286 | ` *  Error reporting functions.` |
|        - | 11287 | ` * Status:` |
|        - | 11288 | ` *    Stable.` |
|        - | 11289 | ` */` |
|        - | 11290 | `/*` |
|        - | 11291 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 11292 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 11293 | ` * Parameters` |
|        - | 11294 | ` *  $error_msg` |
|        - | 11295 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 11296 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 11297 | ` * $error_type` |
|        - | 11298 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 11299 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 11300 | ` * Return` |
|        - | 11301 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 11302 | ` */` |
|       12 | 11303 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11304 |  |
|       14 | 11305 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 11306 | `	int rc = PH7_OK;` |
|       14 | 11307 | `	if( nArg > 0 ){` |
|        - | 11308 | `		const char *zErr;` |
|        - | 11309 | `		int nLen;` |
|        - | 11310 | `		/* Extract the error message */` |
|       12 | 11311 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 11312 | `		if( nArg > 1 ){` |
|        - | 11313 | `			/* Extract the error type */` |
|       12 | 11314 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 11315 | `			switch( nErr ){` |
|        1 | 11316 | `			case 1:   /* E_ERROR */` |
|        - | 11317 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 11318 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 11319 | `			case 256: /* E_USER_ERROR */` |
|        3 | 11320 | `				nErr = PH7_CTX_ERR;` |
|        3 | 11321 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 11322 | `				break;` |
|        1 | 11323 | `			case 2:   /* E_WARNING */` |
|        - | 11324 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 11325 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 11326 | `			case 512: /* E_USER_WARNING */` |
|        3 | 11327 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 11328 | `				break;` |
|        3 | 11329 | `			default:` |
|        8 | 11330 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 11331 | `				break;` |
|        - | 11332 | `			}` |
|        5 | 11333 | `		}` |
|        - | 11334 | `		/* Report error */` |
|       12 | 11335 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 11336 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 11337 | `			return rc;` |
|        - | 11338 | `		}` |
|        - | 11339 | `		/* Return true */` |
|       12 | 11340 | `		ph7_result_bool(pCtx,1);` |
|        7 | 11341 | `	}else{` |
|        - | 11342 | `		/* Missing arguments,return FALSE */` |
|        3 | 11343 | `		ph7_result_bool(pCtx,0);` |
|        - | 11344 | `	}` |
|       14 | 11345 | `	return rc;` |
|        8 | 11346 |  |
|        - | 11347 | `/*` |
|        - | 11348 | ` * int error_reporting([int $level])` |
|        - | 11349 | ` *  Sets which PHP errors are reported.` |
|        - | 11350 | ` * Parameters` |
|        - | 11351 | ` *  $level` |
|        - | 11352 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 11353 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 11354 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 11355 | ` *   levels will not always behave as expected.` |
|        - | 11356 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 11357 | ` *   in the predefined constants.` |
|        - | 11358 | ` * Return` |
|        - | 11359 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 11360 | ` *   parameter is given.` |
|        - | 11361 | ` */` |
|       38 | 11362 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11363 |  |
|       40 | 11364 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11365 | `	int nOld;` |
|        - | 11366 | `	/* Extract the old reporting level */` |
|       40 | 11367 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 11368 | `	if( nArg > 0 ){` |
|        - | 11369 | `		int nNew;` |
|        - | 11370 | `		/* Extract the desired error reporting level */` |
|       32 | 11371 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 11372 | `		if( !nNew ){` |
|        - | 11373 | `			/* Do not report errors at all */` |
|        5 | 11374 | `			pVm->bErrReport = 0;` |
|        3 | 11375 | `		}else{` |
|        - | 11376 | `			/* Report all errors */` |
|       28 | 11377 | `			pVm->bErrReport = 1;` |
|        - | 11378 | `		}` |
|       15 | 11379 | `	}` |
|        - | 11380 | `	/* Return the old level */` |
|       40 | 11381 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 11382 | `	return PH7_OK;` |
|        2 | 11383 |  |
|        - | 11384 | `/*` |
|        - | 11385 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 11386 | ` *  Send an error message somewhere.` |
|        - | 11387 | ` * Parameter` |
|        - | 11388 | ` *  $message` |
|        - | 11389 | ` *   The error message that should be logged.` |
|        - | 11390 | ` *  $message_type` |
|        - | 11391 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 11392 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 11393 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 11394 | ` *       This is the default option.` |
|        - | 11395 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 11396 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 11397 | ` *    2  No longer an option.` |
|        - | 11398 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 11399 | ` *       to the end of the message string.` |
|        - | 11400 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 11401 | ` *  $destination` |
|        - | 11402 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 11403 | ` *  $extra_headers` |
|        - | 11404 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 11405 | ` * Return` |
|        - | 11406 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11407 | ` * NOTE:` |
|        - | 11408 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 11409 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 11410 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 11411 | ` *  Otherwise this function is no-op.` |
|        - | 11412 | ` */` |
|        4 | 11413 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11414 |  |
|        - | 11415 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 11416 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 11417 | `	int iType = 0;` |
|        5 | 11418 | `	if( nArg < 1 ){` |
|        - | 11419 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 11420 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11421 | `		return PH7_OK;` |
|        - | 11422 | `	}` |
|        5 | 11423 | `	if( pVm->xErrLog  ){` |
|        - | 11424 | `		/* Invoke the user callback */` |
|      ! 0 | 11425 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 11426 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 11427 | `		if( nArg > 1 ){` |
|      ! 0 | 11428 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11429 | `			if( nArg > 2 ){` |
|      ! 0 | 11430 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 11431 | `				if( nArg > 3 ){` |
|      ! 0 | 11432 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 11433 | `				}` |
|      ! 0 | 11434 | `			}` |
|      ! 0 | 11435 | `		}` |
|      ! 0 | 11436 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 11437 | `	}` |
|        - | 11438 | `	/* Retun TRUE */` |
|        5 | 11439 | `	ph7_result_bool(pCtx,1);` |
|        5 | 11440 | `	return PH7_OK;` |
|        3 | 11441 |  |
|        - | 11442 | `/*` |
|        - | 11443 | ` * bool restore_exception_handler(void)` |
|        - | 11444 | ` *  Restores the previously defined exception handler function.` |
|        - | 11445 | ` * Parameter` |
|        - | 11446 | ` *  None` |
|        - | 11447 | ` * Return` |
|        - | 11448 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 11449 | ` */` |
|        4 | 11450 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11451 |  |
|        5 | 11452 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11453 | `	ph7_value *pOld,*pNew;` |
|        - | 11454 | `	/* Point to the old and the new handler */` |
|        5 | 11455 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 11456 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 11457 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11458 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11459 | `		SXUNUSED(apArg);` |
|        - | 11460 | `		/* No installed handler,return FALSE */` |
|        5 | 11461 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11462 | `		return PH7_OK;` |
|        - | 11463 | `	}` |
|        - | 11464 | `	/* Copy the old handler */` |
|      ! 0 | 11465 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11466 | `	PH7_MemObjRelease(pOld);` |
|        - | 11467 | `	/* Return TRUE */` |
|      ! 0 | 11468 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11469 | `	return PH7_OK;` |
|        3 | 11470 |  |
|        - | 11471 | `/*` |
|        - | 11472 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 11473 | ` *  Sets a user-defined exception handler function.` |
|        - | 11474 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 11475 | ` * NOTE` |
|        - | 11476 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 11477 | ` *  the satndard PHP engine.` |
|        - | 11478 | ` * Parameters` |
|        - | 11479 | ` *  $exception_handler` |
|        - | 11480 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 11481 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 11482 | ` *   that was thrown.` |
|        - | 11483 | ` *  Note:` |
|        - | 11484 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11485 | ` * Return` |
|        - | 11486 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 11487 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11488 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11489 | ` */` |
|        4 | 11490 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11491 |  |
|        6 | 11492 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11493 | `	ph7_value *pOld,*pNew;` |
|        - | 11494 | `	/* Point to the old and the new handler */` |
|        6 | 11495 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 11496 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 11497 | `	/* Return the old handler */` |
|        6 | 11498 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 11499 | `	if( nArg > 0 ){` |
|        6 | 11500 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11501 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 11502 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 11503 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11504 | `		}else{` |
|        6 | 11505 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11506 | `			/* Install the new handler */` |
|        6 | 11507 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11508 | `		}` |
|        2 | 11509 | `	}` |
|        6 | 11510 | `	return PH7_OK;` |
|        2 | 11511 |  |
|        - | 11512 | `/*` |
|        - | 11513 | ` * bool restore_error_handler(void)` |
|        - | 11514 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11515 | ` * Parameters:` |
|        - | 11516 | ` *  None.` |
|        - | 11517 | ` * Return` |
|        - | 11518 | ` *  Always TRUE.` |
|        - | 11519 | ` */` |
|        4 | 11520 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11521 |  |
|        5 | 11522 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11523 | `	ph7_value *pOld,*pNew;` |
|        - | 11524 | `	/* Point to the old and the new handler */` |
|        5 | 11525 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 11526 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 11527 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11528 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11529 | `		SXUNUSED(apArg);` |
|        - | 11530 | `		/* No installed callback,return FALSE */` |
|        5 | 11531 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11532 | `		return PH7_OK;` |
|        - | 11533 | `	}` |
|        - | 11534 | `	/* Copy the old callback */` |
|      ! 0 | 11535 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11536 | `	PH7_MemObjRelease(pOld);` |
|        - | 11537 | `	/* Return TRUE */` |
|      ! 0 | 11538 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11539 | `	return PH7_OK;` |
|        3 | 11540 |  |
|        - | 11541 | `/*` |
|        - | 11542 | ` * value set_error_handler(callable $error_handler)` |
|        - | 11543 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11544 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11545 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11546 | ` *  Sets a user-defined error handler function.` |
|        - | 11547 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 11548 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 11549 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 11550 | ` *  conditions (using trigger_error()).` |
|        - | 11551 | ` * Parameters` |
|        - | 11552 | ` *  $error_handler` |
|        - | 11553 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 11554 | ` *   describing the error.` |
|        - | 11555 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 11556 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 11557 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 11558 | ` *   The function can be shown as:` |
|        - | 11559 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 11560 | ` *     errno` |
|        - | 11561 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 11562 | ` *   errstr` |
|        - | 11563 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 11564 | ` *   errfile` |
|        - | 11565 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 11566 | ` *     was raised in, as a string.` |
|        - | 11567 | ` *  Note:` |
|        - | 11568 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11569 | ` * Return` |
|        - | 11570 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 11571 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11572 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11573 | ` */` |
|     9878 | 11574 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11575 |  |
|     9880 | 11576 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11577 | `	ph7_value *pOld,*pNew;` |
|        - | 11578 | `	/* Point to the old and the new handler */` |
|     9880 | 11579 | `	pOld = &pVm->aErrCB[0];` |
|     9880 | 11580 | `	pNew = &pVm->aErrCB[1];` |
|        - | 11581 | `	/* Return the old handler */` |
|     9880 | 11582 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9880 | 11583 | `	if( nArg > 0 ){` |
|     9880 | 11584 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11585 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4939 | 11586 | `			PH7_MemObjRelease(pNew);` |
|     4939 | 11587 | `			ph7_result_bool(pCtx,1);` |
|     2470 | 11588 | `		}else{` |
|     4942 | 11589 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11590 | `			/* Install the new handler */` |
|     4942 | 11591 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11592 | `		}` |
|     4939 | 11593 | `	}` |
|     9880 | 11594 | `	return PH7_OK;` |
|        2 | 11595 |  |
|        - | 11596 | `/*` |
|        - | 11597 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 11598 | ` *  Generates a backtrace.` |
|        - | 11599 | ` * Paramaeter` |
|        - | 11600 | ` *  $options` |
|        - | 11601 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 11602 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 11603 | ` *   all the function/method arguments, to save memory.` |
|        - | 11604 | ` * $limit` |
|        - | 11605 | ` *   (Not Used)` |
|        - | 11606 | ` * Return` |
|        - | 11607 | ` *  An array.The possible returned elements are as follows:` |
|        - | 11608 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 11609 | ` *          Name        Type      Description` |
|        - | 11610 | ` *          ------      ------     -----------` |
|        - | 11611 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 11612 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 11613 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 11614 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 11615 | ` *          object      object    The current object.` |
|        - | 11616 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 11617 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 11618 | ` */` |
|      614 | 11619 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11620 |  |
|      616 | 11621 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11622 | `	ph7_value *pArray;` |
|        - | 11623 | `	ph7_class *pClass;` |
|        - | 11624 | `	ph7_value *pValue;` |
|        - | 11625 | `	SyString *pFile;` |
|        - | 11626 | `	/* Create a new array */` |
|      616 | 11627 | `	pArray = ph7_context_new_array(pCtx);` |
|      616 | 11628 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      616 | 11629 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11630 | `		/* Out of memory,return NULL */` |
|      ! 0 | 11631 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11632 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11633 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11634 | `		SXUNUSED(apArg);` |
|      ! 0 | 11635 | `		return PH7_OK;` |
|        - | 11636 | `	}` |
|        - | 11637 | `	/* Dump running function name and it's arguments  */` |
|      616 | 11638 | `	if( pVm->pFrame->pParent ){` |
|      616 | 11639 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 11640 | `		ph7_vm_func *pFunc;` |
|        - | 11641 | `		ph7_value *pArg;` |
|      616 | 11642 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      616 | 11643 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      616 | 11644 | `		if( pFrame->pParent && pFunc ){` |
|      616 | 11645 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      616 | 11646 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      616 | 11647 | `			ph7_value_reset_string_cursor(pValue);` |
|      307 | 11648 | `		}` |
|        - | 11649 | `		/* Function arguments */` |
|      616 | 11650 | `		pArg = ph7_context_new_array(pCtx);` |
|      616 | 11651 | `		if( pArg  ){` |
|        - | 11652 | `			ph7_value *pObj;` |
|        - | 11653 | `			VmSlot *aSlot;` |
|        - | 11654 | `			sxu32 n;` |
|        - | 11655 | `			/* Start filling the array with the given arguments */` |
|      616 | 11656 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2450 | 11657 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1836 | 11658 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1836 | 11659 | `				if( pObj ){` |
|     1836 | 11660 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      917 | 11661 | `				}` |
|      919 | 11662 | `			}` |
|        - | 11663 | `			/* Save the array */` |
|      616 | 11664 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      307 | 11665 | `		}` |
|      307 | 11666 | `	}` |
|      616 | 11667 | `	ph7_value_int(pValue,1);` |
|        - | 11668 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 11669 | `	 * line numbers at run-time. )` |
|        - | 11670 | `	 */` |
|      616 | 11671 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 11672 | `	/* Current processed script */` |
|      616 | 11673 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      616 | 11674 | `	if( pFile ){` |
|      616 | 11675 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      616 | 11676 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      616 | 11677 | `		ph7_value_reset_string_cursor(pValue);` |
|      307 | 11678 | `	}` |
|        - | 11679 | `	/* Top class */` |
|      616 | 11680 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      616 | 11681 | `	if( pClass ){` |
|      612 | 11682 | `		ph7_value_reset_string_cursor(pValue);` |
|      612 | 11683 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      612 | 11684 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      305 | 11685 | `	}` |
|        - | 11686 | `	/* Return the freshly created array */` |
|      616 | 11687 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11688 | `	/*` |
|        - | 11689 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 11690 | `	 * as soon we return from this function.` |
|        - | 11691 | `	 */` |
|      616 | 11692 | `	return PH7_OK;` |
|      309 | 11693 |  |
|        - | 11694 | `/*` |
|        - | 11695 | ` * Generate a small backtrace.` |
|        - | 11696 | ` * Store the generated dump in the given BLOB` |
|        - | 11697 | ` */` |
|        4 | 11698 | `static int VmMiniBacktrace(` |
|        - | 11699 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11700 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 11701 | `	)` |
|        1 | 11702 |  |
|        5 | 11703 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11704 | `	ph7_vm_func *pFunc;` |
|        - | 11705 | `	ph7_class *pClass;` |
|        - | 11706 | `	SyString *pFile;` |
|        - | 11707 | `	/* Called function */` |
|        5 | 11708 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 11709 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 11710 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11711 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 11712 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 11713 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 11714 | `	}else{` |
|      ! 0 | 11715 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 11716 | `	}` |
|        5 | 11717 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 11718 | `	/* Current processed script */` |
|        5 | 11719 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 11720 | `	if( pFile ){` |
|        5 | 11721 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11722 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 11723 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 11724 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 11725 | `	}` |
|        - | 11726 | `	/* Top class */` |
|        5 | 11727 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 11728 | `	if( pClass ){` |
|      ! 0 | 11729 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 11730 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 11731 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 11732 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 11733 | `	}` |
|        5 | 11734 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 11735 | `	/* All done */` |
|        5 | 11736 | `	return SXRET_OK;` |
|        1 | 11737 |  |
|        - | 11738 | `/*` |
|        - | 11739 | ` * void debug_print_backtrace()` |
|        - | 11740 | ` *  Prints a backtrace` |
|        - | 11741 | ` * Parameters` |
|        - | 11742 | ` * None` |
|        - | 11743 | ` * Return` |
|        - | 11744 | ` * NULL` |
|        - | 11745 | ` */` |
|        2 | 11746 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11747 |  |
|        3 | 11748 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11749 | `	SyBlob sDump;` |
|        3 | 11750 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11751 | `	/* Generate the backtrace */` |
|        3 | 11752 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11753 | `	/* Output backtrace */` |
|        3 | 11754 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11755 | `	/* All done,cleanup */` |
|        3 | 11756 | `	SyBlobRelease(&sDump);` |
|        1 | 11757 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11758 | `	SXUNUSED(apArg);` |
|        3 | 11759 | `	return PH7_OK;` |
|        1 | 11760 |  |
|        - | 11761 | `/*` |
|        - | 11762 | ` * string debug_string_backtrace()` |
|        - | 11763 | ` *  Generate a backtrace` |
|        - | 11764 | ` * Parameters` |
|        - | 11765 | ` * None` |
|        - | 11766 | ` * Return` |
|        - | 11767 | ` *  A mini backtrace().` |
|        - | 11768 | ` * Note that this is a symisc extension.` |
|        - | 11769 | ` */` |
|        2 | 11770 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11771 |  |
|        3 | 11772 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11773 | `	SyBlob sDump;` |
|        3 | 11774 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11775 | `	/* Generate the backtrace */` |
|        3 | 11776 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11777 | `	/* Return the backtrace */` |
|        3 | 11778 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 11779 | `	/* All done,cleanup */` |
|        3 | 11780 | `	SyBlobRelease(&sDump);` |
|        1 | 11781 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11782 | `	SXUNUSED(apArg);` |
|        3 | 11783 | `	return PH7_OK;` |
|        1 | 11784 |  |
|        - | 11785 | `/*` |
|        - | 11786 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 11787 | ` * exception is triggered.` |
|        - | 11788 | ` */` |
|      480 | 11789 | `static sxi32 VmUncaughtException(` |
|        - | 11790 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11791 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11792 | `	)` |
|        1 | 11793 |  |
|        - | 11794 | `	ph7_value *apArg[2],sArg;` |
|      481 | 11795 | `	int nArg = 1;` |
|        - | 11796 | `	sxi32 rc;` |
|      481 | 11797 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 11798 | `		/* Nesting limit reached */` |
|      ! 0 | 11799 | `		return SXRET_OK;` |
|        - | 11800 | `	}` |
|        - | 11801 | `	/* Call any exception handler if available */` |
|      481 | 11802 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 11803 | `	if( pThis ){` |
|        - | 11804 | `		/* Load the exception instance */` |
|      481 | 11805 | `		sArg.x.pOther = pThis;` |
|      481 | 11806 | `		pThis->iRef++;` |
|      481 | 11807 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 11808 | `	}else{` |
|      ! 0 | 11809 | `		nArg = 0;` |
|        - | 11810 | `	}` |
|      481 | 11811 | `	apArg[0] = &sArg;` |
|        - | 11812 | `	/* Call the exception handler if available */` |
|      481 | 11813 | `	pVm->nExceptDepth++;` |
|      481 | 11814 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 11815 | `	pVm->nExceptDepth--;` |
|      481 | 11816 | `	if( rc != SXRET_OK ){` |
|        - | 11817 | `		SyBlob sMsgBuf;` |
|      479 | 11818 | `		const char *zClass = "Exception";` |
|      479 | 11819 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 11820 | `		const char *zMsg;` |
|        - | 11821 | `		sxu32 nMsg;` |
|        - | 11822 | `		const char *zFuncName;` |
|        - | 11823 | `		int nFuncLen;` |
|      479 | 11824 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 11825 | `		if( pThis ){` |
|        - | 11826 | `			ph7_class_method *pGetMessage;` |
|        - | 11827 | `			ph7_value sMsg;` |
|        - | 11828 | `			const char *zTmp;` |
|        - | 11829 | `			int nTmp;` |
|      479 | 11830 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 11831 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 11832 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 11833 | `			if( pGetMessage ){` |
|      479 | 11834 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 11835 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 11836 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 11837 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 11838 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 11839 | `					}` |
|      239 | 11840 | `				}` |
|      479 | 11841 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 11842 | `			}` |
|      239 | 11843 | `		}` |
|      479 | 11844 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 11845 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 11846 | `		}` |
|      479 | 11847 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 11848 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 11849 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 11850 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 11851 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 11852 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 11853 | `		rc = SXERR_ABORT;` |
|      239 | 11854 | `	}` |
|      481 | 11855 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 11856 | `	return rc;` |
|      241 | 11857 |  |
|        - | 11858 | `/*` |
|        - | 11859 | ` * Throw a user exception.` |
|        - | 11860 | ` *` |
|        - | 11861 | ` * Exception dispatch follows this sequence:` |
|        - | 11862 | ` *` |
|        - | 11863 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 11864 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 11865 | ` *` |
|        - | 11866 | ` * 2. If NO catch matches:` |
|        - | 11867 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 11868 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 11869 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 11870 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 11871 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 11872 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 11873 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 11874 | ` *` |
|        - | 11875 | ` * 3. If a catch DOES match:` |
|        - | 11876 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 11877 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 11878 | ` *       inside the catch body from immediately propagating past our` |
|        - | 11879 | ` *       finally block.` |
|        - | 11880 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 11881 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 11882 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 11883 | ` *       in pPendingException (step 2c).` |
|        - | 11884 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 11885 | ` *    d. Run finally (if present).` |
|        - | 11886 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 11887 | ` *       that handlers are restored and finally has run.` |
|        - | 11888 | ` */` |
|      618 | 11889 | `static sxi32 VmThrowException(` |
|        - | 11890 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 11891 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11892 | `	)` |
|        2 | 11893 |  |
|        - | 11894 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 11895 | `	ph7_exception **apException;` |
|        - | 11896 | `	ph7_exception *pException;` |
|        - | 11897 | `	/* Point to the stack of loaded exceptions */` |
|      620 | 11898 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      620 | 11899 | `	pException = 0;` |
|      620 | 11900 | `	pCatch = 0;` |
|      620 | 11901 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11902 | `		ph7_exception_block *aCatch;` |
|        - | 11903 | `		ph7_class *pClass;` |
|        - | 11904 | `		SyString *aNames;` |
|        - | 11905 | `		sxu32 nNames;` |
|        - | 11906 | `		int matched;` |
|        - | 11907 | `		sxu32 j,k;` |
|        - | 11908 | `		/* Locate the appropriate block to execute */` |
|      134 | 11909 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      134 | 11910 | `		(void)SySetPop(&pVm->aException);` |
|      134 | 11911 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      136 | 11912 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 11913 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      134 | 11914 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      134 | 11915 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      134 | 11916 | `			matched = 0;` |
|      148 | 11917 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 11918 | `				/* Extract the target class */` |
|      146 | 11919 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|      146 | 11920 | `				if( pClass == 0 ){` |
|        - | 11921 | `					/* No such class */` |
|      ! 0 | 11922 | `					continue;` |
|        - | 11923 | `				}` |
|      146 | 11924 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      132 | 11925 | `					matched = 1;` |
|      132 | 11926 | `					break;` |
|        - | 11927 | `				}` |
|        8 | 11928 | `			}` |
|      134 | 11929 | `			if( matched ){` |
|        - | 11930 | `				/* Catch block found,break immediately */` |
|      132 | 11931 | `				pCatch = &aCatch[j];` |
|      132 | 11932 | `				break;` |
|        - | 11933 | `			}` |
|        2 | 11934 | `		}` |
|       66 | 11935 | `	}` |
|        - | 11936 | `	/* Execute the cached block if available */` |
|      620 | 11937 | `	if( pCatch == 0 ){` |
|        - | 11938 | `		sxi32 rc;` |
|        - | 11939 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 11940 | `		if( pException && pException->iHasFinally ){` |
|        3 | 11941 | `			pException->iFinallyDone = 1;` |
|        3 | 11942 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 11943 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11944 | `				return SXERR_ABORT;` |
|        - | 11945 | `			}` |
|        1 | 11946 | `		}` |
|        - | 11947 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 11948 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11949 | `			/* Re-throw to the outer handler */` |
|        3 | 11950 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 11951 | `		}` |
|        - | 11952 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 11953 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 11954 | `		 * exception instead of reporting it uncaught.` |
|        - | 11955 | `		 */` |
|      488 | 11956 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 11957 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 11958 | `			 * by looking for a catch frame on the stack.` |
|        - | 11959 | `			 */` |
|      488 | 11960 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 11961 | `			int inCatch = 0;` |
|      974 | 11962 | `			while( pF ){` |
|      494 | 11963 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 11964 | `					inCatch = 1;` |
|        7 | 11965 | `					break;` |
|        - | 11966 | `				}` |
|      487 | 11967 | `				pF = pF->pParent;` |
|        1 | 11968 | `			}` |
|      488 | 11969 | `			if( inCatch ){` |
|        - | 11970 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 11971 | `				pThis->iRef++;` |
|        7 | 11972 | `				pVm->pPendingException = pThis;` |
|        7 | 11973 | `				return SXRET_OK;` |
|        - | 11974 | `			}` |
|      240 | 11975 | `		}` |
|        - | 11976 | `		/* Truly uncaught */` |
|      481 | 11977 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 11978 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 11979 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 11980 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 11981 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 11982 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 11983 | `			}` |
|      ! 0 | 11984 | `		}` |
|      481 | 11985 | `		return rc;` |
|      ! 0 | 11986 | `	}else{` |
|      132 | 11987 | `		VmFrame *pFrame = pVm->pFrame;` |
|      132 | 11988 | `		ph7_exception **apSaved = 0;` |
|        - | 11989 | `		sxu32 nSavedCount;` |
|        - | 11990 | `		sxi32 rc;` |
|      132 | 11991 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      132 | 11992 | `		if( pException->pFrame == pFrame ){` |
|       88 | 11993 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       43 | 11994 | `		}` |
|        - | 11995 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 11996 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 11997 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 11998 | `		 */` |
|      132 | 11999 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      132 | 12000 | `		if( nSavedCount > 0 ){` |
|       13 | 12001 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 12002 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 12003 | `			if( apSaved ){` |
|       13 | 12004 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 12005 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 12006 | `				SySetReset(&pVm->aException);` |
|        4 | 12007 | `			}` |
|        4 | 12008 | `		}` |
|        - | 12009 | `		/* Create a private frame first */` |
|      132 | 12010 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      132 | 12011 | `		if( rc == SXRET_OK ){` |
|      132 | 12012 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      132 | 12013 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      132 | 12014 | `			if( pObj ){` |
|      132 | 12015 | `				pThis->iRef++;` |
|      132 | 12016 | `				pObj->x.pOther = pThis;` |
|      132 | 12017 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       65 | 12018 | `			}` |
|        - | 12019 | `			/* Execute the catch block */` |
|      132 | 12020 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 12021 | `			/* Leave the frame */` |
|      132 | 12022 | `			VmLeaveFrame(&(*pVm));` |
|       65 | 12023 | `		}` |
|        - | 12024 | `		/* Restore the outer exception handlers */` |
|      132 | 12025 | `		if( apSaved ){` |
|        - | 12026 | `			sxu32 k;` |
|        - | 12027 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 12028 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 12029 | `			 * Restore the original outer entries.` |
|        - | 12030 | `			 */` |
|        9 | 12031 | `			SySetReset(&pVm->aException);` |
|       17 | 12032 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 12033 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 12034 | `			}` |
|        9 | 12035 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 12036 | `		}` |
|        - | 12037 | `		/* Execute the finally block after catch */` |
|      132 | 12038 | `		if( pException->iHasFinally ){` |
|       16 | 12039 | `			pException->iFinallyDone = 1;` |
|        - | 12040 | `			{` |
|       16 | 12041 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 12042 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 12043 | `					return SXERR_ABORT;` |
|        - | 12044 | `				}` |
|        - | 12045 | `			}` |
|        7 | 12046 | `		}` |
|      132 | 12047 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12048 | `			return SXERR_ABORT;` |
|        - | 12049 | `		}` |
|        - | 12050 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 12051 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 12052 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 12053 | `		 */` |
|      132 | 12054 | `		if( pVm->pPendingException ){` |
|        7 | 12055 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 12056 | `			pVm->pPendingException = 0;` |
|        7 | 12057 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 12058 | `		}` |
|        - | 12059 | `	}` |
|        - | 12060 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 12061 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 12062 | `	 */` |
|      126 | 12063 | `	return SXRET_OK;` |
|      311 | 12064 |  |
|        - | 12065 | `/*` |
|        - | 12066 | ` * Section:` |
|        - | 12067 | ` *  Version,Credits and Copyright related functions.` |
|        - | 12068 | ` * Status:` |
|        - | 12069 | ` *    Stable.` |
|        - | 12070 | ` */` |
|        - | 12071 | `/*` |
|        - | 12072 | ` * string ph7version(void)` |
|        - | 12073 | ` *  Returns the running version of the PH7 version.` |
|        - | 12074 | ` * Parameters` |
|        - | 12075 | ` *  None` |
|        - | 12076 | ` * Return` |
|        - | 12077 | ` * Current PH7 version.` |
|        - | 12078 | ` */` |
|        2 | 12079 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12080 |  |
|        1 | 12081 | `	SXUNUSED(nArg);` |
|        1 | 12082 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 12083 | `	/* Current engine version */` |
|        3 | 12084 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 12085 | `	return PH7_OK;` |
|        1 | 12086 |  |
|        - | 12087 | `/*` |
|        - | 12088 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 12089 | ` */` |
|        - | 12090 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 12091 | ` "<html><head>"\` |
|        - | 12092 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 12093 | ` "<style type=\"text/css\">"\` |
|        - | 12094 | ` "div {"\` |
|        - | 12095 | `     "border: 1px solid #cccccc;"\` |
|        - | 12096 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 12097 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 12098 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 12099 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 12100 | `     "-webkit-border-radius: 10px;"\` |
|        - | 12101 | `     "-o-border-radius: 10px;"\` |
|        - | 12102 | `     "border-radius: 10px;"\` |
|        - | 12103 | `     "padding-left: 2em;"\` |
|        - | 12104 | `     "background-color: white;"\` |
|        - | 12105 | `     "margin-left: auto;"\` |
|        - | 12106 | `     "font-family: verdana;"\` |
|        - | 12107 | `     "padding-right: 2em;"\` |
|        - | 12108 | `     "margin-right: auto;"\` |
|        - | 12109 | `     "}"\` |
|        - | 12110 | `     "body {"\` |
|        - | 12111 | `     "padding: 0.2em;"\` |
|        - | 12112 | `     "font-style: normal;"\` |
|        - | 12113 | `     "font-size: medium;"\` |
|        - | 12114 | `     "background-color: #f2f2f2;"\` |
|        - | 12115 | `     "}"\` |
|        - | 12116 | `     "hr {"\` |
|        - | 12117 | `     "border-style: solid none none;"\` |
|        - | 12118 | `     "border-width: 1px medium medium;"\` |
|        - | 12119 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 12120 | `     "height: 1px;"\` |
|        - | 12121 | `     "}"\` |
|        - | 12122 | `     "a {"\` |
|        - | 12123 | `     "color: #3366cc;"\` |
|        - | 12124 | `     "text-decoration: none;"\` |
|        - | 12125 | `     "}"\` |
|        - | 12126 | `     "a:hover {"\` |
|        - | 12127 | `     "color: #999999;"\` |
|        - | 12128 | `     "}"\` |
|        - | 12129 | `     "a:active {"\` |
|        - | 12130 | `     "color: #663399;"\` |
|        - | 12131 | `     "}"\` |
|        - | 12132 | `     "h1 {"\` |
|        - | 12133 | `     "margin: 0;"\` |
|        - | 12134 | `     "padding: 0;"\` |
|        - | 12135 | `     "font-family: Verdana;"\` |
|        - | 12136 | `     "font-weight: bold;"\` |
|        - | 12137 | `     "font-style: normal;"\` |
|        - | 12138 | `     "font-size: medium;"\` |
|        - | 12139 | `     "text-transform: capitalize;"\` |
|        - | 12140 | `     "color: #0a328c;"\` |
|        - | 12141 | `     "}"\` |
|        - | 12142 | `     "p {"\` |
|        - | 12143 | `     "margin: 0 auto;"\` |
|        - | 12144 | `     "font-size: medium;"\` |
|        - | 12145 | `     "font-style: normal;"\` |
|        - | 12146 | `     "font-family: verdana;"\` |
|        - | 12147 | `     "}"\` |
|        - | 12148 | `"</style></head><body>"\` |
|        - | 12149 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 12150 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 12151 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 12152 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 12153 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 12154 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 12155 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 12156 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 12157 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 12158 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 12159 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 12160 |  |
|        - | 12161 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12162 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 12163 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 12164 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 12165 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12166 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 12167 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12168 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 12169 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12170 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 12171 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12172 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 12173 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 12174 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 12175 |  |
|        - | 12176 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 12177 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 12178 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 12179 | `"&nbsp;*<br>"\` |
|        - | 12180 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 12181 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 12182 | `"&nbsp;* are met:<br>"\` |
|        - | 12183 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 12184 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 12185 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 12186 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 12187 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 12188 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 12189 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 12190 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 12191 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 12192 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 12193 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 12194 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 12195 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 12196 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 12197 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 12198 | `"&nbsp;*<br>"\` |
|        - | 12199 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 12200 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 12201 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 12202 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 12203 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 12204 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 12205 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 12206 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 12207 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 12208 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 12209 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 12210 | `"&nbsp;*/<br>"\` |
|        - | 12211 | `"</span></small></small></p>"\` |
|        - | 12212 | `"</div></body></html>"` |
|        - | 12213 | `/*` |
|        - | 12214 | ` * bool ph7credits(void)` |
|        - | 12215 | ` * bool ph7info(void)` |
|        - | 12216 | ` * bool ph7copyright(void)` |
|        - | 12217 | ` *  Prints out the credits for PH7 engine` |
|        - | 12218 | ` * Parameters` |
|        - | 12219 | ` *  None` |
|        - | 12220 | ` * Return` |
|        - | 12221 | ` *  Always TRUE` |
|        - | 12222 | ` */` |
|        2 | 12223 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12224 |  |
|        3 | 12225 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 12226 | `	/* Expand the HTML page above*/` |
|        3 | 12227 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 12228 | `	ph7_context_output_format(` |
|        1 | 12229 | `		pCtx,` |
|        - | 12230 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 12231 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 12232 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 12233 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 12234 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 12235 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 12236 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 12237 | `#ifdef __WINNT__` |
|        - | 12238 | `		"Windows NT"` |
|        - | 12239 | `#elif defined(__UNIXES__)` |
|        - | 12240 | `		"UNIX-Like"` |
|        - | 12241 | `#else` |
|        - | 12242 | `		"Other OS"` |
|        - | 12243 | `#endif` |
|        - | 12244 | `		);` |
|        3 | 12245 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 12246 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12247 | `	SXUNUSED(apArg);` |
|        - | 12248 | `	/* Return TRUE */` |
|        - | 12249 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 12250 | `	return PH7_OK;` |
|        1 | 12251 |  |
|        - | 12252 | `/*` |
|        - | 12253 | ` * Section:` |
|        - | 12254 | ` *    URL related routines.` |
|        - | 12255 | ` * Status:` |
|        - | 12256 | ` *    Stable.` |
|        - | 12257 | ` */` |
|        - | 12258 | `/*` |
|        - | 12259 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 12260 | ` *  Parse a URL and return its fields.` |
|        - | 12261 | ` * Parameters` |
|        - | 12262 | ` *  $url` |
|        - | 12263 | ` *   The URL to parse.` |
|        - | 12264 | ` * $component` |
|        - | 12265 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 12266 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 12267 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 12268 | ` *  in which case the return value will be an integer).` |
|        - | 12269 | ` * Return` |
|        - | 12270 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 12271 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 12272 | ` *  this array are:` |
|        - | 12273 | ` *   scheme - e.g. http` |
|        - | 12274 | ` *   host` |
|        - | 12275 | ` *   port` |
|        - | 12276 | ` *   user` |
|        - | 12277 | ` *   pass` |
|        - | 12278 | ` *   path` |
|        - | 12279 | ` *   query - after the question mark ?` |
|        - | 12280 | ` *   fragment - after the hashmark #` |
|        - | 12281 | ` * Note:` |
|        - | 12282 | ` *  FALSE is returned on failure.` |
|        - | 12283 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 12284 | ` *  with the standard PHP engine.` |
|        - | 12285 | ` */` |
|       28 | 12286 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12287 |  |
|        - | 12288 | `	const char *zStr; /* Input string */` |
|        - | 12289 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 12290 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 12291 | `	int nLen;` |
|        - | 12292 | `	sxi32 rc;` |
|       29 | 12293 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12294 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 12295 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12296 | `		return PH7_OK;` |
|        - | 12297 | `	}` |
|        - | 12298 | `	/* Extract the given URI */` |
|       29 | 12299 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 12300 | `	if( nLen < 1 ){` |
|        - | 12301 | `		/* Nothing to process,return FALSE */` |
|        3 | 12302 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12303 | `		return PH7_OK;` |
|        - | 12304 | `	}` |
|        - | 12305 | `	/* Get a parse */` |
|       27 | 12306 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 12307 | `	if( rc != SXRET_OK ){` |
|        - | 12308 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 12309 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12310 | `		return PH7_OK;` |
|        - | 12311 | `	}` |
|       27 | 12312 | `	if( nArg > 1 ){` |
|      ! 0 | 12313 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 12314 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 12315 | `		switch(nComponent){` |
|      ! 0 | 12316 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 12317 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 12318 | `			if( pComp->nByte < 1 ){` |
|        - | 12319 | `				/* No available value,return NULL */` |
|      ! 0 | 12320 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12321 | `			}else{` |
|      ! 0 | 12322 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12323 | `			}` |
|      ! 0 | 12324 | `			break;` |
|      ! 0 | 12325 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 12326 | `			pComp = &sURI.sHost;` |
|      ! 0 | 12327 | `			if( pComp->nByte < 1 ){` |
|        - | 12328 | `				/* No available value,return NULL */` |
|      ! 0 | 12329 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12330 | `			}else{` |
|      ! 0 | 12331 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12332 | `			}` |
|      ! 0 | 12333 | `			break;` |
|      ! 0 | 12334 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 12335 | `			pComp = &sURI.sPort;` |
|      ! 0 | 12336 | `			if( pComp->nByte < 1 ){` |
|        - | 12337 | `				/* No available value,return NULL */` |
|      ! 0 | 12338 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12339 | `			}else{` |
|      ! 0 | 12340 | `				int iPort = 0;` |
|        - | 12341 | `				/* Cast the value to integer */` |
|      ! 0 | 12342 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 12343 | `				ph7_result_int(pCtx,iPort);` |
|        - | 12344 | `			}` |
|      ! 0 | 12345 | `			break;` |
|      ! 0 | 12346 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 12347 | `			pComp = &sURI.sUser;` |
|      ! 0 | 12348 | `			if( pComp->nByte < 1 ){` |
|        - | 12349 | `				/* No available value,return NULL */` |
|      ! 0 | 12350 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12351 | `			}else{` |
|      ! 0 | 12352 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12353 | `			}` |
|      ! 0 | 12354 | `			break;` |
|      ! 0 | 12355 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 12356 | `			pComp = &sURI.sPass;` |
|      ! 0 | 12357 | `			if( pComp->nByte < 1 ){` |
|        - | 12358 | `				/* No available value,return NULL */` |
|      ! 0 | 12359 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12360 | `			}else{` |
|      ! 0 | 12361 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12362 | `			}` |
|      ! 0 | 12363 | `			break;` |
|      ! 0 | 12364 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 12365 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 12366 | `			if( pComp->nByte < 1 ){` |
|        - | 12367 | `				/* No available value,return NULL */` |
|      ! 0 | 12368 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12369 | `			}else{` |
|      ! 0 | 12370 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12371 | `			}` |
|      ! 0 | 12372 | `			break;` |
|      ! 0 | 12373 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 12374 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 12375 | `			if( pComp->nByte < 1 ){` |
|        - | 12376 | `				/* No available value,return NULL */` |
|      ! 0 | 12377 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12378 | `			}else{` |
|      ! 0 | 12379 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12380 | `			}` |
|      ! 0 | 12381 | `			break;` |
|      ! 0 | 12382 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 12383 | `			pComp = &sURI.sPath;` |
|      ! 0 | 12384 | `			if( pComp->nByte < 1 ){` |
|        - | 12385 | `				/* No available value,return NULL */` |
|      ! 0 | 12386 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12387 | `			}else{` |
|      ! 0 | 12388 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12389 | `			}` |
|      ! 0 | 12390 | `			break;` |
|      ! 0 | 12391 | `		default:` |
|        - | 12392 | `			/* No such entry,return NULL */` |
|      ! 0 | 12393 | `			ph7_result_null(pCtx);` |
|      ! 0 | 12394 | `			break;` |
|        - | 12395 | `		}` |
|      ! 0 | 12396 | `	}else{` |
|        - | 12397 | `		ph7_value *pArray,*pValue;` |
|        - | 12398 | `		/* Return an associative array */` |
|       27 | 12399 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 12400 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 12401 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12402 | `			/* Out of memory */` |
|      ! 0 | 12403 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12404 | `			/* Return false */` |
|      ! 0 | 12405 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 12406 | `			return PH7_OK;` |
|        - | 12407 | `		}` |
|        - | 12408 | `		/* Fill the array */` |
|       27 | 12409 | `		pComp = &sURI.sScheme;` |
|       27 | 12410 | `		if( pComp->nByte > 0 ){` |
|       19 | 12411 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 12412 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 12413 | `		}` |
|        - | 12414 | `		/* Reset the string cursor */` |
|       27 | 12415 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12416 | `		pComp = &sURI.sHost;` |
|       27 | 12417 | `		if( pComp->nByte > 0 ){` |
|       25 | 12418 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 12419 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 12420 | `		}` |
|        - | 12421 | `		/* Reset the string cursor */` |
|       27 | 12422 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12423 | `		pComp = &sURI.sPort;` |
|       27 | 12424 | `		if( pComp->nByte > 0 ){` |
|       11 | 12425 | `			int iPort = 0;/* cc warning */` |
|        - | 12426 | `			/* Convert to integer */` |
|       11 | 12427 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 12428 | `			ph7_value_int(pValue,iPort);` |
|       11 | 12429 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 12430 | `		}` |
|        - | 12431 | `		/* Reset the string cursor */` |
|       27 | 12432 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12433 | `		pComp = &sURI.sUser;` |
|       27 | 12434 | `		if( pComp->nByte > 0 ){` |
|        7 | 12435 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12436 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 12437 | `		}` |
|        - | 12438 | `		/* Reset the string cursor */` |
|       27 | 12439 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12440 | `		pComp = &sURI.sPass;` |
|       27 | 12441 | `		if( pComp->nByte > 0 ){` |
|        7 | 12442 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12443 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 12444 | `		}` |
|        - | 12445 | `		/* Reset the string cursor */` |
|       27 | 12446 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12447 | `		pComp = &sURI.sPath;` |
|       27 | 12448 | `		if( pComp->nByte > 0 ){` |
|       17 | 12449 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 12450 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 12451 | `		}` |
|        - | 12452 | `		/* Reset the string cursor */` |
|       27 | 12453 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12454 | `		pComp = &sURI.sQuery;` |
|       27 | 12455 | `		if( pComp->nByte > 0 ){` |
|        5 | 12456 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12457 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 12458 | `		}` |
|        - | 12459 | `		/* Reset the string cursor */` |
|       27 | 12460 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12461 | `		pComp = &sURI.sFragment;` |
|       27 | 12462 | `		if( pComp->nByte > 0 ){` |
|        5 | 12463 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12464 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 12465 | `		}` |
|        - | 12466 | `		/* Return the created array */` |
|       27 | 12467 | `		ph7_result_value(pCtx,pArray);` |
|        - | 12468 | `		/* NOTE:` |
|        - | 12469 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 12470 | `		 * automatically as soon we return from this function.` |
|        - | 12471 | `		 */` |
|        - | 12472 | `	}` |
|        - | 12473 | `	/* All done */` |
|       27 | 12474 | `	return PH7_OK;` |
|       15 | 12475 |  |
|        - | 12476 | `/*` |
|        - | 12477 | ` * Section:` |
|        - | 12478 | ` *   Array related routines.` |
|        - | 12479 | ` * Status:` |
|        - | 12480 | ` *    Stable.` |
|        - | 12481 | ` * Note 2012-5-21 01:04:15:` |
|        - | 12482 | ` *  Array related functions that need access to the underlying` |
|        - | 12483 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 12484 | ` */` |
|        - | 12485 | `/*` |
|        - | 12486 | ` * The [compact()] function store it's state information in an instance` |
|        - | 12487 | ` * of the following structure.` |
|        - | 12488 | ` */` |
|        - | 12489 | `struct compact_data` |
|        - | 12490 |  |
|        - | 12491 | `	ph7_value *pArray;  /* Target array */` |
|        - | 12492 | `	int nRecCount;      /* Recursion count */` |
|        - | 12493 | `};` |
|        - | 12494 | `/*` |
|        - | 12495 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 12496 | ` */` |
|      ! 0 | 12497 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 12498 |  |
|      ! 0 | 12499 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 12500 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 12501 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12502 | `	/* Act according to the hashmap value */` |
|      ! 0 | 12503 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 12504 | `		SyString sVar;` |
|      ! 0 | 12505 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 12506 | `		if( sVar.nByte > 0 ){` |
|        - | 12507 | `			/* Query the current frame */` |
|      ! 0 | 12508 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 12509 | `			/* ^` |
|        - | 12510 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 12511 | `			 */` |
|      ! 0 | 12512 | `			if( pKey ){` |
|        - | 12513 | `				/* Perform the insertion */` |
|      ! 0 | 12514 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 12515 | `			}` |
|      ! 0 | 12516 | `		}` |
|      ! 0 | 12517 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 12518 | `		int rc;` |
|        - | 12519 | `		/* Recursively traverse this array */` |
|      ! 0 | 12520 | `		pData->nRecCount++;` |
|      ! 0 | 12521 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 12522 | `		pData->nRecCount--;` |
|      ! 0 | 12523 | `		return rc;` |
|        - | 12524 | `	}` |
|      ! 0 | 12525 | `	return SXRET_OK;` |
|      ! 0 | 12526 |  |
|        - | 12527 | `/*` |
|        - | 12528 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 12529 | ` *  Create array containing variables and their values.` |
|        - | 12530 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 12531 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 12532 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 12533 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 12534 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 12535 | ` * Parameters` |
|        - | 12536 | ` *  $varname` |
|        - | 12537 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 12538 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 12539 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 12540 | ` *   it recursively.` |
|        - | 12541 | ` * Return` |
|        - | 12542 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 12543 | ` */` |
|        2 | 12544 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12545 |  |
|        - | 12546 | `	ph7_value *pArray,*pObj;` |
|        3 | 12547 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12548 | `	const char *zName;` |
|        - | 12549 | `	SyString sVar;` |
|        - | 12550 | `	int i,nLen;` |
|        3 | 12551 | `	if( nArg < 1 ){` |
|        - | 12552 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 12553 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12554 | `		return PH7_OK;` |
|        - | 12555 | `	}` |
|        - | 12556 | `	/* Create the array */` |
|        3 | 12557 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12558 | `	if( pArray == 0 ){` |
|        - | 12559 | `		/* Out of memory */` |
|      ! 0 | 12560 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12561 | `		/* Return NULL */` |
|      ! 0 | 12562 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12563 | `		return PH7_OK;` |
|        - | 12564 | `	}` |
|        - | 12565 | `	/* Perform the requested operation */` |
|        7 | 12566 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 12567 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 12568 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 12569 | `				struct compact_data sData;` |
|      ! 0 | 12570 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 12571 | `				/* Recursively walk the array */` |
|      ! 0 | 12572 | `				sData.nRecCount = 0;` |
|      ! 0 | 12573 | `				sData.pArray = pArray;` |
|      ! 0 | 12574 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 12575 | `			}` |
|      ! 0 | 12576 | `		}else{` |
|        - | 12577 | `			/* Extract variable name */` |
|        5 | 12578 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 12579 | `			if( nLen > 0 ){` |
|        5 | 12580 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 12581 | `				/* Check if the variable is available in the current frame */` |
|        5 | 12582 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 12583 | `				if( pObj ){` |
|        5 | 12584 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 12585 | `				}` |
|        2 | 12586 | `			}` |
|        - | 12587 | `		}` |
|        3 | 12588 | `	}` |
|        - | 12589 | `	/* Return the array */` |
|        3 | 12590 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12591 | `	return PH7_OK;` |
|        2 | 12592 |  |
|        - | 12593 | `/*` |
|        - | 12594 | ` * The [extract()] function store it's state information in an instance` |
|        - | 12595 | ` * of the following structure.` |
|        - | 12596 | ` */` |
|        - | 12597 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 12598 | `struct extract_aux_data` |
|        - | 12599 |  |
|        - | 12600 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 12601 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 12602 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 12603 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 12604 | `	int iFlags;           /* Control flags */` |
|        - | 12605 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 12606 | `};` |
|        - | 12607 | `/* Forward declaration */` |
|        - | 12608 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 12609 | `/*` |
|        - | 12610 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 12611 | ` *   Import variables into the current symbol table from an array.` |
|        - | 12612 | ` * Parameters` |
|        - | 12613 | ` * $var_array` |
|        - | 12614 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 12615 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 12616 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 12617 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 12618 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 12619 | ` * $extract_type` |
|        - | 12620 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 12621 | ` *  It can be one of the following values:` |
|        - | 12622 | ` *   EXTR_OVERWRITE` |
|        - | 12623 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 12624 | ` *   EXTR_SKIP` |
|        - | 12625 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 12626 | ` *   EXTR_PREFIX_SAME` |
|        - | 12627 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 12628 | ` *   EXTR_PREFIX_ALL` |
|        - | 12629 | ` *       Prefix all variable names with prefix.` |
|        - | 12630 | ` *   EXTR_PREFIX_INVALID` |
|        - | 12631 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 12632 | ` *   EXTR_IF_EXISTS` |
|        - | 12633 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 12634 | ` *       otherwise do nothing.` |
|        - | 12635 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 12636 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 12637 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 12638 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 12639 | ` *      the current symbol table.` |
|        - | 12640 | ` * $prefix` |
|        - | 12641 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 12642 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 12643 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 12644 | ` *  underscore character.` |
|        - | 12645 | ` * Return` |
|        - | 12646 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 12647 | ` */` |
|        4 | 12648 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12649 |  |
|        - | 12650 | `	extract_aux_data sAux;` |
|        - | 12651 | `	ph7_hashmap *pMap;` |
|        5 | 12652 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 12653 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 12654 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12655 | `		return PH7_OK;` |
|        - | 12656 | `	}` |
|        - | 12657 | `	/* Point to the target hashmap */` |
|        5 | 12658 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 12659 | `	if( pMap->nEntry < 1 ){` |
|        - | 12660 | `		/* Empty map,return  0 */` |
|      ! 0 | 12661 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12662 | `		return PH7_OK;` |
|        - | 12663 | `	}` |
|        - | 12664 | `	/* Prepare the aux data */` |
|        5 | 12665 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 12666 | `	if( nArg > 1 ){` |
|        3 | 12667 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 12668 | `		if( nArg > 2 ){` |
|      ! 0 | 12669 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 12670 | `		}` |
|        1 | 12671 | `	}` |
|        5 | 12672 | `	sAux.pVm = pCtx->pVm;` |
|        - | 12673 | `	/* Invoke the worker callback */` |
|        5 | 12674 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 12675 | `	/* Number of variables successfully imported */` |
|        5 | 12676 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 12677 | `	return PH7_OK;` |
|        3 | 12678 |  |
|        - | 12679 | `/*` |
|        - | 12680 | ` * Worker callback for the [extract()] function defined` |
|        - | 12681 | ` * below.` |
|        - | 12682 | ` */` |
|        8 | 12683 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12684 |  |
|        9 | 12685 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 12686 | `	int iFlags = pAux->iFlags;` |
|        9 | 12687 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12688 | `	ph7_value *pObj;` |
|        - | 12689 | `	SyString sVar;` |
|        9 | 12690 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 12691 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 12692 | `	}` |
|        - | 12693 | `	/* Perform a string cast */` |
|        9 | 12694 | `	PH7_MemObjToString(pKey);` |
|        9 | 12695 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12696 | `		/* Unavailable variable name */` |
|      ! 0 | 12697 | `		return SXRET_OK;` |
|        - | 12698 | `	}` |
|        9 | 12699 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 12700 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 12701 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12702 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12703 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12704 | `			);` |
|      ! 0 | 12705 | `	}else{` |
|       13 | 12706 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 12707 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12708 | `	}` |
|        9 | 12709 | `	sVar.zString = pAux->zWorker;` |
|        - | 12710 | `	/* Try to extract the variable */` |
|        9 | 12711 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 12712 | `	if( pObj ){` |
|        - | 12713 | `		/* Collision */` |
|        5 | 12714 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 12715 | `			return SXRET_OK;` |
|        - | 12716 | `		}` |
|        5 | 12717 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 12718 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 12719 | `				/* Already prefixed */` |
|      ! 0 | 12720 | `				return SXRET_OK;` |
|        - | 12721 | `			}` |
|      ! 0 | 12722 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12723 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12724 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12725 | `				);` |
|      ! 0 | 12726 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 12727 | `		}` |
|        3 | 12728 | `	}else{` |
|        - | 12729 | `		/* Create the variable */` |
|        5 | 12730 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 12731 | `	}` |
|        9 | 12732 | `	if( pObj ){` |
|        - | 12733 | `		/* Overwrite the old value */` |
|        9 | 12734 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 12735 | `		/* Increment counter */` |
|        9 | 12736 | `		pAux->iCount++;` |
|        4 | 12737 | `	}` |
|        9 | 12738 | `	return SXRET_OK;` |
|        5 | 12739 |  |
|        - | 12740 | `/*` |
|        - | 12741 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 12742 | ` * defined below.` |
|        - | 12743 | ` */` |
|        2 | 12744 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12745 |  |
|        3 | 12746 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 12747 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12748 | `	ph7_value *pObj;` |
|        - | 12749 | `	SyString sVar;` |
|        - | 12750 | `	/* Perform a string cast */` |
|        3 | 12751 | `	PH7_MemObjToString(pKey);` |
|        3 | 12752 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12753 | `		/* Unavailable variable name */` |
|      ! 0 | 12754 | `		return SXRET_OK;` |
|        - | 12755 | `	}` |
|        3 | 12756 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 12757 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 12758 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 12759 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 12760 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12761 | `			);` |
|        2 | 12762 | `	}else{` |
|      ! 0 | 12763 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 12764 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12765 | `	}` |
|        3 | 12766 | `	sVar.zString = pAux->zWorker;` |
|        - | 12767 | `	/* Extract the variable */` |
|        3 | 12768 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 12769 | `	if( pObj ){` |
|        3 | 12770 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 12771 | `	}` |
|        3 | 12772 | `	return SXRET_OK;` |
|        2 | 12773 |  |
|        - | 12774 | `/*` |
|        - | 12775 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 12776 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 12777 | ` * Parameters` |
|        - | 12778 | ` * $types` |
|        - | 12779 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 12780 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 12781 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 12782 | ` *  POST includes the POST uploaded file information.` |
|        - | 12783 | ` *  Note:` |
|        - | 12784 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 12785 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 12786 | ` * $prefix` |
|        - | 12787 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 12788 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 12789 | ` *  variable named $pref_userid.` |
|        - | 12790 | ` * Return` |
|        - | 12791 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12792 | ` */` |
|        2 | 12793 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12794 |  |
|        - | 12795 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 12796 | `	extract_aux_data sAux;` |
|        - | 12797 | `	int nLen,nPrefixLen;` |
|        - | 12798 | `	ph7_value *pSuper;` |
|        - | 12799 | `	ph7_vm *pVm;` |
|        - | 12800 | `	/* By default import only $_GET variables  */` |
|        3 | 12801 | `	zImport = "G";` |
|        3 | 12802 | `	nLen = (int)sizeof(char);` |
|        3 | 12803 | `	zPrefix = 0;` |
|        3 | 12804 | `	nPrefixLen = 0;` |
|        3 | 12805 | `	if( nArg > 0 ){` |
|        3 | 12806 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 12807 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 12808 | `		}` |
|        3 | 12809 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12810 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 12811 | `		}` |
|        1 | 12812 | `	}` |
|        - | 12813 | `	/* Point to the underlying VM */` |
|        3 | 12814 | `	pVm = pCtx->pVm;` |
|        - | 12815 | `	/* Initialize the aux data */` |
|        3 | 12816 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 12817 | `	sAux.zPrefix = zPrefix;` |
|        3 | 12818 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 12819 | `	sAux.pVm = pVm;` |
|        - | 12820 | `	/* Extract */` |
|        3 | 12821 | `	zEnd = &zImport[nLen];` |
|        5 | 12822 | `	while( zImport < zEnd ){` |
|        3 | 12823 | `		int c = zImport[0];` |
|        3 | 12824 | `		pSuper = 0;` |
|        3 | 12825 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 12826 | `			/* Import $_GET variables */` |
|        3 | 12827 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 12828 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 12829 | `			/* Import $_POST variables */` |
|      ! 0 | 12830 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 12831 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 12832 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 12833 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 12834 | `		}` |
|        3 | 12835 | `		if( pSuper ){` |
|        - | 12836 | `			/* Iterate throw array entries */` |
|        3 | 12837 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 12838 | `		}` |
|        - | 12839 | `		/* Advance the cursor */` |
|        3 | 12840 | `		zImport++;` |
|        1 | 12841 | `	}` |
|        - | 12842 | `	/* All done,return TRUE*/` |
|        3 | 12843 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12844 | `	return PH7_OK;` |
|        1 | 12845 |  |
|        - | 12846 | `/*` |
|        - | 12847 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 12848 | ` * Refer to the eval() language construct implementation for more` |
|        - | 12849 | ` * information.` |
|        - | 12850 | ` */` |
|    11542 | 12851 | `static sxi32 VmEvalChunk(` |
|        - | 12852 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 12853 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 12854 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 12855 | `	int iFlags,         /* Compile flag */` |
|        - | 12856 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 12857 | `	)` |
|        2 | 12858 |  |
|        - | 12859 | `	SySet *pByteCode,aByteCode;` |
|        - | 12860 | `	SyBlob sSavedNs;` |
|    11544 | 12861 | `	ProcConsumer xErr = 0;` |
|    11544 | 12862 | `	void *pErrData = 0;` |
|        - | 12863 | `	/* Initialize bytecode container */` |
|    11544 | 12864 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11544 | 12865 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 12866 | `	/* Reset the code generator */` |
|    11544 | 12867 | `	if( bTrueReturn ){` |
|        - | 12868 | `		/* Included file,log compile-time errors */` |
|     8686 | 12869 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8686 | 12870 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4342 | 12871 | `	}` |
|    11544 | 12872 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 12873 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 12874 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 12875 | `	 * the caller's namespace is restored. */` |
|    11544 | 12876 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11544 | 12877 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11544 | 12878 | `	if( bTrueReturn ){` |
|        - | 12879 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8686 | 12880 | `		SyBlobReset(&pVm->sNamespace);` |
|     4342 | 12881 | `	}` |
|        - | 12882 | `	/* Swap bytecode container */` |
|    11544 | 12883 | `	pByteCode = pVm->pByteContainer;` |
|    11544 | 12884 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 12885 | `	/* Compile the chunk */` |
|    11544 | 12886 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17315 | 12887 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 12888 | `		/* Compilation error,return false */` |
|        3 | 12889 | `		if( pCtx ){` |
|        3 | 12890 | `			ph7_result_bool(pCtx,0);` |
|        1 | 12891 | `		}` |
|        2 | 12892 | `	}else{` |
|        - | 12893 | `		/* Mount any newly defined classes */` |
|        - | 12894 | `		SyHashEntry *pEntry;` |
|        - | 12895 | `		ph7_class *pClass;` |
|        - | 12896 | `		ph7_value sResult; /* Return value */` |
|        - | 12897 | `		sxi32 rc;` |
|    11542 | 12898 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   478640 | 12899 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   461330 | 12900 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 12901 | `			/* Only mount classes that haven't been mounted yet */` |
|   461330 | 12902 | `			if( !pClass->bMounted ){` |
|    91096 | 12903 | `				rc = VmMountUserClass(pVm,pClass);` |
|    91096 | 12904 | `				if( rc != SXRET_OK ){` |
|        - | 12905 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 12906 | `					if( pCtx ){` |
|      ! 0 | 12907 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 12908 | `					}` |
|      ! 0 | 12909 | `					goto Cleanup;` |
|        - | 12910 | `				}` |
|    45547 | 12911 | `			}` |
|        2 | 12912 | `		}` |
|    11542 | 12913 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 12914 | `			/* Out of memory */` |
|      ! 0 | 12915 | `			if( pCtx ){` |
|      ! 0 | 12916 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 12917 | `			}` |
|      ! 0 | 12918 | `			goto Cleanup;` |
|        - | 12919 | `		}` |
|    11542 | 12920 | `		if( bTrueReturn ){` |
|        - | 12921 | `			/* Assume a boolean true return value */` |
|     8686 | 12922 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4344 | 12923 | `		}else{` |
|        - | 12924 | `			/* Assume a null return value */` |
|     2858 | 12925 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 12926 | `		}` |
|        - | 12927 | `		/* Execute the compiled chunk */` |
|    11542 | 12928 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11542 | 12929 | `		if( pCtx ){` |
|        - | 12930 | `			/* Set the execution result */` |
|     8704 | 12931 | `			ph7_result_value(pCtx,&sResult);` |
|     4351 | 12932 | `		}` |
|    11542 | 12933 | `		PH7_MemObjRelease(&sResult);` |
|        - | 12934 | `	}` |
|     5771 | 12935 | `Cleanup:` |
|        - | 12936 | `	/* Cleanup the mess left behind */` |
|    11544 | 12937 | `	pVm->pByteContainer = pByteCode;` |
|    11544 | 12938 | `	SySetRelease(&aByteCode);` |
|        - | 12939 | `	/* Restore caller's namespace state */` |
|    11544 | 12940 | `	SyBlobReset(&pVm->sNamespace);` |
|    11544 | 12941 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11544 | 12942 | `	SyBlobRelease(&sSavedNs);` |
|    11544 | 12943 | `	return SXRET_OK;` |
|        2 | 12944 |  |
|        - | 12945 | `/*` |
|        - | 12946 | ` * value eval(string $code)` |
|        - | 12947 | ` *   Evaluate a string as PHP code.` |
|        - | 12948 | ` * Parameter` |
|        - | 12949 | ` *  code: PHP code to evaluate.` |
|        - | 12950 | ` * Return` |
|        - | 12951 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 12952 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 12953 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 12954 | ` */` |
|       22 | 12955 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12956 |  |
|        - | 12957 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 12958 | `	if( nArg < 1 ){` |
|        - | 12959 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12960 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12961 | `		return SXRET_OK;` |
|        - | 12962 | `	}` |
|        - | 12963 | `	/* Chunk to evaluate */` |
|       24 | 12964 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 12965 | `	if( sChunk.nByte < 1 ){` |
|        - | 12966 | `		/* Empty string,return NULL */` |
|        3 | 12967 | `		ph7_result_null(pCtx);` |
|        3 | 12968 | `		return SXRET_OK;` |
|        - | 12969 | `	}` |
|        - | 12970 | `	/* Eval the chunk */` |
|       22 | 12971 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 12972 | `	return SXRET_OK;` |
|       13 | 12973 |  |
|        - | 12974 | `/*` |
|        - | 12975 | ` * Check if a file path is already included.` |
|        - | 12976 | ` */` |
|    17364 | 12977 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 12978 |  |
|        - | 12979 | `	SyString *aEntries;` |
|        - | 12980 | `	sxu32 n;` |
|    17366 | 12981 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 12982 | `	/* Perform a linear search */` |
| 75327438 | 12983 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 75310080 | 12984 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 12985 | `			/* Already included */` |
|        7 | 12986 | `			return TRUE;` |
|        - | 12987 | `		}` |
| 37655038 | 12988 | `	}` |
|    17360 | 12989 | `	return FALSE;` |
|     8684 | 12990 |  |
|        - | 12991 | `/*` |
|        - | 12992 | ` * Push a file path in the appropriate VM container.` |
|        - | 12993 | ` */` |
|    20194 | 12994 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 12995 |  |
|        - | 12996 | `	SyString sPath;` |
|        - | 12997 | `	char *zDup;` |
|        - | 12998 | `#ifdef __WINNT__` |
|        - | 12999 | `	char *zCur;` |
|        - | 13000 | `#endif` |
|        - | 13001 | `	sxi32 rc;` |
|    20196 | 13002 | `	if( nLen < 0 ){` |
|     2832 | 13003 | `		nLen = SyStrlen(zPath);` |
|     1415 | 13004 | `	}` |
|        - | 13005 | `	/* Duplicate the file path first */` |
|    20196 | 13006 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20196 | 13007 | `	if( zDup == 0 ){` |
|      ! 0 | 13008 | `		return SXERR_MEM;` |
|        - | 13009 | `	}` |
|        - | 13010 | `#ifdef __WINNT__` |
|        - | 13011 | `	/* Normalize path on windows` |
|        - | 13012 | `	 * Example:` |
|        - | 13013 | `	 *    Path/To/File.php` |
|        - | 13014 | `	 * becomes` |
|        - | 13015 | `	 *   path\to\file.php` |
|        - | 13016 | `	 */` |
|        2 | 13017 | `	zCur = zDup;` |
|        2 | 13018 | `	while( zCur[0] != 0 ){` |
|        2 | 13019 | `		if( zCur[0] == '/' ){` |
|        2 | 13020 | `			zCur[0] = '\\';` |
|        2 | 13021 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 13022 | `			int c = SyToLower(zCur[0]);` |
|        1 | 13023 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 13024 | `		}` |
|        2 | 13025 | `		zCur++;` |
|        2 | 13026 | `	}` |
|        - | 13027 | `#endif` |
|        - | 13028 | `	/* Install the file path */` |
|    20196 | 13029 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20196 | 13030 | `	if( !bMain ){` |
|    17366 | 13031 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 13032 | `			/* Already included */` |
|        7 | 13033 | `			*pNew = 0;` |
|        4 | 13034 | `		}else{` |
|        - | 13035 | `			/* Insert in the corresponding container */` |
|    17360 | 13036 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17360 | 13037 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13038 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 13039 | `				return rc;` |
|        - | 13040 | `			}` |
|    17360 | 13041 | `			*pNew = 1;` |
|        - | 13042 | `		}` |
|     8682 | 13043 | `	}` |
|    20196 | 13044 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20196 | 13045 | `	return SXRET_OK;` |
|    10099 | 13046 |  |
|        - | 13047 | `/*` |
|        - | 13048 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 13049 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 13050 | ` * indicates failure.` |
|        - | 13051 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 13052 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 13053 | ` * operations.` |
|        - | 13054 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 13055 | ` * this function is a no-op.` |
|        - | 13056 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 13057 | ` * constructs for more information.` |
|        - | 13058 | ` */` |
|     8694 | 13059 | `static sxi32 VmExecIncludedFile(` |
|        - | 13060 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 13061 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 13062 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 13063 | `	 )` |
|        2 | 13064 |  |
|        - | 13065 | `	sxi32 rc;` |
|        - | 13066 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13067 | `	const ph7_io_stream *pStream;` |
|        - | 13068 | `	SyBlob sContents;` |
|        - | 13069 | `	void *pHandle;` |
|        - | 13070 | `	ph7_vm *pVm;` |
|        - | 13071 | `	int isNew;` |
|        - | 13072 | `	/* Initialize fields */` |
|     8696 | 13073 | `	pVm = pCtx->pVm;` |
|     8696 | 13074 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8696 | 13075 | `	isNew = 0;` |
|        - | 13076 | `	/* Extract the associated stream */` |
|     8696 | 13077 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 13078 | `	/*` |
|        - | 13079 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 13080 | `	 * in a read-only mode.` |
|        - | 13081 | `	 */` |
|     8696 | 13082 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8696 | 13083 | `	if( pHandle == 0 ){` |
|        8 | 13084 | `		return SXERR_IO;` |
|        - | 13085 | `	}` |
|     8690 | 13086 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8690 | 13087 | `	if( IncludeOnce && !isNew ){` |
|        - | 13088 | `		/* Already included */` |
|        5 | 13089 | `		rc = SXERR_EXISTS;` |
|        3 | 13090 | `	}else{` |
|        - | 13091 | `		/* Read the whole file contents */` |
|     8686 | 13092 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8686 | 13093 | `		if( rc == SXRET_OK ){` |
|        - | 13094 | `			SyString sScript;` |
|        - | 13095 | `			/* Compile and execute the script */` |
|     8686 | 13096 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8686 | 13097 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4342 | 13098 | `		}` |
|        - | 13099 | `	}` |
|        - | 13100 | `	/* Pop from the set of included file */` |
|     8690 | 13101 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 13102 | `	/* Close the handle */` |
|     8690 | 13103 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 13104 | `	/* Release the working buffer */` |
|     8690 | 13105 | `	SyBlobRelease(&sContents);` |
|        - | 13106 | `#else` |
|        - | 13107 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 13108 | `	SXUNUSED(pPath);` |
|        - | 13109 | `	SXUNUSED(IncludeOnce);` |
|        - | 13110 | `	rc = SXERR_IO;` |
|        - | 13111 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8690 | 13112 | `	return rc;` |
|     4349 | 13113 |  |
|        - | 13114 | `/*` |
|        - | 13115 | ` * string get_include_path(void)` |
|        - | 13116 | ` *  Gets the current include_path configuration option.` |
|        - | 13117 | ` * Parameter` |
|        - | 13118 | ` *  None` |
|        - | 13119 | ` * Return` |
|        - | 13120 | ` *  Included paths as a string` |
|        - | 13121 | ` */` |
|        2 | 13122 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13123 |  |
|        3 | 13124 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13125 | `	SyString *aEntry;` |
|        - | 13126 | `	int dir_sep;` |
|        - | 13127 | `	sxu32 n;` |
|        - | 13128 | `#ifdef __WINNT__` |
|        1 | 13129 | `	dir_sep = ';';` |
|        - | 13130 | `#else` |
|        - | 13131 | `	/* Assume UNIX path separator */` |
|        2 | 13132 | `	dir_sep = ':';` |
|        - | 13133 | `#endif` |
|        1 | 13134 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13135 | `	SXUNUSED(apArg);` |
|        - | 13136 | `	/* Point to the list of import paths */` |
|        3 | 13137 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 13138 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 13139 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 13140 | `		if( n > 0 ){` |
|        - | 13141 | `			/* Append dir seprator */` |
|      ! 0 | 13142 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 13143 | `		}` |
|        - | 13144 | `		/* Append path */` |
|        3 | 13145 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 13146 | `	}` |
|        3 | 13147 | `	return PH7_OK;` |
|        1 | 13148 |  |
|        - | 13149 | `/*` |
|        - | 13150 | ` * string get_get_included_files(void)` |
|        - | 13151 | ` *  Gets the current include_path configuration option.` |
|        - | 13152 | ` * Parameter` |
|        - | 13153 | ` *  None` |
|        - | 13154 | ` * Return` |
|        - | 13155 | ` *  Included paths as a string` |
|        - | 13156 | ` */` |
|        2 | 13157 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13158 |  |
|        3 | 13159 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 13160 | `	ph7_value *pArray,*pWorker;` |
|        - | 13161 | `	SyString *pEntry;` |
|        - | 13162 | `	int c,d;` |
|        - | 13163 | `	/* Create an array and a working value */` |
|        3 | 13164 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 13165 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 13166 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 13167 | `		/* Out of memory,return null */` |
|      ! 0 | 13168 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13169 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13170 | `		SXUNUSED(apArg);` |
|      ! 0 | 13171 | `		return PH7_OK;` |
|        - | 13172 | `	}` |
|        3 | 13173 | `	c = d = '/';` |
|        - | 13174 | `#ifdef __WINNT__` |
|        1 | 13175 | `	d = '\\';` |
|        - | 13176 | `#endif` |
|        - | 13177 | `	/* Iterate throw entries */` |
|        3 | 13178 | `	SySetResetCursor(pFiles);` |
|     3839 | 13179 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 13180 | `		const char *zBase,*zEnd;` |
|        - | 13181 | `		int iLen;` |
|        - | 13182 | `		/* reset the string cursor */` |
|     3837 | 13183 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 13184 | `		/* Extract base name */` |
|     3837 | 13185 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 13186 | `		/* Ignore trailing '/' */` |
|     5755 | 13187 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 13188 | `			zEnd--;` |
|      ! 0 | 13189 | `		}` |
|     3837 | 13190 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 13191 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 13192 | `			zEnd--;` |
|        1 | 13193 | `		}` |
|     3837 | 13194 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 13195 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 13196 | `		/* Copy entry name */` |
|     3837 | 13197 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 13198 | `		/* Perform the insertion */` |
|     3837 | 13199 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 13200 | `	}` |
|        - | 13201 | `	/* All done,return the created array */` |
|        3 | 13202 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13203 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 13204 | `	 * by the engine as soon we return from this foreign` |
|        - | 13205 | `	 * function.` |
|        - | 13206 | `	 */` |
|        3 | 13207 | `	return PH7_OK;` |
|        2 | 13208 |  |
|        - | 13209 | `/*` |
|        - | 13210 | ` * include:` |
|        - | 13211 | ` * According to the PHP reference manual.` |
|        - | 13212 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 13213 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 13214 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 13215 | ` *  include() will finally check in the calling script's own directory` |
|        - | 13216 | ` *  and the current working directory before failing. The include()` |
|        - | 13217 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 13218 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 13219 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 13220 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 13221 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 13222 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 13223 | ` *  directory to find the requested file.` |
|        - | 13224 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 13225 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 13226 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 13227 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 13228 | ` */` |
|     8676 | 13229 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13230 |  |
|        - | 13231 | `	SyString sFile;` |
|        - | 13232 | `	sxi32 rc;` |
|     8678 | 13233 | `	if( nArg < 1 ){` |
|        - | 13234 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13235 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13236 | `		return SXRET_OK;` |
|        - | 13237 | `	}` |
|        - | 13238 | `	/* File to include */` |
|     8678 | 13239 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8678 | 13240 | `	if( sFile.nByte < 1 ){` |
|        - | 13241 | `		/* Empty string,return NULL */` |
|      ! 0 | 13242 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13243 | `		return SXRET_OK;` |
|        - | 13244 | `	}` |
|        - | 13245 | `	/* Open,compile and execute the desired script */` |
|     8678 | 13246 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8678 | 13247 | `	if( rc != SXRET_OK ){` |
|        - | 13248 | `		/* Emit a warning and return false */` |
|        3 | 13249 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 13250 | `		ph7_result_bool(pCtx,0);` |
|        1 | 13251 | `	}` |
|     8678 | 13252 | `	return SXRET_OK;` |
|     4340 | 13253 |  |
|        - | 13254 | `/*` |
|        - | 13255 | ` * include_once:` |
|        - | 13256 | ` *  According to the PHP reference manual.` |
|        - | 13257 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 13258 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 13259 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 13260 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 13261 | ` *   just once.` |
|        - | 13262 | ` */` |
|        4 | 13263 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13264 |  |
|        - | 13265 | `	SyString sFile;` |
|        - | 13266 | `	sxi32 rc;` |
|        5 | 13267 | `	if( nArg < 1 ){` |
|        - | 13268 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13269 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13270 | `		return SXRET_OK;` |
|        - | 13271 | `	}` |
|        - | 13272 | `	/* File to include */` |
|        5 | 13273 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13274 | `	if( sFile.nByte < 1 ){` |
|        - | 13275 | `		/* Empty string,return NULL */` |
|      ! 0 | 13276 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13277 | `		return SXRET_OK;` |
|        - | 13278 | `	}` |
|        - | 13279 | `	/* Open,compile and execute the desired script */` |
|        5 | 13280 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13281 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13282 | `		/* File already included,return TRUE */` |
|        3 | 13283 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13284 | `		return SXRET_OK;` |
|        - | 13285 | `	}` |
|        3 | 13286 | `	if( rc != SXRET_OK ){` |
|        - | 13287 | `		/* Emit a warning and return false */` |
|      ! 0 | 13288 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13289 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13290 | ` 	}` |
|        3 | 13291 | `	return SXRET_OK;` |
|        3 | 13292 |  |
|        - | 13293 | `/*` |
|        - | 13294 | ` * require.` |
|        - | 13295 | ` *  According to the PHP reference manual.` |
|        - | 13296 | ` *   require() is identical to include() except upon failure it will` |
|        - | 13297 | ` *   also produce a fatal level error.` |
|        - | 13298 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 13299 | ` *   emits a warning  which allows the script to continue.` |
|        - | 13300 | ` */` |
|        6 | 13301 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13302 |  |
|        - | 13303 | `	SyString sFile;` |
|        - | 13304 | `	sxi32 rc;` |
|        8 | 13305 | `	if( nArg < 1 ){` |
|        - | 13306 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13307 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13308 | `		return SXRET_OK;` |
|        - | 13309 | `	}` |
|        - | 13310 | `	/* File to include */` |
|        8 | 13311 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 13312 | `	if( sFile.nByte < 1 ){` |
|        - | 13313 | `		/* Empty string,return NULL */` |
|      ! 0 | 13314 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13315 | `		return SXRET_OK;` |
|        - | 13316 | `	}` |
|        - | 13317 | `	/* Open,compile and execute the desired script */` |
|        8 | 13318 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 13319 | `	if( rc != SXRET_OK ){` |
|        - | 13320 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13321 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13322 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13323 | `		return PH7_ABORT;` |
|        - | 13324 | `	}` |
|        8 | 13325 | `	return SXRET_OK;` |
|        5 | 13326 |  |
|        - | 13327 | `/*` |
|        - | 13328 | ` * require_once:` |
|        - | 13329 | ` *  According to the PHP reference manual.` |
|        - | 13330 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 13331 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 13332 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 13333 | ` *   and how it differs from its non _once siblings.` |
|        - | 13334 | ` */` |
|        4 | 13335 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13336 |  |
|        - | 13337 | `	SyString sFile;` |
|        - | 13338 | `	sxi32 rc;` |
|        5 | 13339 | `	if( nArg < 1 ){` |
|        - | 13340 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13341 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13342 | `		return SXRET_OK;` |
|        - | 13343 | `	}` |
|        - | 13344 | `	/* File to include */` |
|        5 | 13345 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13346 | `	if( sFile.nByte < 1 ){` |
|        - | 13347 | `		/* Empty string,return NULL */` |
|      ! 0 | 13348 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13349 | `		return SXRET_OK;` |
|        - | 13350 | `	}` |
|        - | 13351 | `	/* Open,compile and execute the desired script */` |
|        5 | 13352 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13353 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13354 | `		/* File already included,return TRUE */` |
|        3 | 13355 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13356 | `		return SXRET_OK;` |
|        - | 13357 | `	}` |
|        3 | 13358 | `	if( rc != SXRET_OK ){` |
|        - | 13359 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13360 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13361 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13362 | `		return PH7_ABORT;` |
|        - | 13363 | `	}` |
|        3 | 13364 | `	return SXRET_OK;` |
|        3 | 13365 |  |
|        - | 13366 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 13367 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 13368 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 13369 | `/*` |
|        - | 13370 | ` * Section:` |
|        - | 13371 | ` *  SPL Autoloading functions.` |
|        - | 13372 | ` * Status:` |
|        - | 13373 | ` *  Stable.` |
|        - | 13374 | ` */` |
|        - | 13375 | `/*` |
|        - | 13376 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 13377 | ` *  Register given function as __autoload() implementation.` |
|        - | 13378 | ` * Parameters` |
|        - | 13379 | ` *  callback` |
|        - | 13380 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 13381 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 13382 | ` *  throw` |
|        - | 13383 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 13384 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 13385 | ` *  prepend` |
|        - | 13386 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 13387 | ` *   autoload stack instead of appending it.` |
|        - | 13388 | ` * Return` |
|        - | 13389 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13390 | ` */` |
|       34 | 13391 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13392 |  |
|        - | 13393 | `	VmAutoloadCB sEntry;` |
|       36 | 13394 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 13395 | `	int iPrepend = 0;` |
|        - | 13396 | `	sxu32 n;` |
|       36 | 13397 | `	if( nArg < 1 ){` |
|        - | 13398 | `		/* No callback provided — register default spl_autoload.` |
|        - | 13399 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 13400 | `		/* Check for duplicates first */` |
|        9 | 13401 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 13402 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 13403 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 13404 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 13405 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 13406 | `				ph7_result_bool(pCtx,1);` |
|        5 | 13407 | `				return SXRET_OK;` |
|        - | 13408 | `			}` |
|      ! 0 | 13409 | `		}` |
|        5 | 13410 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 13411 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 13412 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 13413 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 13414 | `		ph7_result_bool(pCtx,1);` |
|        5 | 13415 | `		return SXRET_OK;` |
|        - | 13416 | `	}` |
|        - | 13417 | `	/* Validate that the callback is callable */` |
|       28 | 13418 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 13419 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 13420 | `		if( nArg >= 2 ){` |
|      ! 0 | 13421 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13422 | `		}` |
|      ! 0 | 13423 | `		if( iThrow ){` |
|      ! 0 | 13424 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 13425 | `				"Argument is not callable");` |
|      ! 0 | 13426 | `		}` |
|      ! 0 | 13427 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13428 | `		return SXRET_OK;` |
|        - | 13429 | `	}` |
|        - | 13430 | `	/* Check for duplicates */` |
|       46 | 13431 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 13432 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 13433 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13434 | `			/* Already registered */` |
|      ! 0 | 13435 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13436 | `			return SXRET_OK;` |
|        - | 13437 | `		}` |
|       11 | 13438 | `	}` |
|        - | 13439 | `	/* Check prepend flag */` |
|       28 | 13440 | `	if( nArg >= 3 ){` |
|        3 | 13441 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 13442 | `	}` |
|        - | 13443 | `	/* Store the callback */` |
|       28 | 13444 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 13445 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 13446 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 13447 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 13448 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 13449 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 13450 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 13451 | `		VmAutoloadCB *aBase;` |
|        3 | 13452 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13453 | `		/* Rotate: move last entry to front */` |
|        3 | 13454 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 13455 | `		if( aBase ){` |
|        - | 13456 | `			VmAutoloadCB sTemp;` |
|        - | 13457 | `			sxu32 i;` |
|        3 | 13458 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 13459 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 13460 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 13461 | `			}` |
|        3 | 13462 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 13463 | `		}` |
|        2 | 13464 | `	}else{` |
|       26 | 13465 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13466 | `	}` |
|       28 | 13467 | `	ph7_result_bool(pCtx,1);` |
|       28 | 13468 | `	return SXRET_OK;` |
|       19 | 13469 |  |
|        - | 13470 | `/*` |
|        - | 13471 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 13472 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 13473 | ` * Parameters` |
|        - | 13474 | ` *  callback` |
|        - | 13475 | ` *   The autoload function being unregistered.` |
|        - | 13476 | ` * Return` |
|        - | 13477 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13478 | ` */` |
|       32 | 13479 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13480 |  |
|       34 | 13481 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13482 | `	sxu32 n,nEntry;` |
|       34 | 13483 | `	if( nArg < 1 ){` |
|      ! 0 | 13484 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13485 | `		return SXRET_OK;` |
|        - | 13486 | `	}` |
|       34 | 13487 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13488 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 13489 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 13490 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13491 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 13492 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 13493 | `			sxu32 i;` |
|       32 | 13494 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 13495 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 13496 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 13497 | `			}` |
|        - | 13498 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 13499 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 13500 | `			ph7_result_bool(pCtx,1);` |
|       32 | 13501 | `			return SXRET_OK;` |
|        - | 13502 | `		}` |
|        3 | 13503 | `	}` |
|        3 | 13504 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13505 | `	return SXRET_OK;` |
|       18 | 13506 |  |
|        - | 13507 | `/*` |
|        - | 13508 | ` * array spl_autoload_functions(void)` |
|        - | 13509 | ` *  Return all registered __autoload() functions.` |
|        - | 13510 | ` * Return` |
|        - | 13511 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 13512 | ` *  an empty array is returned.` |
|        - | 13513 | ` */` |
|       20 | 13514 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13515 |  |
|       21 | 13516 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13517 | `	ph7_value *pArray;` |
|        - | 13518 | `	sxu32 n,nEntry;` |
|       10 | 13519 | `	SXUNUSED(nArg);` |
|       10 | 13520 | `	SXUNUSED(apArg);` |
|       21 | 13521 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 13522 | `	if( pArray == 0 ){` |
|      ! 0 | 13523 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13524 | `		return SXRET_OK;` |
|        - | 13525 | `	}` |
|       21 | 13526 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 13527 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 13528 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 13529 | `		if( pEntry ){` |
|       15 | 13530 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 13531 | `		}` |
|        8 | 13532 | `	}` |
|       21 | 13533 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 13534 | `	return SXRET_OK;` |
|       11 | 13535 |  |
|        - | 13536 | `/*` |
|        - | 13537 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 13538 | ` *  Default implementation of __autoload().` |
|        - | 13539 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 13540 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 13541 | ` * Parameters` |
|        - | 13542 | ` *  class` |
|        - | 13543 | ` *   The class name being searched.` |
|        - | 13544 | ` *  file_extensions` |
|        - | 13545 | ` *   Comma-separated list of file extensions to try.` |
|        - | 13546 | ` */` |
|        2 | 13547 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13548 |  |
|        - | 13549 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 13550 | `	SyBlob sPath;` |
|        - | 13551 | `	int nClass;` |
|        - | 13552 | `	sxi32 rc;` |
|        3 | 13553 | `	if( nArg < 1 ){` |
|      ! 0 | 13554 | `		return SXRET_OK;` |
|        - | 13555 | `	}` |
|        3 | 13556 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 13557 | `	if( nClass < 1 ){` |
|      ! 0 | 13558 | `		return SXRET_OK;` |
|        - | 13559 | `	}` |
|        - | 13560 | `	/* Default extensions */` |
|        3 | 13561 | `	zExt = ".php,.inc";` |
|        3 | 13562 | `	if( nArg >= 2 ){` |
|        - | 13563 | `		int nExt;` |
|      ! 0 | 13564 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 13565 | `		if( nExt < 1 ){` |
|      ! 0 | 13566 | `			zExt = ".php,.inc";` |
|      ! 0 | 13567 | `		}` |
|      ! 0 | 13568 | `	}` |
|        3 | 13569 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 13570 | `	/* Iterate over comma-separated extensions */` |
|        3 | 13571 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 13572 | `	zCur = zExt;` |
|        7 | 13573 | `	while( zCur < zEnd ){` |
|        - | 13574 | `		const char *zComma;` |
|        - | 13575 | `		SyString sFile;` |
|        - | 13576 | `		int i;` |
|        - | 13577 | `		/* Find next comma or end */` |
|        5 | 13578 | `		zComma = zCur;` |
|       21 | 13579 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 13580 | `			zComma++;` |
|        1 | 13581 | `		}` |
|        - | 13582 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 13583 | `		SyBlobReset(&sPath);` |
|       69 | 13584 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 13585 | `			char c = zClass[i];` |
|       65 | 13586 | `			if( c == '\\' ){` |
|      ! 0 | 13587 | `				c = '/';` |
|       65 | 13588 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 13589 | `				c = c + ('a' - 'A');` |
|        6 | 13590 | `			}` |
|       65 | 13591 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 13592 | `		}` |
|        - | 13593 | `		/* Append extension */` |
|        5 | 13594 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 13595 | `		/* Try to include the file */` |
|        5 | 13596 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 13597 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 13598 | `		if( rc == SXRET_OK ){` |
|        - | 13599 | `			/* File included successfully */` |
|      ! 0 | 13600 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 13601 | `			return SXRET_OK;` |
|        - | 13602 | `		}` |
|        - | 13603 | `		/* Move past the comma */` |
|        5 | 13604 | `		zCur = zComma;` |
|        5 | 13605 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 13606 | `			zCur++;` |
|        1 | 13607 | `		}` |
|        1 | 13608 | `	}` |
|        3 | 13609 | `	SyBlobRelease(&sPath);` |
|        3 | 13610 | `	return SXRET_OK;` |
|        2 | 13611 |  |
|        - | 13612 | `/* Table of built-in VM functions. */` |
|        - | 13613 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13614 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13615 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13616 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13617 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13618 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13619 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13620 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13621 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13622 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13623 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13624 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13625 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13626 | `	    /* Constants management */` |
|        - | 13627 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13628 | `	{ "define",   vm_builtin_define               },` |
|        - | 13629 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13630 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13631 | `	   /* Class/Object functions */` |
|        - | 13632 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13633 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13634 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13635 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13636 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13637 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13638 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13639 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13640 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13641 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13642 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13643 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13644 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13645 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13646 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13647 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13648 | `	   /* SPL Autoloading */` |
|        - | 13649 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 13650 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 13651 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 13652 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 13653 | `	   /* Random numbers/strings generators */` |
|        - | 13654 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13655 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13656 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13657 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13658 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13659 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13660 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13661 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13662 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13663 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13664 | `	   /* Language constructs functions */` |
|        - | 13665 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13666 | `	{ "print", vm_builtin_print                   },` |
|        - | 13667 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13668 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13669 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13670 | `	  /* Variable handling functions */` |
|        - | 13671 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13672 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13673 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13674 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13675 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13676 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13677 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13678 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13679 | `	  /* Ouput control functions */` |
|        - | 13680 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13681 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13682 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13683 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13684 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13685 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13686 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13687 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13688 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13689 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13690 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13691 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13692 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13693 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13694 | `	  /* Assertion functions */` |
|        - | 13695 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13696 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13697 | `	  /* Error reporting functions */` |
|        - | 13698 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13699 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13700 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13701 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13702 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13703 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13704 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13705 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13706 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13707 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13708 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13709 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13710 | `	  /* Release info */` |
|        - | 13711 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13712 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13713 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13714 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13715 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13716 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13717 | `	  /* hashmap */` |
|        - | 13718 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13719 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13720 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13721 | `	  /* URL related function */` |
|        - | 13722 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13723 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13724 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13725 | `	   /* XML processing functions */` |
|        - | 13726 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13727 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13728 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13729 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13730 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13731 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13732 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13733 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13734 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13735 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13736 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13737 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13738 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13739 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13740 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13741 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13742 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13743 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13744 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13745 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13746 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13747 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13748 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13749 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13750 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13751 | `	   /* Command line processing */` |
|        - | 13752 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13753 | `	   /* JSON encoding/decoding */` |
|        - | 13754 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13755 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13756 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13757 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13758 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13759 | `	   /* Files/URI inclusion facility */` |
|        - | 13760 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13761 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13762 | `	{ "include",      vm_builtin_include          },` |
|        - | 13763 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13764 | `	{ "require",      vm_builtin_require          },` |
|        - | 13765 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13766 | `};` |
|        - | 13767 | `/*` |
|        - | 13768 | ` * Register the built-in VM functions defined above.` |
|        - | 13769 | ` */` |
|     2554 | 13770 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13771 |  |
|        - | 13772 | `	sxi32 rc;` |
|        - | 13773 | `	sxu32 n;` |
|   329468 | 13774 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13775 | `		/* Note that these special functions have access` |
|        - | 13776 | `		 * to the underlying virtual machine as their` |
|        - | 13777 | `		 * private data.` |
|        - | 13778 | `		 */` |
|   326914 | 13779 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   326914 | 13780 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13781 | `			return rc;` |
|        - | 13782 | `		}` |
|   163458 | 13783 | `	}` |
|     2556 | 13784 | `	return SXRET_OK;` |
|     1279 | 13785 |  |
|        - | 13786 | `/*` |
|        - | 13787 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 13788 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 13789 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 13790 | ` */` |
|    36196 | 13791 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 13792 |  |
|    36198 | 13793 | `	if( !iLoadable ){` |
|    34538 | 13794 | `		return pClass;` |
|        - | 13795 | `	}` |
|     1662 | 13796 | `	while(pClass){` |
|     1662 | 13797 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1662 | 13798 | `			return pClass;` |
|        - | 13799 | `		}` |
|      ! 0 | 13800 | `		pClass = pClass->pNextName;` |
|      ! 0 | 13801 | `	}` |
|      ! 0 | 13802 | `	return 0;` |
|    18100 | 13803 |  |
|        - | 13804 | `/*` |
|        - | 13805 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 13806 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 13807 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 13808 | ` * registered in the VM's class table.` |
|        - | 13809 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 13810 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 13811 | ` */` |
|       36 | 13812 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13813 |  |
|        - | 13814 | `	VmAutoloadCB *pEntry;` |
|        - | 13815 | `	ph7_value sArg,sResult;` |
|        - | 13816 | `	SyHashEntry *pHashEntry;` |
|        - | 13817 | `	ph7_class *pClass;` |
|        - | 13818 | `	sxu32 n,nEntry;` |
|       38 | 13819 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13820 | `	if( nEntry < 1 ){` |
|       24 | 13821 | `		return 0;` |
|        - | 13822 | `	}` |
|        - | 13823 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 13824 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 13825 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 13826 | `	}` |
|        - | 13827 | `	/* Mark this class as being autoloaded */` |
|       14 | 13828 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 13829 | `	/* Prepare the class name argument */` |
|       14 | 13830 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 13831 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 13832 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 13833 | `	pClass = 0;` |
|       28 | 13834 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 13835 | `		ph7_value *apArg[1];` |
|       24 | 13836 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 13837 | `		if( pEntry == 0 ){` |
|      ! 0 | 13838 | `			continue;` |
|        - | 13839 | `		}` |
|       24 | 13840 | `		apArg[0] = &sArg;` |
|       24 | 13841 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 13842 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 13843 | `			continue;` |
|        - | 13844 | `		}` |
|        - | 13845 | `		/* Check if the class is now available */` |
|       24 | 13846 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 13847 | `		if( pHashEntry ){` |
|       10 | 13848 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 13849 | `			if( pClass ){` |
|       10 | 13850 | `				break;` |
|        - | 13851 | `			}` |
|      ! 0 | 13852 | `		}` |
|        9 | 13853 | `	}` |
|       14 | 13854 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 13855 | `	PH7_MemObjRelease(&sResult);` |
|        - | 13856 | `	/* Remove reentrancy guard */` |
|       14 | 13857 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 13858 | `	return pClass;` |
|       20 | 13859 |  |
|        - | 13860 | `/*` |
|        - | 13861 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 13862 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 13863 | ` */` |
|       18 | 13864 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13865 |  |
|       20 | 13866 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 13867 |  |
|        - | 13868 | `/*` |
|        - | 13869 | ` * Check if the given name refer to an installed class.` |
|        - | 13870 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13871 | ` */` |
|    36206 | 13872 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13873 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13874 | `	const char *zName,  /* Name of the target class */` |
|        - | 13875 | `	sxu32 nByte,        /* zName length */` |
|        - | 13876 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13877 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13878 | `						 */` |
|        - | 13879 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13880 | `	)` |
|        2 | 13881 |  |
|        - | 13882 | `	SyHashEntry *pEntry;` |
|        - | 13883 | `	ph7_class *pClass;` |
|    18103 | 13884 | `	SXUNUSED(iNest);` |
|        - | 13885 | `	/* Exact class lookup.` |
|        - | 13886 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 13887 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    36208 | 13888 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    36208 | 13889 | `	if( pEntry == 0 ){` |
|        - | 13890 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 13891 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 13892 | `	}` |
|    36190 | 13893 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    36190 | 13894 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    18105 | 13895 |  |
|        - | 13896 | `/*` |
|        - | 13897 | ` * Reference Table Implementation` |
|        - | 13898 | ` * Status: stable <chm@symisc.net>` |
|        - | 13899 | ` * Intro` |
|        - | 13900 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 13901 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 13902 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 13903 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 13904 | ` *  Refer to the official for more information on this powerful` |
|        - | 13905 | ` *  extension.` |
|        - | 13906 | ` */` |
|        - | 13907 | `/*` |
|        - | 13908 | ` * Allocate a new reference entry.` |
|        - | 13909 | ` */` |
|  3103286 | 13910 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 13911 |  |
|        - | 13912 | `	VmRefObj *pRef;` |
|        - | 13913 | `	/* Allocate a new instance */` |
|  3103288 | 13914 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3103288 | 13915 | `	if( pRef == 0 ){` |
|      ! 0 | 13916 | `		return 0;` |
|        - | 13917 | `	}` |
|        - | 13918 | `	/* Zero the structure */` |
|  3103288 | 13919 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 13920 | `	/* Initialize fields */` |
|  3103288 | 13921 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3103288 | 13922 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3103288 | 13923 | `	pRef->nIdx = nIdx;` |
|  3103288 | 13924 | `	return pRef;` |
|  1551645 | 13925 |  |
|        - | 13926 | `/*` |
|        - | 13927 | ` * Default hash function used by the reference table` |
|        - | 13928 | ` * for lookup/insertion operations.` |
|        - | 13929 | ` */` |
| 17098694 | 13930 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 13931 |  |
|        - | 13932 | `	/* Calculate the hash based on the memory object index */` |
| 17098696 | 13933 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 13934 |  |
|        - | 13935 | `/*` |
|        - | 13936 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 13937 | ` * in the reference table.` |
|        - | 13938 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 13939 | ` * otherwise.` |
|        - | 13940 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13941 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13942 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13943 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13944 | ` * Refer to the official for more information on this powerful` |
|        - | 13945 | ` * extension.` |
|        - | 13946 | ` */` |
|  9258146 | 13947 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 13948 |  |
|        - | 13949 | `	VmRefObj *pRef;` |
|        - | 13950 | `	sxu32 nBucket;` |
|        - | 13951 | `	/* Point to the appropriate bucket */` |
|  9258148 | 13952 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 13953 | `	/* Perform the lookup */` |
|  9258148 | 13954 | `	pRef = pVm->apRefObj[nBucket];` |
| 20144207 | 13955 | `	for(;;){` |
| 40278580 | 13956 | `		if( pRef == 0 ){` |
|  3192348 | 13957 | `			break;` |
|        - | 13958 | `		}` |
| 37086234 | 13959 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 13960 | `			/* Entry found */` |
|  6065802 | 13961 | `			return pRef;` |
|        - | 13962 | `		}` |
|        - | 13963 | `		/* Point to the next entry */` |
| 31020434 | 13964 | `		pRef = pRef->pNextCollide;` |
|        2 | 13965 | `	}` |
|        - | 13966 | `	/* No such entry,return NULL */` |
|  3192348 | 13967 | `	return 0;` |
|  4629075 | 13968 |  |
|        - | 13969 | `/*` |
|        - | 13970 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13971 | ` *` |
|        - | 13972 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13973 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13974 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13975 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13976 | ` * Refer to the official for more information on this powerful` |
|        - | 13977 | ` * extension.` |
|        - | 13978 | ` */` |
|  3103286 | 13979 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13980 |  |
|        - | 13981 | `	sxu32 nBucket;` |
|  3103288 | 13982 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 13983 | `		VmRefObj **apNew;` |
|        - | 13984 | `		sxu32 nNew;` |
|        - | 13985 | `		/* Allocate a larger table */` |
|     4350 | 13986 | `		nNew = pVm->nRefSize << 1;` |
|     4350 | 13987 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4350 | 13988 | `		if( apNew ){` |
|     4350 | 13989 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 13990 | `			sxu32 n;` |
|        - | 13991 | `			/* Zero the structure */` |
|     4350 | 13992 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 13993 | `			/* Rehash all referenced entries */` |
|  2844514 | 13994 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 13995 | `				/* Remove old collision links */` |
|  2840166 | 13996 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 13997 | `				/* Point to the appropriate bucket */` |
|  2840166 | 13998 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 13999 | `				/* Insert the entry  */` |
|  2840166 | 14000 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2840166 | 14001 | `				if( apNew[nBucket] ){` |
|  2298896 | 14002 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14003 | `				}` |
|  2840166 | 14004 | `				apNew[nBucket] = pEntry;` |
|        - | 14005 | `				/* Point to the next entry */` |
|  2840166 | 14006 | `				pEntry = pEntry->pNext;` |
|  1420084 | 14007 | `			}` |
|        - | 14008 | `			/* Release the old table */` |
|     4350 | 14009 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14010 | `			/* Install the new one */` |
|     4350 | 14011 | `			pVm->apRefObj = apNew;` |
|     4350 | 14012 | `			pVm->nRefSize = nNew;` |
|     2174 | 14013 | `		}` |
|     2174 | 14014 | `	}` |
|        - | 14015 | `	/* Point to the appropriate bucket */` |
|  3103288 | 14016 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14017 | `	/* Insert the entry */` |
|  3103288 | 14018 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3103288 | 14019 | `	if( pVm->apRefObj[nBucket] ){` |
|  2551669 | 14020 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1275860 | 14021 | `	}` |
|  3103288 | 14022 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3103288 | 14023 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3103288 | 14024 | `	pVm->nRefUsed++;` |
|  3103288 | 14025 | `	return SXRET_OK;` |
|        2 | 14026 |  |
|        - | 14027 | `/*` |
|        - | 14028 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14029 | ` * the reference table.` |
|        - | 14030 | ` * This function is invoked when the user perform an unset` |
|        - | 14031 | ` * call [i.e: unset($var); ].` |
|        - | 14032 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14033 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14034 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14035 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14036 | ` * Refer to the official for more information on this powerful` |
|        - | 14037 | ` * extension.` |
|        - | 14038 | ` */` |
|  3066474 | 14039 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14040 |  |
|        - | 14041 | `	ph7_hashmap_node **apNode;` |
|        - | 14042 | `	SyHashEntry **apEntry;` |
|        - | 14043 | `	sxu32 n;` |
|        - | 14044 | `	/* Point to the reference table */` |
|  3066476 | 14045 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3066476 | 14046 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14047 | `	/* Unlink the entry from the reference table */` |
|  3161880 | 14048 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    95406 | 14049 | `		if( apEntry[n] ){` |
|    95356 | 14050 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    47677 | 14051 | `		}` |
|    47704 | 14052 | `	}` |
|  6039970 | 14053 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2973496 | 14054 | `		if( apNode[n] ){` |
|     7272 | 14055 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3635 | 14056 | `		}` |
|  1486749 | 14057 | `	}` |
|  3066476 | 14058 | `	if( pRef->pPrevCollide ){` |
|  1169378 | 14059 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   584765 | 14060 | `	}else{` |
|  1897100 | 14061 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14062 | `	}` |
|  3066476 | 14063 | `	if( pRef->pNextCollide ){` |
|  1739273 | 14064 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   869636 | 14065 | `	}` |
|  3066476 | 14066 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14067 | `	/* Release the node */` |
|  3066476 | 14068 | `	SySetRelease(&pRef->aReference);` |
|  3066476 | 14069 | `	SySetRelease(&pRef->aArrEntries);` |
|  3066476 | 14070 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3066476 | 14071 | `	pVm->nRefUsed--;` |
|  3066476 | 14072 | `	return SXRET_OK;` |
|        2 | 14073 |  |
|        - | 14074 | `/*` |
|        - | 14075 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14076 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14077 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14078 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14079 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14080 | ` * Refer to the official for more information on this powerful` |
|        - | 14081 | ` * extension.` |
|        - | 14082 | ` */` |
|  3136324 | 14083 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14084 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14085 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14086 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14087 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14088 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14089 | `	)` |
|        2 | 14090 |  |
|  3136326 | 14091 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14092 | `	VmRefObj *pRef;` |
|        - | 14093 | `	/* Check if the referenced object already exists */` |
|  3136326 | 14094 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3136326 | 14095 | `	if( pRef == 0 ){` |
|        - | 14096 | `		/* Create a new entry */` |
|  3103288 | 14097 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3103288 | 14098 | `		if( pRef == 0 ){` |
|      ! 0 | 14099 | `			return SXERR_MEM;` |
|        - | 14100 | `		}` |
|  3103288 | 14101 | `		pRef->iFlags = iFlags;` |
|        - | 14102 | `		/* Install the entry */` |
|  3103288 | 14103 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1551643 | 14104 | `	}` |
|  3136326 | 14105 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3136326 | 14106 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14107 | `		VmSlot sRef;` |
|        - | 14108 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14109 | `		 * be deleted when we leave this frame.` |
|        - | 14110 | `		 */` |
|    89156 | 14111 | `		sRef.nIdx = nIdx;` |
|    89156 | 14112 | `		sRef.pUserData = pEntry;` |
|    89156 | 14113 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14114 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14115 | `		}` |
|    44577 | 14116 | `	}` |
|  3136326 | 14117 | `	if( pEntry ){` |
|        - | 14118 | `		/* Address of the hash-entry */` |
|   121996 | 14119 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    60997 | 14120 | `	}` |
|  3136326 | 14121 | `	if( pMapEntry ){` |
|        - | 14122 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3008344 | 14123 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1504171 | 14124 | `	}` |
|  3136326 | 14125 | `	return SXRET_OK;` |
|  1568164 | 14126 |  |
|        - | 14127 | `/*` |
|        - | 14128 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14129 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14130 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14131 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14132 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14133 | ` * Refer to the official for more information on this powerful` |
|        - | 14134 | ` * extension.` |
|        - | 14135 | ` */` |
|  3055342 | 14136 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14137 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14138 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14139 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14140 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14141 | `	)` |
|        2 | 14142 |  |
|        - | 14143 | `	VmRefObj *pRef;` |
|        - | 14144 | `	sxu32 n;` |
|        - | 14145 | `	/* Check if the referenced object already exists */` |
|  3055344 | 14146 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3055344 | 14147 | `	if( pRef == 0 ){` |
|        - | 14148 | `		/* Not such entry */` |
|    89056 | 14149 | `		return SXERR_NOTFOUND;` |
|        - | 14150 | `	}` |
|        - | 14151 | `	/* Remove the desired entry */` |
|  2966290 | 14152 | `	if( pEntry ){` |
|        - | 14153 | `		SyHashEntry **apEntry;` |
|       60 | 14154 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      222 | 14155 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      164 | 14156 | `			if( apEntry[n] == pEntry ){` |
|        - | 14157 | `				/* Nullify the entry */` |
|       60 | 14158 | `				apEntry[n] = 0;` |
|        - | 14159 | `				/*` |
|        - | 14160 | `				 * NOTE:` |
|        - | 14161 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14162 | `				 * we avoid wasting spaces.` |
|        - | 14163 | `				 */` |
|       29 | 14164 | `			}` |
|       83 | 14165 | `		}` |
|       29 | 14166 | `	}` |
|  2966290 | 14167 | `	if( pMapEntry ){` |
|        - | 14168 | `		ph7_hashmap_node **apNode;` |
|  2966232 | 14169 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5932556 | 14170 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2966326 | 14171 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14172 | `				/* nullify the entry */` |
|  2966232 | 14173 | `				apNode[n] = 0;` |
|  1483115 | 14174 | `			}` |
|  1483164 | 14175 | `		}` |
|  1483115 | 14176 | `	}` |
|  2966290 | 14177 | `	return SXRET_OK;` |
|  1527673 | 14178 |  |
|        - | 14179 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14180 | `/*` |
|        - | 14181 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14182 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14183 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14184 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14185 | ` * For more information on how to register IO stream devices,please` |
|        - | 14186 | ` * refer to the official documentation.` |
|        - | 14187 | ` */` |
|    26408 | 14188 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14189 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14190 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14191 | `	int nByte              /* *pzDevice length*/` |
|        - | 14192 | `	)` |
|        2 | 14193 |  |
|        - | 14194 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14195 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14196 | `	SyString sDev,sCur;` |
|        - | 14197 | `	sxu32 n,nEntry;` |
|        - | 14198 | `	int rc;` |
|        - | 14199 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    26410 | 14200 | `	zNext = zCur = zIn = *pzDevice;` |
|    26410 | 14201 | `	zEnd = &zIn[nByte];` |
|  1678050 | 14202 | `	while( zIn < zEnd ){` |
|  1651644 | 14203 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14204 | `			/* Got one */` |
|        3 | 14205 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14206 | `			break;` |
|        - | 14207 | `		}` |
|        - | 14208 | `		/* Advance the cursor */` |
|  1651642 | 14209 | `		zIn++;` |
|        2 | 14210 | `	}` |
|    26410 | 14211 | `	if( zIn >= zEnd ){` |
|        - | 14212 | `		/* No such scheme,return the default stream */` |
|    26408 | 14213 | `		return pVm->pDefStream;` |
|        - | 14214 | `	}` |
|        3 | 14215 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14216 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14217 | `	SyStringFullTrim(&sDev);` |
|        - | 14218 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14219 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14220 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14221 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14222 | `		pStream = apStream[n];` |
|        3 | 14223 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14224 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14225 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14226 | `		if( rc == 0 ){` |
|        - | 14227 | `			/* Stream device found */` |
|        3 | 14228 | `			*pzDevice = zNext;` |
|        3 | 14229 | `			return pStream;` |
|        - | 14230 | `		}` |
|      ! 0 | 14231 | `	}` |
|        - | 14232 | `	/* No such stream,return NULL */` |
|      ! 0 | 14233 | `	return 0;` |
|    13206 | 14234 |  |
|        - | 14235 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14236 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 14237 |  |
