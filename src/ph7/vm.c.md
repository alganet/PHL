# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3892/5130 lines (75.87%)

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
|   799744 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   799746 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       27 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   799720 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   799712 |    94 | `	return FALSE;` |
|   399896 |    95 |  |
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
|   390686 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   390688 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   390688 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   390684 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   390684 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   390684 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   390684 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   390684 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   390684 |   142 | `	pCons->xExpand = xExpand;` |
|   390684 |   143 | `	pCons->pUserData = pUserData;` |
|   390684 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   390684 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   390684 |   151 | `	return SXRET_OK;` |
|   195345 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   841290 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   841292 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   841292 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   841292 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   841292 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   841292 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   841292 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   841292 |   185 | `	pFunc->pVm   = pVm;` |
|   841292 |   186 | `	pFunc->xFunc = xFunc;` |
|   841292 |   187 | `	pFunc->pUserData = pUserData;` |
|   841292 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   841292 |   190 | `	*ppOut = pFunc;` |
|   841292 |   191 | `	return SXRET_OK;` |
|   420647 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   843224 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   843226 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   843226 |   213 | `	if( pEntry ){` |
|     1936 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1936 |   215 | `		pFunc->pUserData = pUserData;` |
|     1936 |   216 | `		pFunc->xFunc = xFunc;` |
|     1936 |   217 | `		SySetReset(&pFunc->aAux);` |
|     1936 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   841292 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   841292 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   841292 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   841292 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   841292 |   233 | `	return SXRET_OK;` |
|   421614 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    91912 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    91914 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    91914 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    91914 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    91914 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    91914 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    91914 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    91914 |   260 | `	pFunc->iFlags = iFlags;` |
|    91914 |   261 | `	pFunc->pUserData = pUserData;` |
|    91914 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    91914 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   267 | ` */` |
|   332786 |   268 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   269 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   270 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   271 | `	SyString *pName     /* Function name */` |
|        - |   272 | `	)` |
|        2 |   273 |  |
|        - |   274 | `	SyHashEntry *pEntry;` |
|        - |   275 | `	sxi32 rc;` |
|   332788 |   276 | `	if( pName == 0 ){` |
|        - |   277 | `		/* Use the built-in name */` |
|    28710 |   278 | `		pName = &pFunc->sName;` |
|    14354 |   279 | `	}` |
|        - |   280 | `	/* Check for duplicates (functions with the same name) first */` |
|   332788 |   281 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   332788 |   282 | `	if( pEntry ){` |
|   258458 |   283 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   258458 |   284 | `		if( pLink != pFunc ){` |
|        - |   285 | `			/* Link */` |
|      179 |   286 | `			pFunc->pNextName = pLink;` |
|      179 |   287 | `			pEntry->pUserData = pFunc;` |
|       89 |   288 | `		}` |
|   258458 |   289 | `		return SXRET_OK;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* First time seen */` |
|    74332 |   292 | `	pFunc->pNextName = 0;` |
|    74332 |   293 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    74332 |   294 | `	return rc;` |
|   166395 |   295 |  |
|        - |   296 | `/*` |
|        - |   297 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   298 | ` */` |
|    26326 |   299 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   300 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   301 | `	ph7_class *pClass /* Target Class */` |
|        - |   302 | `	)` |
|        2 |   303 |  |
|    26328 |   304 | `	SyString *pName = &pClass->sName;` |
|        - |   305 | `	SyHashEntry *pEntry;` |
|        - |   306 | `	sxi32 rc;` |
|        - |   307 | `	/* Check for duplicates */` |
|    26328 |   308 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    26328 |   309 | `	if( pEntry ){` |
|       31 |   310 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   311 | `		/* Link entry with the same name */` |
|       31 |   312 | `		pClass->pNextName = pLink;` |
|       31 |   313 | `		pEntry->pUserData = pClass;` |
|       31 |   314 | `		return SXRET_OK;` |
|        - |   315 | `	}` |
|    26298 |   316 | `	pClass->pNextName = 0;` |
|        - |   317 | `	/* Perform a simple hashtable insertion */` |
|    26298 |   318 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    26298 |   319 | `	return rc;` |
|    13165 |   320 |  |
|        - |   321 | `/*` |
|        - |   322 | ` * Instruction builder interface.` |
|        - |   323 | ` */` |
|  2447598 |   324 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2447600 |   336 | `	sInstr.iOp = (sxu8)iOp;` |
|  2447600 |   337 | `	sInstr.iP1 = iP1;` |
|  2447600 |   338 | `	sInstr.iP2 = iP2;` |
|  2447600 |   339 | `	sInstr.p3  = p3;` |
|  2447600 |   340 | `	if( pIndex ){` |
|        - |   341 | `		/* Instruction index in the bytecode array */` |
|   156032 |   342 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    78015 |   343 | `	}` |
|        - |   344 | `	/* Finally,record the instruction */` |
|  2447600 |   345 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2447600 |   346 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   347 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   348 | `		/* Fall throw */` |
|      ! 0 |   349 | `	}` |
|  2447600 |   350 | `	return rc;` |
|        2 |   351 |  |
|        - |   352 | `/*` |
|        - |   353 | ` * Swap the current bytecode container with the given one.` |
|        - |   354 | ` */` |
|   223416 |   355 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   356 |  |
|   223418 |   357 | `	if( pContainer == 0 ){` |
|        - |   358 | `		/* Point to the default container */` |
|      ! 0 |   359 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   360 | `	}else{` |
|        - |   361 | `		/* Change container */` |
|   223418 |   362 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   363 | `	}` |
|   223418 |   364 | `	return SXRET_OK;` |
|        2 |   365 |  |
|        - |   366 | `/*` |
|        - |   367 | ` * Return the current bytecode container.` |
|        - |   368 | ` */` |
|   111708 |   369 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   370 |  |
|   111710 |   371 | `	return pVm->pByteContainer;` |
|        2 |   372 |  |
|        - |   373 | `/*` |
|        - |   374 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   375 | ` */` |
|   153782 |   376 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   377 |  |
|        - |   378 | `	VmInstr *pInstr;` |
|   153784 |   379 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   153784 |   380 | `	return pInstr;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   384 | ` */` |
|   685166 |   385 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   386 |  |
|   685168 |   387 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   388 |  |
|        - |   389 | `/*` |
|        - |   390 | ` * Pop the last VM instruction.` |
|        - |   391 | ` */` |
|   145918 |   392 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   393 |  |
|   145920 |   394 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Peek the last VM instruction.` |
|        - |   398 | ` */` |
|   386426 |   399 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   400 |  |
|   386428 |   401 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   402 |  |
|    11090 |   403 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   404 |  |
|        - |   405 | `	VmInstr *aInstr;` |
|        - |   406 | `	sxu32 n;` |
|    11092 |   407 | `	n = SySetUsed(pVm->pByteContainer);` |
|    11092 |   408 | `	if( n < 2 ){` |
|      ! 0 |   409 | `		return 0;` |
|        - |   410 | `	}` |
|    11092 |   411 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    11092 |   412 | `	return &aInstr[n - 2];` |
|     5547 |   413 |  |
|        - |   414 | `/*` |
|        - |   415 | ` * Allocate a new virtual machine frame.` |
|        - |   416 | ` */` |
|    13598 |   417 | `static VmFrame * VmNewFrame(` |
|        - |   418 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   419 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   420 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   421 | `	)` |
|        2 |   422 |  |
|        - |   423 | `	VmFrame *pFrame;` |
|        - |   424 | `	/* Allocate a new vm frame */` |
|    13600 |   425 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    13600 |   426 | `	if( pFrame == 0 ){` |
|      ! 0 |   427 | `		return 0;` |
|        - |   428 | `	}` |
|        - |   429 | `	/* Zero the structure */` |
|    13600 |   430 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   431 | `	/* Initialize frame fields */` |
|    13600 |   432 | `	pFrame->pUserData = pUserData;` |
|    13600 |   433 | `	pFrame->pThis = pThis;` |
|    13600 |   434 | `	pFrame->pVm = pVm;` |
|    13600 |   435 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    13600 |   436 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    13600 |   437 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    13600 |   438 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    13600 |   439 | `	return pFrame;` |
|     6801 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Enter a VM frame.` |
|        - |   443 | ` */` |
|    13598 |   444 | `static sxi32 VmEnterFrame(` |
|        - |   445 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   446 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   447 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   448 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   449 | `	)` |
|        2 |   450 |  |
|        - |   451 | `	VmFrame *pFrame;` |
|        - |   452 | `	/* Allocate a new frame */` |
|    13600 |   453 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    13600 |   454 | `	if( pFrame == 0 ){` |
|      ! 0 |   455 | `		return SXERR_MEM;` |
|        - |   456 | `	}` |
|        - |   457 | `	/* Link to the list of active VM frame */` |
|    13600 |   458 | `	pFrame->pParent = pVm->pFrame;` |
|    13600 |   459 | `	pVm->pFrame = pFrame;` |
|    13600 |   460 | `	if( ppFrame ){` |
|        - |   461 | `		/* Write a pointer to the new VM frame */` |
|    11428 |   462 | `		*ppFrame = pFrame;` |
|     5713 |   463 | `	}` |
|    13600 |   464 | `	return SXRET_OK;` |
|     6801 |   465 |  |
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
|       63 |   484 | `	while( pFrame ){` |
|       63 |   485 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   486 | `			/* Query the current frame */` |
|       49 |   487 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       49 |   488 | `			if( pEntry ){` |
|        - |   489 | `				/* Variable found */` |
|       49 |   490 | `				break;` |
|        - |   491 | `			}` |
|      ! 0 |   492 | `		}` |
|        - |   493 | `		/* Point to the upper frame */` |
|       15 |   494 | `		pFrame = pFrame->pParent;` |
|        1 |   495 | `	}` |
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
|    11424 |   512 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   513 |  |
|    11426 |   514 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    11426 |   515 | `	if( pCurFrame ){` |
|        - |   516 | `		/* Unlink from the list of active VM frame */` |
|    11426 |   517 | `		pVm->pFrame = pCurFrame->pParent;` |
|    11426 |   518 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   519 | `			VmSlot  *aSlot;` |
|        - |   520 | `			sxu32 n;` |
|        - |   521 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    11408 |   522 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    82586 |   523 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   524 | `				/* Unset the local variable */` |
|    71180 |   525 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    35591 |   526 | `			}` |
|        - |   527 | `			/* Remove local reference */` |
|    11408 |   528 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    82638 |   529 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    71232 |   530 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    35617 |   531 | `			}` |
|     5703 |   532 | `		}` |
|        - |   533 | `		/* Release internal containers */` |
|    11426 |   534 | `		SyHashRelease(&pCurFrame->hVar);` |
|    11426 |   535 | `		SySetRelease(&pCurFrame->sArg);` |
|    11426 |   536 | `		SySetRelease(&pCurFrame->sLocal);` |
|    11426 |   537 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   538 | `		/* Release the whole structure */` |
|    11426 |   539 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5712 |   540 | `	}` |
|    11426 |   541 |  |
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
|    81370 |   658 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   659 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   660 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   661 | `	)` |
|        2 |   662 |  |
|        - |   663 | `	ph7_class_method *pMeth;` |
|        - |   664 | `	ph7_class_attr *pAttr;` |
|        - |   665 | `	SyHashEntry *pEntry;` |
|        - |   666 | `	sxi32 rc;` |
|        - |   667 | `	/* Reset the loop cursor */` |
|    81372 |   668 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   669 | `	/* Process only static and constant attribute */` |
|   312409 |   670 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   671 | `		/* Extract the current attribute */` |
|   190354 |   672 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   190354 |   673 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    81372 |   695 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   696 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   697 | `		 */` |
|    44180 |   698 | `		return SXRET_OK;` |
|        - |   699 | `	}` |
|        - |   700 | `	/* Create constructor alias if not yet done */` |
|    37194 |   701 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   702 | `		/* User constructor with the same base class name */` |
|      210 |   703 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      210 |   704 | `		if( pEntry ){` |
|      ! 0 |   705 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   706 | `			/* Create the alias */` |
|      ! 0 |   707 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   708 | `		}` |
|      104 |   709 | `	}` |
|        - |   710 | `	/* Install the methods now */` |
|    37194 |   711 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   359874 |   712 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   304086 |   713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   304086 |   714 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   304080 |   715 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   304080 |   716 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   717 | `				return rc;` |
|        - |   718 | `			}` |
|   152039 |   719 | `		}` |
|        2 |   720 | `	}` |
|        - |   721 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    37194 |   722 | `	pClass->bMounted = TRUE;` |
|    37194 |   723 | `	return SXRET_OK;` |
|    40687 |   724 |  |
|        - |   725 | `/*` |
|        - |   726 | ` * Allocate a private frame for attributes of the given` |
|        - |   727 | ` * class instance (Object in the PHP jargon).` |
|        - |   728 | ` */` |
|     1004 |   729 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   731 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   732 | `	)` |
|        2 |   733 |  |
|     1006 |   734 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   735 | `	ph7_class_attr *pAttr;` |
|        - |   736 | `	SyHashEntry *pEntry;` |
|        - |   737 | `	sxi32 rc;` |
|        - |   738 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1006 |   739 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4296 |   740 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   741 | `		VmClassAttr *pVmAttr;` |
|        - |   742 | `		/* Extract the current attribute */` |
|     3292 |   743 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3292 |   744 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3292 |   745 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   746 | `			return SXERR_MEM;` |
|        - |   747 | `		}` |
|     3292 |   748 | `		pVmAttr->pAttr = pAttr;` |
|     3292 |   749 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   750 | `			ph7_value *pMemObj;` |
|        - |   751 | `			/* Reserve a memory object for this attribute */` |
|     3286 |   752 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3286 |   753 | `			if( pMemObj == 0 ){` |
|      ! 0 |   754 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   755 | `				return SXERR_MEM;` |
|        - |   756 | `			}` |
|     3286 |   757 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3286 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1072 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      535 |   761 | `			}` |
|     3286 |   762 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3286 |   763 | `			if( rc != SXRET_OK ){` |
|        - |   764 | `				VmSlot sSlot;` |
|        - |   765 | `				/* Restore memory object */` |
|      ! 0 |   766 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   767 | `				sSlot.pUserData = 0;` |
|      ! 0 |   768 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   769 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   770 | `				return SXERR_MEM;` |
|        - |   771 | `			}` |
|        - |   772 | `			/* Install attribute in the reference table */` |
|     3286 |   773 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1644 |   774 | `		}else{` |
|        - |   775 | `			/* Install static/constant attribute */` |
|        8 |   776 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   777 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   778 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   779 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   780 | `				return SXERR_MEM;` |
|        - |   781 | `			}` |
|        - |   782 | `		}` |
|        2 |   783 | `	}` |
|     1006 |   784 | `	return SXRET_OK;` |
|      504 |   785 |  |
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
|   268136 |   797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   798 |  |
|        - |   799 | `	ph7_value *pObj;` |
|        - |   800 | `	sxi32 rc;` |
|   268138 |   801 | `	if( pIndex ){` |
|        - |   802 | `		/* Object index in the object table */` |
|   261622 |   803 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   130810 |   804 | `	}` |
|        - |   805 | `	/* Reserve a slot for the new object */` |
|   268138 |   806 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   268138 |   807 | `	if( rc != SXRET_OK ){` |
|        - |   808 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   809 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   810 | `		 */` |
|      ! 0 |   811 | `		return 0;` |
|        - |   812 | `	}` |
|   268138 |   813 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   268138 |   814 | `	return pObj;` |
|   134070 |   815 |  |
|        - |   816 | `/*` |
|        - |   817 | ` * Reserve a memory object.` |
|        - |   818 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   819 | ` */` |
|  2134894 |   820 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   821 |  |
|        - |   822 | `	ph7_value *pObj;` |
|        - |   823 | `	sxi32 rc;` |
|  2134896 |   824 | `	if( pIndex ){` |
|        - |   825 | `		/* Object index in the object table */` |
|  2134896 |   826 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1067447 |   827 | `	}` |
|        - |   828 | `	/* Reserve a slot for the new object */` |
|  2134896 |   829 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2134896 |   830 | `	if( rc != SXRET_OK ){` |
|        - |   831 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   832 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   833 | `		 */` |
|      ! 0 |   834 | `		return 0;` |
|        - |   835 | `	}` |
|  2134896 |   836 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2134896 |   837 | `	return pObj;` |
|  1067449 |   838 |  |
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
|     2172 |  1191 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1192 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1193 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1194 | `	 )` |
|        2 |  1195 |  |
|        - |  1196 | `	SyString sBuiltin;` |
|        - |  1197 | `	ph7_value *pObj;` |
|        - |  1198 | `	sxi32 rc;` |
|        - |  1199 | `	/* Zero the structure */` |
|     2174 |  1200 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1201 | `	/* Initialize VM fields */` |
|     2174 |  1202 | `	pVm->pEngine = &(*pEngine);` |
|     2174 |  1203 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1204 | `	/* Instructions containers */` |
|     2174 |  1205 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2174 |  1206 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2174 |  1207 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1208 | `	/* Object containers */` |
|     2174 |  1209 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2174 |  1210 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1211 | `	/* Virtual machine internal containers */` |
|     2174 |  1212 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2174 |  1213 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2174 |  1214 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2174 |  1215 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2174 |  1216 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2174 |  1217 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2174 |  1218 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2174 |  1219 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2174 |  1220 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2174 |  1221 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2174 |  1222 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2174 |  1223 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2174 |  1224 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2174 |  1225 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2174 |  1226 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1227 | `	/* Configuration containers */` |
|     2174 |  1228 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2174 |  1229 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2174 |  1230 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2174 |  1231 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2174 |  1232 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1233 | `	/* Error callbacks containers */` |
|     2174 |  1234 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2174 |  1235 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2174 |  1236 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2174 |  1237 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2174 |  1238 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1239 | `	/* Set a default recursion limit */` |
|        - |  1240 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2174 |  1241 | `	pVm->nMaxDepth = 32;` |
|        - |  1242 | `#else` |
|        - |  1243 | `	pVm->nMaxDepth = 16;` |
|        - |  1244 | `#endif` |
|        - |  1245 | `	/* Default assertion flags */` |
|     2174 |  1246 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1247 | `	/* JSON return status */` |
|     2174 |  1248 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1249 | `	/* PRNG context */` |
|     2174 |  1250 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1251 | `	/* Install the null constant */` |
|     2174 |  1252 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2174 |  1253 | `	if( pObj == 0 ){` |
|      ! 0 |  1254 | `		rc = SXERR_MEM;` |
|      ! 0 |  1255 | `		goto Err;` |
|        - |  1256 | `	}` |
|     2174 |  1257 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1258 | `	/* Install the boolean TRUE constant */` |
|     2174 |  1259 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2174 |  1260 | `	if( pObj == 0 ){` |
|      ! 0 |  1261 | `		rc = SXERR_MEM;` |
|      ! 0 |  1262 | `		goto Err;` |
|        - |  1263 | `	}` |
|     2174 |  1264 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1265 | `	/* Install the boolean FALSE constant */` |
|     2174 |  1266 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2174 |  1267 | `	if( pObj == 0 ){` |
|      ! 0 |  1268 | `		rc = SXERR_MEM;` |
|      ! 0 |  1269 | `		goto Err;` |
|        - |  1270 | `	}` |
|     2174 |  1271 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1272 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1273 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1274 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2174 |  1275 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2174 |  1276 | `	if( pObj == 0 ){` |
|      ! 0 |  1277 | `		rc = SXERR_MEM;` |
|      ! 0 |  1278 | `		goto Err;` |
|        - |  1279 | `	}` |
|     2174 |  1280 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1281 | `	/* Create the global frame */` |
|     2174 |  1282 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2174 |  1283 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1284 | `		goto Err;` |
|        - |  1285 | `	}` |
|        - |  1286 | `	/* Initialize the code generator */` |
|     2174 |  1287 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2174 |  1288 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1289 | `		goto Err;` |
|        - |  1290 | `	}` |
|        - |  1291 | `	/* VM correctly initialized,set the magic number */` |
|     2174 |  1292 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2174 |  1293 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1294 | `	/* Compile the built-in library */` |
|     2174 |  1295 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1296 | `	/* Reset the code generator */` |
|     2174 |  1297 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2174 |  1298 | `	return SXRET_OK;` |
|      ! 0 |  1299 | `Err:` |
|      ! 0 |  1300 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1301 | `	return rc;` |
|     1088 |  1302 |  |
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
|    28802 |  1332 | `static ph7_value * VmNewOperandStack(` |
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
|    28804 |  1345 | `	nInstr += VM_STACK_GUARD;` |
|    28804 |  1346 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    28804 |  1347 | `	if( pStack == 0 ){` |
|      ! 0 |  1348 | `		return 0;` |
|        - |  1349 | `	}` |
|        - |  1350 | `	/* Initialize the operand stack */` |
|  1828448 |  1351 | `	while( nInstr > 0 ){` |
|  1799646 |  1352 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1799646 |  1353 | `		--nInstr;` |
|        2 |  1354 | `	}` |
|        - |  1355 | `	/* Ready for bytecode execution */` |
|    28804 |  1356 | `	return pStack;` |
|    14403 |  1357 |  |
|        - |  1358 | `/* Forward declaration */` |
|        - |  1359 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1360 | `/*` |
|        - |  1361 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1362 | ` * This routine gets called by the PH7 engine after` |
|        - |  1363 | ` * successful compilation of the target PHP program.` |
|        - |  1364 | ` */` |
|     1934 |  1365 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1366 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1367 | `	)` |
|        2 |  1368 |  |
|        - |  1369 | `	SyHashEntry *pEntry;` |
|        - |  1370 | `	sxi32 rc;` |
|     1936 |  1371 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1372 | `		/* Initialize your VM first */` |
|      ! 0 |  1373 | `		return SXERR_CORRUPT;` |
|        - |  1374 | `	}` |
|        - |  1375 | `	/* Mark the VM ready for byte-code execution */` |
|     1936 |  1376 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1377 | `	/* Release the code generator now we have compiled our program */` |
|     1936 |  1378 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1379 | `	/* Emit the DONE instruction */` |
|     1936 |  1380 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1936 |  1381 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1382 | `		return SXERR_MEM;` |
|        - |  1383 | `	}` |
|        - |  1384 | `	/* Script return value */` |
|     1936 |  1385 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1386 | `	/* Allocate a new operand stack */` |
|     1936 |  1387 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1936 |  1388 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1389 | `		return SXERR_MEM;` |
|        - |  1390 | `	}` |
|        - |  1391 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1392 | `	 * private data. */` |
|     1936 |  1393 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1936 |  1394 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1395 | `	/* Allocate the reference table */` |
|     1936 |  1396 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1936 |  1397 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1936 |  1398 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1399 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1400 | `		return SXERR_MEM;` |
|        - |  1401 | `	}` |
|        - |  1402 | `	/* Zero the reference table */` |
|     1936 |  1403 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1404 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1936 |  1405 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1936 |  1406 | `	if( rc != SXRET_OK ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return rc;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1936 |  1411 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1936 |  1412 | `	if( rc != SXRET_OK ){` |
|        - |  1413 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1414 | `		return rc;` |
|        - |  1415 | `	}` |
|        - |  1416 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1936 |  1417 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1418 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1936 |  1419 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1420 | `	/* Initialize and install static and constants class attributes */` |
|     1936 |  1421 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    25178 |  1422 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    23244 |  1423 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    23244 |  1424 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1425 | `			return rc;` |
|        - |  1426 | `		}` |
|        2 |  1427 | `	}` |
|        - |  1428 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1936 |  1429 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1430 | `	/* VM is ready for bytecode execution */` |
|     1936 |  1431 | `	return SXRET_OK;` |
|      969 |  1432 |  |
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
|     1926 |  1452 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1453 |  |
|        - |  1454 | `	/* Set the stale magic number */` |
|     1928 |  1455 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1456 | `	/* Release the private memory subsystem */` |
|     1928 |  1457 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1928 |  1458 | `	return SXRET_OK;` |
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
|   551452 |  1470 | `static sxi32 VmInitCallContext(` |
|        - |  1471 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1472 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1473 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1474 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1475 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1476 | `	)` |
|        2 |  1477 |  |
|   551454 |  1478 | `	pOut->pFunc = pFunc;` |
|   551454 |  1479 | `	pOut->pVm   = pVm;` |
|   551454 |  1480 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   551454 |  1481 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1482 | `	/* Assume a null return value */` |
|   551454 |  1483 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   551454 |  1484 | `	pOut->pRet = pRet;` |
|   551454 |  1485 | `	pOut->iFlags = iFlags;` |
|   551454 |  1486 | `	return SXRET_OK;` |
|        2 |  1487 |  |
|        - |  1488 | `/*` |
|        - |  1489 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1490 | ` * left behind.` |
|        - |  1491 | ` */` |
|   551452 |  1492 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1493 |  |
|        - |  1494 | `	sxu32 n;` |
|   551454 |  1495 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6442 |  1496 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18304 |  1497 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    11864 |  1498 | `			if( apObj[n] == 0 ){` |
|        - |  1499 | `				/* Already released */` |
|      250 |  1500 | `				continue;` |
|        - |  1501 | `			}` |
|    11616 |  1502 | `			PH7_MemObjRelease(apObj[n]);` |
|    11616 |  1503 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5809 |  1504 | `		}` |
|     6442 |  1505 | `		SySetRelease(&pCtx->sVar);` |
|     3220 |  1506 | `	}` |
|   551454 |  1507 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   551454 |  1523 |  |
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
|  3326836 |  1554 | `static void VmPopOperand(` |
|        - |  1555 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1556 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1557 | `	)` |
|        2 |  1558 |  |
|  3326838 |  1559 | `	ph7_value *pTos = *ppTos;` |
|  7037706 |  1560 | `	while( nPop > 0 ){` |
|  3710870 |  1561 | `		PH7_MemObjRelease(pTos);` |
|  3710870 |  1562 | `		pTos--;` |
|  3710870 |  1563 | `		nPop--;` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Top of the stack */` |
|  3326838 |  1566 | `	*ppTos = pTos;` |
|  3326838 |  1567 |  |
|        - |  1568 | `/*` |
|        - |  1569 | ` * Reserve a memory object.` |
|        - |  1570 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1571 | ` */` |
|  2973368 |  1572 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1573 |  |
|  2973370 |  1574 | `	ph7_value *pObj = 0;` |
|        - |  1575 | `	VmSlot *pSlot;` |
|        - |  1576 | `	sxu32 nIdx;` |
|        - |  1577 | `	/* Check for a free slot */` |
|  2973370 |  1578 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2973370 |  1579 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2973370 |  1580 | `	if( pSlot ){` |
|   838476 |  1581 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   838476 |  1582 | `		nIdx = pSlot->nIdx;` |
|   419237 |  1583 | `	}` |
|  2973370 |  1584 | `	if( pObj == 0 ){` |
|        - |  1585 | `		/* Reserve a new memory object */` |
|  2134896 |  1586 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2134896 |  1587 | `		if( pObj == 0 ){` |
|      ! 0 |  1588 | `			return 0;` |
|        - |  1589 | `		}` |
|  1067447 |  1590 | `	}` |
|        - |  1591 | `	/* Set a null default value */` |
|  2973370 |  1592 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2973370 |  1593 | `	pObj->nIdx = nIdx;` |
|  2973370 |  1594 | `	return pObj;` |
|  1486686 |  1595 |  |
|        - |  1596 | `/*` |
|        - |  1597 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1598 | ` */` |
|    24894 |  1599 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1600 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1601 | `	const char *zKey,  /* Entry key */` |
|        - |  1602 | `	sxu32 nByte,       /* Key length */` |
|        - |  1603 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1604 | `	)` |
|        2 |  1605 |  |
|        - |  1606 | `	ph7_value sKey;` |
|        - |  1607 | `	sxi32 rc;` |
|    24896 |  1608 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    24896 |  1609 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1610 | `	/* Perform the insertion */` |
|    24896 |  1611 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    24896 |  1612 | `	PH7_MemObjRelease(&sKey);` |
|    24896 |  1613 | `	return rc;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1617 | ` * Return a pointer to the variable value on success.` |
|        - |  1618 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1619 | ` */` |
|  3150374 |  1620 | `static ph7_value * VmExtractMemObj(` |
|        - |  1621 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1622 | `	const SyString *pName, /* Variable name */` |
|        - |  1623 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1624 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1625 | `	)` |
|        2 |  1626 |  |
|  3150376 |  1627 | `	int bNullify = FALSE;` |
|        - |  1628 | `	SyHashEntry *pEntry;` |
|        - |  1629 | `	VmFrame *pFrame;` |
|        - |  1630 | `	ph7_value *pObj;` |
|        - |  1631 | `	sxu32 nIdx;` |
|        - |  1632 | `	sxi32 rc;` |
|        - |  1633 | `	/* Point to the top active frame */` |
|  3150376 |  1634 | `	pFrame = pVm->pFrame;` |
|  3199728 |  1635 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1636 | `		/* Safely ignore the exception frame */` |
|    49353 |  1637 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1638 | `	}` |
|        - |  1639 | `	/* Perform the lookup */` |
|  3150376 |  1640 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1641 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1642 | `		pName = &sAnnon;` |
|        - |  1643 | `		/* Always nullify the object */` |
|      ! 0 |  1644 | `		bNullify = TRUE;` |
|      ! 0 |  1645 | `		bDup = FALSE;` |
|      ! 0 |  1646 | `	}` |
|        - |  1647 | `	/* Check the superglobals table first */` |
|  3150376 |  1648 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3150376 |  1649 | `	if( pEntry == 0 ){` |
|        - |  1650 | `		/* Query the top active frame */` |
|  3150340 |  1651 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3150340 |  1652 | `		if( pEntry == 0 ){` |
|    77338 |  1653 | `			char *zName = (char *)pName->zString;` |
|        - |  1654 | `			VmSlot sLocal;` |
|    77338 |  1655 | `			if( !bCreate ){` |
|        - |  1656 | `				/* Do not create the variable,return NULL instead */` |
|      630 |  1657 | `				return 0;` |
|        - |  1658 | `			}` |
|        - |  1659 | `			/* No such variable,automatically create a new one and install` |
|        - |  1660 | `			 * it in the current frame.` |
|        - |  1661 | `			 */` |
|    76710 |  1662 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    76710 |  1663 | `			if( pObj == 0 ){` |
|      ! 0 |  1664 | `				return 0;` |
|        - |  1665 | `			}` |
|    76710 |  1666 | `			nIdx = pObj->nIdx;` |
|    76710 |  1667 | `			if( bDup ){` |
|        - |  1668 | `				/* Duplicate name */` |
|      164 |  1669 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1670 | `				if( zName == 0 ){` |
|      ! 0 |  1671 | `					return 0;` |
|        - |  1672 | `				}` |
|       81 |  1673 | `			}` |
|        - |  1674 | `			/* Link to the top active VM frame */` |
|    76710 |  1675 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    76710 |  1676 | `			if( rc != SXRET_OK ){` |
|        - |  1677 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1678 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1679 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1680 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1681 | `				return 0;` |
|        - |  1682 | `			}` |
|    76710 |  1683 | `			if( pFrame->pParent != 0 ){` |
|        - |  1684 | `				/* Local variable */` |
|    71180 |  1685 | `				sLocal.nIdx = nIdx;` |
|    71180 |  1686 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    35591 |  1687 | `			}else{` |
|        - |  1688 | `				/* Register in the $GLOBALS array */` |
|     5532 |  1689 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1690 | `			}` |
|        - |  1691 | `			/* Install in the reference table */` |
|    76710 |  1692 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1693 | `			/* Save object index */` |
|    76710 |  1694 | `			pObj->nIdx = nIdx;` |
|    38356 |  1695 | `		}else{` |
|        - |  1696 | `			/* Extract variable contents */` |
|  3073004 |  1697 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3073004 |  1698 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3073004 |  1699 | `			if( bNullify && pObj ){` |
|      ! 0 |  1700 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1701 | `			}` |
|        - |  1702 | `		}` |
|  1574967 |  1703 | `	}else{` |
|        - |  1704 | `		/* Superglobal */` |
|       38 |  1705 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1706 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1707 | `	}` |
|  3149748 |  1708 | `	return pObj;` |
|  1575299 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1712 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1713 | ` */` |
|     1960 |  1714 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1715 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1716 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1717 | `	sxu32 nByte        /* zName length */` |
|        - |  1718 | `	)` |
|        2 |  1719 |  |
|        - |  1720 | `	SyHashEntry *pEntry;` |
|        - |  1721 | `	ph7_value *pValue;` |
|        - |  1722 | `	sxu32 nIdx;` |
|        - |  1723 | `	/* Query the superglobal table */` |
|     1962 |  1724 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1962 |  1725 | `	if( pEntry == 0 ){` |
|        - |  1726 | `		/* No such entry */` |
|      ! 0 |  1727 | `		return 0;` |
|        - |  1728 | `	}` |
|        - |  1729 | `	/* Extract the superglobal index in the global object pool */` |
|     1962 |  1730 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1731 | `	/* Extract the variable value  */` |
|     1962 |  1732 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1962 |  1733 | `	return pValue;` |
|      982 |  1734 |  |
|        - |  1735 | `/*` |
|        - |  1736 | ` * Perform a raw hashmap insertion.` |
|        - |  1737 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1738 | ` */` |
|     1958 |  1739 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1740 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1741 | `	const char *zKey,   /* Entry key */` |
|        - |  1742 | `	int nKeylen,        /* zKey length*/` |
|        - |  1743 | `	const char *zData,  /* Entry data */` |
|        - |  1744 | `	int nLen            /* zData length */` |
|        - |  1745 | `	)` |
|        2 |  1746 |  |
|        - |  1747 | `	ph7_value sKey,sValue;` |
|        - |  1748 | `	sxi32 rc;` |
|     1960 |  1749 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1960 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1960 |  1751 | `	if( zKey ){` |
|     1938 |  1752 | `		if( nKeylen < 0 ){` |
|     1938 |  1753 | `			nKeylen = (int)SyStrlen(zKey);` |
|      968 |  1754 | `		}` |
|     1938 |  1755 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      968 |  1756 | `	}` |
|     1960 |  1757 | `	if( zData ){` |
|     1960 |  1758 | `		if( nLen < 0 ){` |
|        - |  1759 | `			/* Compute length automatically */` |
|      ! 0 |  1760 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1761 | `		}` |
|     1960 |  1762 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      979 |  1763 | `	}` |
|        - |  1764 | `	/* Perform the insertion */` |
|     1960 |  1765 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1960 |  1766 | `	PH7_MemObjRelease(&sKey);` |
|     1960 |  1767 | `	PH7_MemObjRelease(&sValue);` |
|     1960 |  1768 | `	return rc;` |
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
|    30968 |  1783 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1784 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1785 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1786 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1787 | `	)` |
|        2 |  1788 |  |
|    30970 |  1789 | `	sxi32 rc = SXRET_OK;` |
|    30970 |  1790 | `	switch(nOp){` |
|      967 |  1791 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1936 |  1792 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1936 |  1793 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1794 | `		/* VM output consumer callback */` |
|        - |  1795 | `#ifdef UNTRUST` |
|        - |  1796 | `		if( xConsumer == 0 ){` |
|        - |  1797 | `			rc = SXERR_CORRUPT;` |
|        - |  1798 | `			break;` |
|        - |  1799 | `		}` |
|        - |  1800 | `#endif` |
|        - |  1801 | `		/* Install the output consumer */` |
|     1936 |  1802 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1936 |  1803 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1936 |  1804 | `		break;` |
|        - |  1805 | `							   }` |
|      967 |  1806 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1807 | `		/* Import path */` |
|        - |  1808 | `		  const char *zPath;` |
|        - |  1809 | `		  SyString sPath;` |
|     1936 |  1810 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1811 | `#if defined(UNTRUST)` |
|        - |  1812 | `		  if( zPath == 0 ){` |
|        - |  1813 | `			  rc = SXERR_EMPTY;` |
|        - |  1814 | `			  break;` |
|        - |  1815 | `		  }` |
|        - |  1816 | `#endif` |
|     1936 |  1817 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1818 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1819 | `#ifdef __WINNT__` |
|        2 |  1820 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1821 | `#endif` |
|     3870 |  1822 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1823 | `		  /* Remove leading and trailing white spaces */` |
|     1936 |  1824 | `		  SyStringFullTrim(&sPath);` |
|     1936 |  1825 | `		  if( sPath.nByte > 0 ){` |
|        - |  1826 | `			  /* Store the path in the corresponding conatiner */` |
|     1936 |  1827 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      967 |  1828 | `		  }` |
|     1936 |  1829 | `		  break;` |
|        - |  1830 | `									 }` |
|      967 |  1831 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1832 | `		/* Run-Time Error report */` |
|     1936 |  1833 | `		pVm->bErrReport = 1;` |
|     1936 |  1834 | `		break;` |
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
|     9670 |  1856 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1857 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1858 | `		/* Create a new superglobal/global variable */` |
|    19342 |  1859 | `		const char *zName = va_arg(ap,const char *);` |
|    19342 |  1860 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    19342 |  1871 | `		nByte = SyStrlen(zName);` |
|    19342 |  1872 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1873 | `			/* Check if the superglobal is already installed */` |
|    19342 |  1874 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     9672 |  1875 | `		}else{` |
|        - |  1876 | `			/* Query the top active VM frame */` |
|      ! 0 |  1877 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1878 | `		}` |
|    19342 |  1879 | `		if( pEntry ){` |
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
|    19342 |  1890 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    19342 |  1891 | `			if( pObj == 0 ){` |
|      ! 0 |  1892 | `				rc = SXERR_MEM;` |
|      ! 0 |  1893 | `				break;` |
|        - |  1894 | `			}` |
|    19342 |  1895 | `			nIdx = pObj->nIdx;` |
|        - |  1896 | `			/* Copy value */` |
|    19342 |  1897 | `			PH7_MemObjStore(pValue,pObj);` |
|    19342 |  1898 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1899 | `				/* Install the superglobal */` |
|    19342 |  1900 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     9672 |  1901 | `			}else{` |
|        - |  1902 | `				/* Install in the current frame */` |
|      ! 0 |  1903 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1904 | `			}` |
|    19342 |  1905 | `			if( rc == SXRET_OK ){` |
|        - |  1906 | `				SyHashEntry *pRef;` |
|    19342 |  1907 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    19342 |  1908 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     9672 |  1909 | `				}else{` |
|      ! 0 |  1910 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1911 | `				}` |
|        - |  1912 | `				/* Install in the reference table */` |
|    19342 |  1913 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    19342 |  1914 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1915 | `					/* Register in the $GLOBALS array */` |
|    19342 |  1916 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     9670 |  1917 | `				}` |
|     9670 |  1918 | `			}` |
|        - |  1919 | `		}` |
|    19342 |  1920 | `		break;` |
|        - |  1921 | `									}` |
|      968 |  1922 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1923 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1924 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1925 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1926 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1927 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1928 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1938 |  1929 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1938 |  1930 | `		const char *zValue = va_arg(ap,const char *);` |
|     1938 |  1931 | `		int nLen = va_arg(ap,int);` |
|        - |  1932 | `		ph7_hashmap *pMap;` |
|        - |  1933 | `		ph7_value *pValue;` |
|     1938 |  1934 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1935 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1936 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1937 |  1937 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1938 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1939 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1936 |  1940 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1941 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1942 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1936 |  1943 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1944 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1945 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1936 |  1946 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1947 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1948 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1936 |  1949 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1950 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1951 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1952 | `		}else{` |
|        - |  1953 | `			/* Extract the $_SERVER superglobal */` |
|     1936 |  1954 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1955 | `		}` |
|     1938 |  1956 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1957 | `			/* No such entry */` |
|      ! 0 |  1958 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1959 | `			break;` |
|        - |  1960 | `		}` |
|        - |  1961 | `		/* Point to the hashmap */` |
|     1938 |  1962 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1963 | `		/* Perform the insertion */` |
|     1938 |  1964 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1938 |  1965 | `		break;` |
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
|     1934 |  2016 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2017 | `		/* Register an IO stream device */` |
|     3870 |  2018 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2019 | `		/* Make sure we are dealing with a valid IO stream */` |
|     5802 |  2020 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     3870 |  2021 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2022 | `				/* Invalid stream */` |
|      ! 0 |  2023 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2024 | `				break;` |
|        - |  2025 | `		}` |
|     3870 |  2026 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2027 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1936 |  2028 | `			pVm->pDefStream = pStream;` |
|      967 |  2029 | `		}` |
|        - |  2030 | `		/* Insert in the appropriate container */` |
|     3870 |  2031 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     3870 |  2032 | `		break;` |
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
|    30970 |  2069 | `	return rc;` |
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
|      512 |  2128 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2129 |  |
|      513 |  2130 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      513 |  2131 | `	sxi32 rc = SXRET_OK;` |
|        - |  2132 | `	/* Append a new line */` |
|        - |  2133 | `#ifdef __WINNT__` |
|        1 |  2134 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2135 | `#else` |
|      512 |  2136 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2137 | `#endif` |
|        - |  2138 | `	/* Invoke the output consumer callback */` |
|      513 |  2139 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      513 |  2140 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2141 | `		/* Increment output length */` |
|      513 |  2142 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      256 |  2143 | `	}` |
|      513 |  2144 | `	return rc;` |
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
|      878 |  2346 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2347 |  |
|        - |  2348 | `	VmFrame *pFrame;` |
|        - |  2349 | `	ph7_vm_func *pFunc;` |
|      879 |  2350 | `	*pzFuncName = 0;` |
|      879 |  2351 | `	*pnFuncLen = 0;` |
|      879 |  2352 | `	pFrame = pVm->pFrame;` |
|      879 |  2353 | `	if( pFrame == 0 ){` |
|      ! 0 |  2354 | `		return;` |
|        - |  2355 | `	}` |
|      879 |  2356 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2357 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2358 | `	}` |
|      879 |  2359 | `	if( pFrame->pParent == 0 ){` |
|      873 |  2360 | `		return;` |
|        - |  2361 | `	}` |
|        7 |  2362 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2363 | `	if( pFunc == 0 ){` |
|      ! 0 |  2364 | `		return;` |
|        - |  2365 | `	}` |
|        7 |  2366 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2367 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      440 |  2368 |  |
|        - |  2369 | `/*` |
|        - |  2370 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2371 | ` */` |
|      442 |  2372 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2373 |  |
|        - |  2374 | `	SyBlob sOut;` |
|        - |  2375 | `	SyString *pFile;` |
|      443 |  2376 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2377 | `		return PH7_OK;` |
|        - |  2378 | `	}` |
|      443 |  2379 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2380 | `		zClass = "Exception";` |
|      ! 0 |  2381 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2382 | `	}` |
|      443 |  2383 | `	if( zMsg == 0 ){` |
|      ! 0 |  2384 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2385 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2386 | `	}` |
|      443 |  2387 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      437 |  2388 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      218 |  2389 | `	}` |
|      443 |  2390 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      443 |  2391 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      443 |  2392 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      443 |  2393 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      443 |  2394 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      443 |  2395 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      443 |  2396 | `	if( pFile ){` |
|      443 |  2397 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      443 |  2398 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      443 |  2399 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      221 |  2400 | `	}` |
|      443 |  2401 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      443 |  2402 | `	if( pFile ){` |
|      443 |  2403 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      443 |  2404 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      443 |  2405 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2406 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2407 | `		}else{` |
|      437 |  2408 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2409 | `		}` |
|      221 |  2410 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2411 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2412 | `	}else{` |
|      ! 0 |  2413 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2414 | `	}` |
|      443 |  2415 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      443 |  2416 | `	if( pFile ){` |
|      443 |  2417 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      443 |  2418 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      443 |  2419 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      443 |  2420 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      221 |  2421 | `	}` |
|      443 |  2422 | `	VmCallErrorHandler(pVm,&sOut);` |
|      443 |  2423 | `	SyBlobRelease(&sOut);` |
|      443 |  2424 | `	return PH7_ABORT;` |
|      222 |  2425 |  |
|        - |  2426 | `/*` |
|        - |  2427 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2428 | ` */` |
|      436 |  2429 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
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
|      438 |  2443 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2444 | `		return PH7_ABORT;` |
|        - |  2445 | `	}` |
|      438 |  2446 | `	pVm = pCtx->pVm;` |
|      438 |  2447 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2448 | `		zClass = "Error";` |
|      ! 0 |  2449 | `	}` |
|      438 |  2450 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      438 |  2451 | `	if( pClass == 0 ){` |
|      ! 0 |  2452 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2453 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2454 | `			zClass` |
|        - |  2455 | `			);` |
|        - |  2456 | `	}` |
|      438 |  2457 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      438 |  2458 | `	if( pThis == 0 ){` |
|      ! 0 |  2459 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2460 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2461 | `			);` |
|        - |  2462 | `	}` |
|        - |  2463 |  |
|      438 |  2464 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      438 |  2465 | `	va_start(ap,zFormat);` |
|      438 |  2466 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      438 |  2467 | `	va_end(ap);` |
|        - |  2468 |  |
|      438 |  2469 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      438 |  2470 | `	if( pCons ){` |
|      438 |  2471 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      438 |  2472 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      438 |  2473 | `		apArg[0] = &sArg;` |
|      438 |  2474 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      438 |  2475 | `		PH7_MemObjRelease(&sArg);` |
|      218 |  2476 | `	}` |
|      438 |  2477 | `	SyBlobRelease(&sMsg);` |
|        - |  2478 |  |
|      438 |  2479 | `	pFrame = pVm->pFrame;` |
|      438 |  2480 | `	if( pFrame ){` |
|      440 |  2481 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2482 | `			pFrame = pFrame->pParent;` |
|        1 |  2483 | `		}` |
|      438 |  2484 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      218 |  2485 | `	}` |
|      438 |  2486 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      438 |  2487 | `	PH7_ClassInstanceUnref(pThis);` |
|      438 |  2488 | `	if( rc == SXERR_ABORT ){` |
|      435 |  2489 | `		return PH7_ABORT;` |
|        - |  2490 | `	}` |
|        3 |  2491 | `	return PH7_EXCEPTION;` |
|      220 |  2492 |  |
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
|    28802 |  2545 | `static sxi32 VmByteCodeExec(` |
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
|    28804 |  2561 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    28804 |  2562 | `	if( nTos < 0 ){` |
|    27272 |  2563 | `		pTos = &pStack[-1];` |
|    13637 |  2564 | `	}else{` |
|     1534 |  2565 | `		pTos = &pStack[nTos];` |
|        - |  2566 | `	}` |
|    28804 |  2567 | `	pc = 0;` |
|        - |  2568 | `	/* Execute as much as we can */` |
|  4990893 |  2569 | `	for(;;){` |
|        - |  2570 | `		/* Fetch the instruction to execute */` |
|  9981084 |  2571 | `		pInstr = &aInstr[pc];` |
|  9981084 |  2572 | `		rc = SXRET_OK;` |
|        - |  2573 | `/*` |
|        - |  2574 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2575 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2576 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2577 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2578 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2579 | ` */` |
|  9981084 |  2580 | `		switch(pInstr->iOp){` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * DONE: P1 * *` |
|        - |  2583 | ` *` |
|        - |  2584 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2585 | ` * and return immediately.` |
|        - |  2586 | ` */` |
|    14170 |  2587 | `case PH7_OP_DONE:` |
|    28342 |  2588 | `	if( pInstr->iP1 ){` |
|        - |  2589 | `#ifdef UNTRUST` |
|        - |  2590 | `		if( pTos < pStack ){` |
|        - |  2591 | `			goto Abort;` |
|        - |  2592 | `		}` |
|        - |  2593 | `#endif` |
|    16302 |  2594 | `		if( pLastRef ){` |
|    10544 |  2595 | `			*pLastRef = pTos->nIdx;` |
|     5271 |  2596 | `		}` |
|    16302 |  2597 | `		if( pResult ){` |
|        - |  2598 | `			/* Execution result */` |
|    15542 |  2599 | `			PH7_MemObjStore(pTos,pResult);` |
|     7770 |  2600 | `		}` |
|    16302 |  2601 | `		VmPopOperand(&pTos,1);` |
|    20192 |  2602 | `	}else if( pLastRef ){` |
|        - |  2603 | `		/* Nothing referenced */` |
|      846 |  2604 | `		*pLastRef = SXU32_HIGH;` |
|      422 |  2605 | `	}` |
|    28342 |  2606 | `	goto Done;` |
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
|   218752 |  2654 | `case PH7_OP_JMP:` |
|   437550 |  2655 | `	pc = pInstr->iP2 - 1;` |
|   437550 |  2656 | `	break;` |
|        - |  2657 | `/*` |
|        - |  2658 | ` * JZ: P1 P2 *` |
|        - |  2659 | ` *` |
|        - |  2660 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2661 | ` * entry in the stack if P1 is zero.` |
|        - |  2662 | ` */` |
|   507400 |  2663 | `case PH7_OP_JZ:` |
|        - |  2664 | `#ifdef UNTRUST` |
|        - |  2665 | `	if( pTos < pStack ){` |
|        - |  2666 | `		goto Abort;` |
|        - |  2667 | `	}` |
|        - |  2668 | `#endif` |
|        - |  2669 | `	/* Get a boolean value */` |
|  1014890 |  2670 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2671 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2672 | `	}` |
|  1014890 |  2673 | `	if( !pTos->x.iVal ){` |
|        - |  2674 | `		/* Take the jump */` |
|   486932 |  2675 | `		pc = pInstr->iP2 - 1;` |
|   243465 |  2676 | `	}` |
|  1014890 |  2677 | `	if( !pInstr->iP1 ){` |
|   800078 |  2678 | `		VmPopOperand(&pTos,1);` |
|   400060 |  2679 | `	}` |
|  1014890 |  2680 | `	break;` |
|        - |  2681 | `/*` |
|        - |  2682 | ` * JNZ: P1 P2 *` |
|        - |  2683 | ` *` |
|        - |  2684 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2685 | ` * entry in the stack if P1 is zero.` |
|        - |  2686 | ` */` |
|    55642 |  2687 | `case PH7_OP_JNZ:` |
|        - |  2688 | `#ifdef UNTRUST` |
|        - |  2689 | `	if( pTos < pStack ){` |
|        - |  2690 | `		goto Abort;` |
|        - |  2691 | `	}` |
|        - |  2692 | `#endif` |
|        - |  2693 | `	/* Get a boolean value */` |
|   111286 |  2694 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2695 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2696 | `	}` |
|   111286 |  2697 | `	if( pTos->x.iVal ){` |
|        - |  2698 | `		/* Take the jump */` |
|     4110 |  2699 | `		pc = pInstr->iP2 - 1;` |
|     2054 |  2700 | `	}` |
|   111286 |  2701 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2702 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2703 | `	}` |
|   111286 |  2704 | `	break;` |
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
|   388339 |  2718 | `case PH7_OP_POP: {` |
|   776724 |  2719 | `	sxi32 n = pInstr->iP1;` |
|   776724 |  2720 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2721 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2722 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2723 | `	}` |
|   776724 |  2724 | `	VmPopOperand(&pTos,n);` |
|   776724 |  2725 | `	break;` |
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
|    11845 |  2858 | `case PH7_OP_ERR_CTRL:` |
|        - |  2859 | `	/*` |
|        - |  2860 | `	 * TICKET 1433-038:` |
|        - |  2861 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2862 | `	 * use the public API,to control error output.` |
|        - |  2863 | `	 */` |
|    23690 |  2864 | `	break;` |
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
|   786501 |  2912 | `case PH7_OP_LOADC: {` |
|        - |  2913 | `	ph7_value *pObj;` |
|        - |  2914 | `	/* Reserve a room */` |
|  1573048 |  2915 | `	pTos++;` |
|  1573048 |  2916 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1573048 |  2917 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2918 | `			SyHashEntry *pEntry;` |
|        - |  2919 | `			/* Candidate for expansion via user defined callbacks */` |
|    18524 |  2920 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18524 |  2921 | `			if( pEntry ){` |
|    14938 |  2922 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2923 | `				/* Set a NULL default value */` |
|    14938 |  2924 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    14938 |  2925 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2926 | `				/* Invoke the callback and deal with the expanded value */` |
|    14938 |  2927 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2928 | `				/* Mark as constant */` |
|    14938 |  2929 | `				pTos->nIdx = SXU32_HIGH;` |
|    14938 |  2930 | `				break;` |
|        - |  2931 | `			}` |
|     1793 |  2932 | `		}` |
|  1558112 |  2933 | `		PH7_MemObjLoad(pObj,pTos);` |
|   779079 |  2934 | `	}else{` |
|        - |  2935 | `		/* Set a NULL value */` |
|      ! 0 |  2936 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2937 | `	}` |
|        - |  2938 | `	/* Mark as constant */` |
|  1558112 |  2939 | `	pTos->nIdx = SXU32_HIGH;` |
|  1558112 |  2940 | `	break;` |
|        - |  2941 | `				  }` |
|        - |  2942 | `/*` |
|        - |  2943 | ` * LOAD: P1 * P3` |
|        - |  2944 | ` *` |
|        - |  2945 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2946 | ` * from the P3 operand.` |
|        - |  2947 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2948 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2949 | ` */` |
|  1390829 |  2950 | `case PH7_OP_LOAD:{` |
|        - |  2951 | `	ph7_value *pObj;` |
|        - |  2952 | `	SyString sName;` |
|  2781880 |  2953 | `	if( pInstr->p3 == 0 ){` |
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
|  2781862 |  2966 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2967 | `		/* Reserve a room for the target object */` |
|  2781862 |  2968 | `		pTos++;` |
|        - |  2969 | `	}` |
|        - |  2970 | `	/* Extract the requested memory object */` |
|  2781880 |  2971 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2781880 |  2972 | `	if( pObj == 0 ){` |
|      622 |  2973 | `		if( pInstr->iP1 ){` |
|        - |  2974 | `			/* Variable not found,load NULL */` |
|      622 |  2975 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2976 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2977 | `			}else{` |
|      622 |  2978 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2979 | `			}` |
|      622 |  2980 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1391141 |  2981 | `			break;` |
|      ! 0 |  2982 | `		}else{` |
|        - |  2983 | `			/* Fatal error */` |
|      ! 0 |  2984 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2985 | `			goto Abort;` |
|        - |  2986 | `		}` |
|        - |  2987 | `	}` |
|        - |  2988 | `	/* Load variable contents */` |
|  2781260 |  2989 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2781260 |  2990 | `	pTos->nIdx = pObj->nIdx;` |
|  2781260 |  2991 | `	break;` |
|        - |  2992 | `				   }` |
|        - |  2993 | `/*` |
|        - |  2994 | ` * LOAD_MAP P1 * *` |
|        - |  2995 | ` *` |
|        - |  2996 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2997 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2998 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2999 | ` */` |
|    17107 |  3000 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3001 | `	ph7_hashmap *pMap;` |
|        - |  3002 | `	/* Allocate a new hashmap instance */` |
|    34216 |  3003 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    34216 |  3004 | `	if( pMap == 0 ){` |
|      ! 0 |  3005 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3006 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3007 | `		goto Abort;` |
|        - |  3008 | `	}` |
|    34216 |  3009 | `	if( pInstr->iP1 > 0 ){` |
|     2076 |  3010 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3011 | `		/* Perform the insertion */` |
|     6294 |  3012 | `		while( pEntry < pTos ){` |
|     4220 |  3013 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3014 | `				/* Insertion by reference */` |
|      142 |  3015 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3016 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3017 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3018 | `					);` |
|       48 |  3019 | `			}else{` |
|        - |  3020 | `				/* Standard insertion */` |
|     6188 |  3021 | `				PH7_HashmapInsert(pMap,` |
|     4124 |  3022 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2062 |  3023 | `					&pEntry[1]` |
|        - |  3024 | `				);` |
|        - |  3025 | `			}` |
|        - |  3026 | `			/* Next pair on the stack */` |
|     4220 |  3027 | `			pEntry += 2;` |
|        2 |  3028 | `		}` |
|        - |  3029 | `		/* Pop P1 elements */` |
|     2076 |  3030 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1037 |  3031 | `	}` |
|        - |  3032 | `	/* Push the hashmap */` |
|    34216 |  3033 | `	pTos++;` |
|    34216 |  3034 | `	pTos->nIdx = SXU32_HIGH;` |
|    34216 |  3035 | `	pTos->x.pOther = pMap;` |
|    34216 |  3036 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    34216 |  3037 | `	break;` |
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
|   232204 |  3093 | `case PH7_OP_LOAD_IDX: {` |
|   464454 |  3094 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   464454 |  3095 | `	ph7_hashmap *pMap = 0;` |
|        - |  3096 | `	ph7_value *pIdx;` |
|   464454 |  3097 | `	pIdx = 0;` |
|   464454 |  3098 | `	if( pInstr->iP1 == 0 ){` |
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
|   464454 |  3115 | `		pIdx = pTos;` |
|   464454 |  3116 | `		pTos--;` |
|        - |  3117 | `	}` |
|   464454 |  3118 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3119 | `		/* String access */` |
|   379770 |  3120 | `		if( pIdx ){` |
|        - |  3121 | `			sxu32 nOfft;` |
|   379770 |  3122 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3123 | `				/* Force an int cast */` |
|      ! 0 |  3124 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3125 | `			}` |
|   379770 |  3126 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   379770 |  3127 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3128 | `				/* Invalid offset,load null */` |
|      ! 0 |  3129 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3130 | `			}else{` |
|   379770 |  3131 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   379770 |  3132 | `				int c = zData[nOfft];` |
|   379770 |  3133 | `				PH7_MemObjRelease(pTos);` |
|   379770 |  3134 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   379770 |  3135 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3136 | `			}` |
|   189908 |  3137 | `		}else{` |
|        - |  3138 | `			/* No available index,load NULL */` |
|      ! 0 |  3139 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3140 | `		}` |
|   379770 |  3141 | `		break;` |
|        - |  3142 | `	}` |
|    84686 |  3143 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3144 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3145 | `			ph7_value *pObj;` |
|      ! 0 |  3146 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3147 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3148 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3149 | `			}` |
|      ! 0 |  3150 | `		}` |
|      ! 0 |  3151 | `	}` |
|    84686 |  3152 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    84686 |  3153 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3154 | `		/* Point to the hashmap */` |
|    84686 |  3155 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    84686 |  3156 | `		if( pIdx ){` |
|        - |  3157 | `			/* Load the desired entry */` |
|    84686 |  3158 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    42342 |  3159 | `		}` |
|    84686 |  3160 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3161 | `			/* Create a new empty entry */` |
|      ! 0 |  3162 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3163 | `			if( rc == SXRET_OK ){` |
|        - |  3164 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3165 | `				pNode = pMap->pLast;` |
|      ! 0 |  3166 | `			}` |
|      ! 0 |  3167 | `		}` |
|    42342 |  3168 | `	}` |
|    84686 |  3169 | `	if( pIdx ){` |
|    84686 |  3170 | `		PH7_MemObjRelease(pIdx);` |
|    42342 |  3171 | `	}` |
|    84686 |  3172 | `	if( rc == SXRET_OK ){` |
|        - |  3173 | `		/* Load entry contents */` |
|    38922 |  3174 | `		if( pMap->iRef < 2 ){` |
|        - |  3175 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3176 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3177 | `			 */` |
|        7 |  3178 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3179 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3180 | `		}else{` |
|    38916 |  3181 | `			pTos->nIdx = pNode->nValIdx;` |
|    38916 |  3182 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    38916 |  3183 | `			PH7_HashmapUnref(pMap);` |
|        - |  3184 | `		}` |
|    19462 |  3185 | `	}else{` |
|        - |  3186 | `		/* No such entry,load NULL */` |
|    45766 |  3187 | `		PH7_MemObjRelease(pTos);` |
|    45766 |  3188 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3189 | `	}` |
|    84686 |  3190 | `	break;` |
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
|   104768 |  3266 | `case PH7_OP_STORE: {` |
|        - |  3267 | `	ph7_value *pObj;` |
|        - |  3268 | `	SyString sName;` |
|        - |  3269 | `#ifdef UNTRUST` |
|        - |  3270 | `	if( pTos < pStack ){` |
|        - |  3271 | `		goto Abort;` |
|        - |  3272 | `	}` |
|        - |  3273 | `#endif` |
|   209538 |  3274 | `	if( pInstr->iP2 ){` |
|        - |  3275 | `		sxu32 nIdx;` |
|        - |  3276 | `		/* Member store operation */` |
|     2678 |  3277 | `		nIdx = pTos->nIdx;` |
|     2678 |  3278 | `		VmPopOperand(&pTos,1);` |
|     2678 |  3279 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3280 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3281 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3282 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3283 | `		}else{` |
|        - |  3284 | `			/* Point to the desired memory object */` |
|     2674 |  3285 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2674 |  3286 | `			if( pObj ){` |
|        - |  3287 | `				/* Perform the store operation */` |
|     2674 |  3288 | `				PH7_MemObjStore(pTos,pObj);` |
|     1336 |  3289 | `			}` |
|        - |  3290 | `		}` |
|   106108 |  3291 | `		break;` |
|   206862 |  3292 | `	}else if( pInstr->p3 == 0 ){` |
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
|   206856 |  3306 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3307 | `	}` |
|        - |  3308 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   206862 |  3309 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   206862 |  3310 | `	if( pObj == 0 ){` |
|      ! 0 |  3311 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3312 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3313 | `		goto Abort;` |
|        - |  3314 | `	}` |
|   206862 |  3315 | `	if( !pInstr->p3 ){` |
|        7 |  3316 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3317 | `	}` |
|        - |  3318 | `	/* Perform the store operation */` |
|   206862 |  3319 | `	PH7_MemObjStore(pTos,pObj);` |
|   206862 |  3320 | `	break;` |
|        - |  3321 | `				   }` |
|        - |  3322 | `/*` |
|        - |  3323 | ` * STORE_IDX:   P1 * P3` |
|        - |  3324 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3325 | ` *` |
|        - |  3326 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3327 | ` */` |
|    77428 |  3328 | `case PH7_OP_STORE_IDX:` |
|        - |  3329 | `case PH7_OP_STORE_IDX_REF: {` |
|   154858 |  3330 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3331 | `	ph7_value *pKey;` |
|        - |  3332 | `	sxu32 nIdx;` |
|   154858 |  3333 | `	if( pInstr->iP1 ){` |
|        - |  3334 | `		/* Key is next on stack */` |
|    55770 |  3335 | `		pKey = pTos;` |
|    55770 |  3336 | `		pTos--;` |
|    27886 |  3337 | `	}else{` |
|    99090 |  3338 | `		pKey = 0;` |
|        - |  3339 | `	}` |
|   154858 |  3340 | `	nIdx = pTos->nIdx;` |
|   154858 |  3341 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3342 | `		/* Hashmap already loaded */` |
|   154806 |  3343 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   154806 |  3344 | `		if( pMap->iRef < 2 ){` |
|        - |  3345 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3346 | `			pMap->iRef = 2;` |
|      ! 0 |  3347 | `		}` |
|    77404 |  3348 | `	}else{` |
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
|   154806 |  3402 | `	VmPopOperand(&pTos,1);` |
|        - |  3403 | `	/* Phase#2: Perform the insertion */` |
|   154806 |  3404 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3405 | `		/* Insertion by reference */` |
|       15 |  3406 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3407 | `	}else{` |
|   154792 |  3408 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3409 | `	}` |
|   154806 |  3410 | `	if( pKey ){` |
|    55720 |  3411 | `		PH7_MemObjRelease(pKey);` |
|    27859 |  3412 | `	}` |
|   154806 |  3413 | `	break;` |
|        - |  3414 | `					   }` |
|        - |  3415 | `/*` |
|        - |  3416 | ` * INCR: P1 * *` |
|        - |  3417 | ` *` |
|        - |  3418 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3419 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3420 | ` * the stack and increment after that.` |
|        - |  3421 | ` */` |
|   168144 |  3422 | `case PH7_OP_INCR:` |
|        - |  3423 | `#ifdef UNTRUST` |
|        - |  3424 | `	if( pTos < pStack ){` |
|        - |  3425 | `		goto Abort;` |
|        - |  3426 | `	}` |
|        - |  3427 | `#endif` |
|   336334 |  3428 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   336334 |  3429 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3430 | `			ph7_value *pObj;` |
|   336334 |  3431 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3432 | `				/* Force a numeric cast */` |
|   336334 |  3433 | `				PH7_MemObjToNumeric(pObj);` |
|   336334 |  3434 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3435 | `					pObj->rVal++;` |
|        - |  3436 | `					/* Try to get an integer representation */` |
|      ! 0 |  3437 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3438 | `				}else{` |
|   336334 |  3439 | `					pObj->x.iVal++;` |
|   336334 |  3440 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3441 | `				}` |
|   336334 |  3442 | `				if( pInstr->iP1 ){` |
|        - |  3443 | `					/* Pre-icrement */` |
|       71 |  3444 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3445 | `				}` |
|   168188 |  3446 | `			}` |
|   168190 |  3447 | `		}else{` |
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
|   168188 |  3462 | `	}` |
|   336334 |  3463 | `	break;` |
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
|    22096 |  3518 | `case PH7_OP_UMINUS:` |
|        - |  3519 | `#ifdef UNTRUST` |
|        - |  3520 | `	if( pTos < pStack ){` |
|        - |  3521 | `		goto Abort;` |
|        - |  3522 | `	}` |
|        - |  3523 | `#endif` |
|        - |  3524 | `	/* Force a numeric (integer,real or both) cast */` |
|    44194 |  3525 | `	PH7_MemObjToNumeric(pTos);` |
|    44194 |  3526 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3527 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3528 | `	}` |
|    44194 |  3529 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    44170 |  3530 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22084 |  3531 | `	}` |
|    44194 |  3532 | `	break;` |
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
|    50598 |  3559 | `case PH7_OP_LNOT:` |
|        - |  3560 | `#ifdef UNTRUST` |
|        - |  3561 | `	if( pTos < pStack ){` |
|        - |  3562 | `		goto Abort;` |
|        - |  3563 | `	}` |
|        - |  3564 | `#endif` |
|        - |  3565 | `	/* Force a boolean cast */` |
|   101242 |  3566 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3567 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3568 | `	}` |
|   101242 |  3569 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   101242 |  3570 | `	break;` |
|        - |  3571 | `/*` |
|        - |  3572 | ` * OP_BITNOT: * * *` |
|        - |  3573 | ` *` |
|        - |  3574 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3575 | ` * with its ones-complement.` |
|        - |  3576 | ` */` |
|        3 |  3577 | `case PH7_OP_BITNOT:` |
|        - |  3578 | `#ifdef UNTRUST` |
|        - |  3579 | `	if( pTos < pStack ){` |
|        - |  3580 | `		goto Abort;` |
|        - |  3581 | `	}` |
|        - |  3582 | `#endif` |
|        - |  3583 | `	/* Force an integer cast */` |
|        7 |  3584 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3585 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3586 | `	}` |
|        7 |  3587 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3588 | `	break;` |
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
|       19 |  3978 | `case PH7_OP_BAND:` |
|        - |  3979 | `case PH7_OP_BOR:` |
|        - |  3980 | `case PH7_OP_BXOR:{` |
|       39 |  3981 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3982 | `	sxi64 a,b,r;` |
|        - |  3983 | `#ifdef UNTRUST` |
|        - |  3984 | `	if( pNos < pStack ){` |
|        - |  3985 | `		goto Abort;` |
|        - |  3986 | `	}` |
|        - |  3987 | `#endif` |
|        - |  3988 | `	/* Force the operands to be integer */` |
|       39 |  3989 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3990 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3991 | `	}` |
|       39 |  3992 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3993 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3994 | `	}` |
|        - |  3995 | `	/* Perform the requested operation */` |
|       39 |  3996 | `	a = pNos->x.iVal;` |
|       39 |  3997 | `	b = pTos->x.iVal;` |
|       39 |  3998 | `	switch(pInstr->iOp){` |
|        6 |  3999 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4000 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4001 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4002 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  4003 | `	case PH7_OP_BAND_STORE:` |
|        7 |  4004 | `	case PH7_OP_BAND:` |
|       15 |  4005 | `	default:          r = a&b; break;` |
|        - |  4006 | `	}` |
|        - |  4007 | `	/* Push the result */` |
|       39 |  4008 | `	pNos->x.iVal = r;` |
|       39 |  4009 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  4010 | `	VmPopOperand(&pTos,1);` |
|       39 |  4011 | `	break;` |
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
|    59189 |  4173 | `case PH7_OP_CAT:{` |
|        - |  4174 | `	ph7_value *pNos,*pCur;` |
|   118380 |  4175 | `	if( pInstr->iP1 < 1 ){` |
|    91500 |  4176 | `		pNos = &pTos[-1];` |
|    45751 |  4177 | `	}else{` |
|    26882 |  4178 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4179 | `	}` |
|        - |  4180 | `#ifdef UNTRUST` |
|        - |  4181 | `	if( pNos < pStack ){` |
|        - |  4182 | `		goto Abort;` |
|        - |  4183 | `	}` |
|        - |  4184 | `#endif` |
|        - |  4185 | `	/* Force a string cast */` |
|   118380 |  4186 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      930 |  4187 | `		PH7_MemObjToString(pNos);` |
|      464 |  4188 | `	}` |
|   118380 |  4189 | `	pCur = &pNos[1];` |
|   238594 |  4190 | `	while( pCur <= pTos ){` |
|   120216 |  4191 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50418 |  4192 | `			PH7_MemObjToString(pCur);` |
|    25208 |  4193 | `		}` |
|        - |  4194 | `		/* Perform the concatenation */` |
|   120216 |  4195 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   120178 |  4196 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    60088 |  4197 | `		}` |
|   120216 |  4198 | `		SyBlobRelease(&pCur->sBlob);` |
|   120216 |  4199 | `		pCur++;` |
|        2 |  4200 | `	}` |
|   118380 |  4201 | `	pTos = pNos;` |
|   118380 |  4202 | `	break;` |
|        - |  4203 | `				}` |
|        - |  4204 | `/*  CAT_STORE: * * *` |
|        - |  4205 | ` *` |
|        - |  4206 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4207 | ` * back.` |
|        - |  4208 | ` */` |
|     2802 |  4209 | `case PH7_OP_CAT_STORE:{` |
|     5606 |  4210 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4211 | `	ph7_value *pObj;` |
|        - |  4212 | `#ifdef UNTRUST` |
|        - |  4213 | `	if( pNos < pStack ){` |
|        - |  4214 | `		goto Abort;` |
|        - |  4215 | `	}` |
|        - |  4216 | `#endif` |
|     5606 |  4217 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4218 | `		/* Force a string cast */` |
|      ! 0 |  4219 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4220 | `	}` |
|     5606 |  4221 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4222 | `		/* Force a string cast */` |
|      ! 0 |  4223 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4224 | `	}` |
|        - |  4225 | `	/* Perform the concatenation (Reverse order) */` |
|     5606 |  4226 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     5606 |  4227 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2802 |  4228 | `	}` |
|        - |  4229 | `	/* Perform the store operation */` |
|     5606 |  4230 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4231 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5606 |  4232 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     5606 |  4233 | `		PH7_MemObjStore(pTos,pObj);` |
|     2802 |  4234 | `	}` |
|     5606 |  4235 | `	PH7_MemObjStore(pTos,pNos);` |
|     5606 |  4236 | `	VmPopOperand(&pTos,1);` |
|     5606 |  4237 | `	break;` |
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
|   107685 |  4251 | `case PH7_OP_LAND:` |
|        - |  4252 | `case PH7_OP_LOR: {` |
|   215416 |  4253 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4254 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4255 | `#ifdef UNTRUST` |
|        - |  4256 | `	if( pNos < pStack ){` |
|        - |  4257 | `		goto Abort;` |
|        - |  4258 | `	}` |
|        - |  4259 | `#endif` |
|        - |  4260 | `	/* Force a boolean cast */` |
|   215416 |  4261 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4262 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4263 | `	}` |
|   215416 |  4264 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4265 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4266 | `	}` |
|   215416 |  4267 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   215416 |  4268 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   215416 |  4269 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4270 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   108240 |  4271 | `		v1 = and_logic[v1*3+v2];` |
|    54143 |  4272 | `	}else{` |
|        - |  4273 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   107178 |  4274 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4275 | `	}` |
|   215416 |  4276 | `	if( v1 == 2 ){` |
|      ! 0 |  4277 | `		v1 = 1;` |
|      ! 0 |  4278 | `	}` |
|   215416 |  4279 | `	VmPopOperand(&pTos,1);` |
|   215416 |  4280 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   215416 |  4281 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   215416 |  4282 | `	break;` |
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
|     3701 |  4330 | `case PH7_OP_EQ:` |
|        - |  4331 | `case PH7_OP_NEQ: {` |
|     7404 |  4332 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4333 | `	/* Perform the comparison and act accordingly */` |
|        - |  4334 | `#ifdef UNTRUST` |
|        - |  4335 | `	if( pNos < pStack ){` |
|        - |  4336 | `		goto Abort;` |
|        - |  4337 | `	}` |
|        - |  4338 | `#endif` |
|     7404 |  4339 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7404 |  4340 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       15 |  4341 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7397 |  4342 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7364 |  4343 | `		rc = rc == 0;` |
|     3683 |  4344 | `	}else{` |
|       28 |  4345 | `		rc = rc != 0;` |
|        - |  4346 | `	}` |
|     7404 |  4347 | `	VmPopOperand(&pTos,1);` |
|     7404 |  4348 | `	if( !pInstr->iP2 ){` |
|        - |  4349 | `		/* Push comparison result without taking the jump */` |
|     7404 |  4350 | `		PH7_MemObjRelease(pTos);` |
|     7404 |  4351 | `		pTos->x.iVal = rc;` |
|        - |  4352 | `		/* Invalidate any prior representation */` |
|     7404 |  4353 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3703 |  4354 | `	}else{` |
|      ! 0 |  4355 | `		if( rc ){` |
|        - |  4356 | `			/* Jump to the desired location */` |
|      ! 0 |  4357 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4358 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4359 | `		}` |
|        - |  4360 | `	}` |
|     7404 |  4361 | `	break;` |
|        - |  4362 | `				 }` |
|        - |  4363 | `/* OP_TEQ P1 P2 *` |
|        - |  4364 | ` *` |
|        - |  4365 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4366 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4367 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4368 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4369 | ` */` |
|   127083 |  4370 | `case PH7_OP_TEQ: {` |
|   254168 |  4371 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4372 | `	/* Perform the comparison and act accordingly */` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `	if( pNos < pStack ){` |
|        - |  4375 | `		goto Abort;` |
|        - |  4376 | `	}` |
|        - |  4377 | `#endif` |
|   254168 |  4378 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   254168 |  4379 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4380 | `		rc = 0;` |
|        2 |  4381 | `	}else{` |
|   254166 |  4382 | `		rc = rc == 0;` |
|        - |  4383 | `	}` |
|   254168 |  4384 | `	VmPopOperand(&pTos,1);` |
|   254168 |  4385 | `	if( !pInstr->iP2 ){` |
|        - |  4386 | `		/* Push comparison result without taking the jump */` |
|   254168 |  4387 | `		PH7_MemObjRelease(pTos);` |
|   254168 |  4388 | `		pTos->x.iVal = rc;` |
|        - |  4389 | `		/* Invalidate any prior representation */` |
|   254168 |  4390 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   127085 |  4391 | `	}else{` |
|      ! 0 |  4392 | `		if( rc ){` |
|        - |  4393 | `			/* Jump to the desired location */` |
|      ! 0 |  4394 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4395 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4396 | `		}` |
|        - |  4397 | `	}` |
|   254168 |  4398 | `	break;` |
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
|   100890 |  4409 | `case PH7_OP_TNE: {` |
|   201782 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|   201782 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   201782 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4419 | `		rc = 1;` |
|        2 |  4420 | `	}else{` |
|   201780 |  4421 | `		rc = rc != 0;` |
|        - |  4422 | `	}` |
|   201782 |  4423 | `	VmPopOperand(&pTos,1);` |
|   201782 |  4424 | `	if( !pInstr->iP2 ){` |
|        - |  4425 | `		/* Push comparison result without taking the jump */` |
|   201782 |  4426 | `		PH7_MemObjRelease(pTos);` |
|   201782 |  4427 | `		pTos->x.iVal = rc;` |
|        - |  4428 | `		/* Invalidate any prior representation */` |
|   201782 |  4429 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100892 |  4430 | `	}else{` |
|      ! 0 |  4431 | `		if( rc ){` |
|        - |  4432 | `			/* Jump to the desired location */` |
|      ! 0 |  4433 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4434 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4435 | `		}` |
|        - |  4436 | `	}` |
|   201782 |  4437 | `	break;` |
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
|   116916 |  4457 | `case PH7_OP_LT:` |
|        - |  4458 | `case PH7_OP_LE: {` |
|   233878 |  4459 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4460 | `	/* Perform the comparison and act accordingly */` |
|        - |  4461 | `#ifdef UNTRUST` |
|        - |  4462 | `	if( pNos < pStack ){` |
|        - |  4463 | `		goto Abort;` |
|        - |  4464 | `	}` |
|        - |  4465 | `#endif` |
|   233878 |  4466 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   233878 |  4467 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4468 | `		rc = 0;` |
|   233874 |  4469 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4470 | `		rc = rc < 1;` |
|      198 |  4471 | `	}else{` |
|   233476 |  4472 | `		rc = rc < 0;` |
|        - |  4473 | `	}` |
|   233878 |  4474 | `	VmPopOperand(&pTos,1);` |
|   233878 |  4475 | `	if( !pInstr->iP2 ){` |
|        - |  4476 | `		/* Push comparison result without taking the jump */` |
|   233878 |  4477 | `		PH7_MemObjRelease(pTos);` |
|   233878 |  4478 | `		pTos->x.iVal = rc;` |
|        - |  4479 | `		/* Invalidate any prior representation */` |
|   233878 |  4480 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   116962 |  4481 | `	}else{` |
|      ! 0 |  4482 | `		if( rc ){` |
|        - |  4483 | `			/* Jump to the desired location */` |
|      ! 0 |  4484 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4485 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4486 | `		}` |
|        - |  4487 | `	}` |
|   233878 |  4488 | `	break;` |
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
|    51260 |  4508 | `case PH7_OP_GT:` |
|        - |  4509 | `case PH7_OP_GE: {` |
|   102522 |  4510 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4511 | `	/* Perform the comparison and act accordingly */` |
|        - |  4512 | `#ifdef UNTRUST` |
|        - |  4513 | `	if( pNos < pStack ){` |
|        - |  4514 | `		goto Abort;` |
|        - |  4515 | `	}` |
|        - |  4516 | `#endif` |
|   102522 |  4517 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   102522 |  4518 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4519 | `		rc = 0;` |
|   102518 |  4520 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   102366 |  4521 | `		rc = rc >= 0;` |
|    51184 |  4522 | `	}else{` |
|      150 |  4523 | `		rc = rc > 0;` |
|        - |  4524 | `	}` |
|   102522 |  4525 | `	VmPopOperand(&pTos,1);` |
|   102522 |  4526 | `	if( !pInstr->iP2 ){` |
|        - |  4527 | `		/* Push comparison result without taking the jump */` |
|   102522 |  4528 | `		PH7_MemObjRelease(pTos);` |
|   102522 |  4529 | `		pTos->x.iVal = rc;` |
|        - |  4530 | `		/* Invalidate any prior representation */` |
|   102522 |  4531 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    51262 |  4532 | `	}else{` |
|      ! 0 |  4533 | `		if( rc ){` |
|        - |  4534 | `			/* Jump to the desired location */` |
|      ! 0 |  4535 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4536 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4537 | `		}` |
|        - |  4538 | `	}` |
|   102522 |  4539 | `	break;` |
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
|       50 |  4681 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4682 | `				/* Safely ignore the exception frame */` |
|       21 |  4683 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4684 | `			}` |
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
|       10 |  4733 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4734 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4735 | `	VmFrame *pFrameLocal;` |
|       22 |  4736 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4737 | `	/* Create the exception frame */` |
|       22 |  4738 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4739 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4740 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4741 | `		goto Abort;` |
|        - |  4742 | `	}` |
|        - |  4743 | `	/* Mark the special frame */` |
|       22 |  4744 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4745 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4746 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4747 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4748 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4749 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4750 | `	}` |
|       22 |  4751 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4752 | `	break;` |
|        - |  4753 | `							}` |
|        - |  4754 | `/*` |
|        - |  4755 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4756 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4757 | ` */` |
|        9 |  4758 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4759 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4760 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4761 | `		ph7_exception **apException;` |
|        - |  4762 | `		/* Pop the loaded exception */` |
|        7 |  4763 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4764 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4765 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4766 | `		}` |
|        3 |  4767 | `	}` |
|       20 |  4768 | `	pException->pFrame = 0;` |
|        - |  4769 | `	/* Leave the exception frame */` |
|       20 |  4770 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4771 | `	break;` |
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
|       30 |  4786 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4787 | `		/* Safely ignore the exception frame */` |
|        8 |  4788 | `		pFrameLocal = pFrameLocal->pParent;` |
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
|     4586 |  4831 | `case PH7_OP_FOREACH_INIT: {` |
|     9174 |  4832 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4833 | `	void *pName;` |
|        - |  4834 | `#ifdef UNTRUST` |
|        - |  4835 | `	if( pTos < pStack ){` |
|        - |  4836 | `		goto Abort;` |
|        - |  4837 | `	}` |
|        - |  4838 | `#endif` |
|     9174 |  4839 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
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
|     9174 |  4852 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
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
|     9174 |  4865 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4866 | `		/* Jump out of the loop */` |
|      ! 0 |  4867 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4868 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4869 | `		}` |
|      ! 0 |  4870 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4871 | `	}else{` |
|        - |  4872 | `		ph7_foreach_step *pStep;` |
|     9174 |  4873 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9174 |  4874 | `		if( pStep == 0 ){` |
|      ! 0 |  4875 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4876 | `			/* Jump out of the loop */` |
|      ! 0 |  4877 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4878 | `		}else{` |
|        - |  4879 | `			/* Zero the structure */` |
|     9174 |  4880 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4881 | `			/* Prepare the step */` |
|     9174 |  4882 | `			pStep->iFlags = pInfo->iFlags;` |
|     9174 |  4883 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9166 |  4884 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4885 | `				/* Reset the internal loop cursor */` |
|     9166 |  4886 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4887 | `				/* Mark the step */` |
|     9166 |  4888 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9166 |  4889 | `				pStep->xIter.pMap = pMap;` |
|     9166 |  4890 | `				pMap->iRef++;` |
|     4584 |  4891 | `			}else{` |
|        9 |  4892 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4893 | `				/* Reset the loop cursor */` |
|        9 |  4894 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4895 | `				/* Mark the step */` |
|        9 |  4896 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4897 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4898 | `				pThis->iRef++;` |
|        - |  4899 | `			}` |
|        - |  4900 | `		}` |
|     9174 |  4901 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4902 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4903 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4904 | `			/* Jump out of the loop */` |
|      ! 0 |  4905 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4906 | `		}` |
|        - |  4907 | `	}` |
|     9174 |  4908 | `	VmPopOperand(&pTos,1);` |
|     9174 |  4909 | `	break;` |
|        - |  4910 | `						  }` |
|        - |  4911 | `/*` |
|        - |  4912 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4913 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4914 | ` */` |
|    73445 |  4915 | `case PH7_OP_FOREACH_STEP: {` |
|   146892 |  4916 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4917 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4918 | `	ph7_value *pValue;` |
|        - |  4919 | `	VmFrame *pFrameLocal;` |
|        - |  4920 | `	/* Peek the last step */` |
|   146892 |  4921 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   146892 |  4922 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   146892 |  4923 | `	pFrameLocal = pVm->pFrame;` |
|   151924 |  4924 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4925 | `		/* Safely ignore the exception frame */` |
|     5033 |  4926 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4927 | `	}` |
|   146892 |  4928 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   146868 |  4929 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4930 | `		ph7_hashmap_node *pNode;` |
|        - |  4931 | `		/* Extract the current node value */` |
|   146868 |  4932 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   146868 |  4933 | `		if( pNode == 0 ){` |
|        - |  4934 | `			/* No more entry to process */` |
|     9166 |  4935 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9166 |  4936 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4937 | `				/* Break the reference with the last element */` |
|        5 |  4938 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4939 | `			}` |
|        - |  4940 | `			/* Automatically reset the loop cursor */` |
|     9166 |  4941 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4942 | `			/* Cleanup the mess left behind */` |
|     9166 |  4943 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9166 |  4944 | `			SySetPop(&pInfo->aStep);` |
|     9166 |  4945 | `			PH7_HashmapUnref(pMap);` |
|     4584 |  4946 | `		}else{` |
|   137704 |  4947 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      408 |  4948 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      408 |  4949 | `				if( pKey ){` |
|      408 |  4950 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      203 |  4951 | `				}` |
|      203 |  4952 | `			}` |
|   137704 |  4953 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
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
|   137692 |  4965 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   137692 |  4966 | `				if( pValue ){` |
|   137692 |  4967 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    68845 |  4968 | `				}` |
|        - |  4969 | `			}` |
|        - |  4970 | `		}` |
|    73435 |  4971 | `	}else{` |
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
|   146892 |  5028 | `	break;` |
|        - |  5029 | `						  }` |
|        - |  5030 | `/*` |
|        - |  5031 | ` * OP_MEMBER P1 P2` |
|        - |  5032 | ` * Load class attribute/method on the stack.` |
|        - |  5033 | ` */` |
|     1740 |  5034 | `case PH7_OP_MEMBER: {` |
|        - |  5035 | `	ph7_class_instance *pThis;` |
|        - |  5036 | `	ph7_value *pNos;` |
|        - |  5037 | `	SyString sName;` |
|     3482 |  5038 | `	if( !pInstr->iP1 ){` |
|     3424 |  5039 | `		pNos = &pTos[-1];` |
|        - |  5040 | `#ifdef UNTRUST` |
|        - |  5041 | `		if( pNos < pStack ){` |
|        - |  5042 | `			goto Abort;` |
|        - |  5043 | `		}` |
|        - |  5044 | `#endif` |
|     3424 |  5045 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5046 | `			ph7_class *pClass;` |
|        - |  5047 | `			/* Class already instantiated */` |
|     3424 |  5048 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5049 | `			/* Point to the instantiated class */` |
|     3424 |  5050 | `			pClass = pThis->pClass;` |
|        - |  5051 | `			/* Extract attribute name first */` |
|     3424 |  5052 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3424 |  5053 | `			if( pInstr->iP2 ){` |
|        - |  5054 | `				/* Method call */` |
|      120 |  5055 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5056 | `				if( sName.nByte > 0 ){` |
|        - |  5057 | `					/* Extract the target method */` |
|      120 |  5058 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5059 | `				}` |
|      120 |  5060 | `				if( pMeth == 0 ){` |
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
|      120 |  5071 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5072 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5073 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5074 | `				}` |
|      120 |  5075 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5076 | `			}else{` |
|        - |  5077 | `				/* Attribute access */` |
|     3306 |  5078 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5079 | `				SyHashEntry *pEntry;` |
|        - |  5080 | `				/* Extract the target attribute */` |
|     3306 |  5081 | `				if( sName.nByte > 0 ){` |
|     3306 |  5082 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3306 |  5083 | `					if( pEntry ){` |
|        - |  5084 | `						/* Point to the attribute value */` |
|     3304 |  5085 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1651 |  5086 | `					}` |
|     1652 |  5087 | `				}` |
|     3306 |  5088 | `				if( pObjAttr == 0 ){` |
|        - |  5089 | `					/* No such attribute,load null */` |
|        4 |  5090 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5091 | `						&pClass->sName,&sName);` |
|        - |  5092 | `					/* Call the __get magic method if available */` |
|        3 |  5093 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5094 | `				}` |
|     3306 |  5095 | `				VmPopOperand(&pTos,1);` |
|        - |  5096 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5097 | `				 * This is due to the following case:` |
|        - |  5098 | `				 *     (new TestClass())->foo;` |
|        - |  5099 | `				 */` |
|     3306 |  5100 | `				pThis->iRef++;` |
|     3306 |  5101 | `				PH7_MemObjRelease(pTos);` |
|     3306 |  5102 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3306 |  5103 | `				if( pObjAttr ){` |
|     3304 |  5104 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5105 | `					/* Check attribute access */` |
|     3304 |  5106 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5107 | `						/* Load attribute */` |
|     3304 |  5108 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3304 |  5109 | `						if( pValue ){` |
|     3304 |  5110 | `							if( pThis->iRef < 2 ){` |
|        - |  5111 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5112 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5113 | `								 */` |
|        3 |  5114 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5115 | `							}else{` |
|        - |  5116 | `								/* Simple load */` |
|     3302 |  5117 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5118 | `							}` |
|     3304 |  5119 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3302 |  5120 | `								if( pThis->iRef > 1 ){` |
|        - |  5121 | `									/* Load attribute index */` |
|     3300 |  5122 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1649 |  5123 | `								}` |
|     1650 |  5124 | `							}` |
|     1651 |  5125 | `						}` |
|     1651 |  5126 | `					}` |
|     1651 |  5127 | `				}` |
|        - |  5128 | `				/* Safely unreference the object */` |
|     3306 |  5129 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5130 | `			}` |
|     1713 |  5131 | `		}else{` |
|      ! 0 |  5132 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5133 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5134 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5135 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5136 | `		}` |
|     1713 |  5137 | `	}else{` |
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
|     3482 |  5281 | `	break;` |
|        - |  5282 | `					}` |
|        - |  5283 | `/*` |
|        - |  5284 | ` * OP_NEW P1 * * *` |
|        - |  5285 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5286 | ` */` |
|      255 |  5287 | `case PH7_OP_NEW: {` |
|      512 |  5288 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      512 |  5289 | `	ph7_class *pClass = 0;` |
|        - |  5290 | `	ph7_class_instance *pNew;` |
|      512 |  5291 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5292 | `		/* Try to extract the desired class */` |
|      767 |  5293 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      510 |  5294 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      255 |  5295 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5296 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5297 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5298 | `	}` |
|      512 |  5299 | `	if( pClass == 0 ){` |
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
|      512 |  5312 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      512 |  5313 | `		if( pNew == 0 ){` |
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
|      512 |  5326 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      512 |  5327 | `		if( pCons == 0 ){` |
|      454 |  5328 | `			SyString *pName = &pClass->sName;` |
|        - |  5329 | `			/* Check for a constructor with the same base class name */` |
|      454 |  5330 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      226 |  5331 | `		}` |
|      512 |  5332 | `		if( pCons ){` |
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
|      512 |  5361 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5362 | `			/* Pop given arguments */` |
|       44 |  5363 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       21 |  5364 | `		}` |
|      512 |  5365 | `		PH7_MemObjRelease(pTos);` |
|      512 |  5366 | `		pTos->x.pOther = pNew;` |
|      512 |  5367 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5368 | `	}` |
|      512 |  5369 | `	break;` |
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
|   281404 |  5456 | `case PH7_OP_CALL: {` |
|   562854 |  5457 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5458 | `	SyHashEntry *pEntry;` |
|        - |  5459 | `	SyString sName;` |
|        - |  5460 | `	/* Extract function name */` |
|   562854 |  5461 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
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
|   281183 |  5496 | `		break;` |
|        - |  5497 | `	}` |
|   562852 |  5498 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5499 | `	/* Check for a compiled function first */` |
|   562852 |  5500 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   562852 |  5501 | `	if( pEntry ){` |
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
|    11396 |  5512 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    11396 |  5513 | `		pThis = 0;` |
|    11396 |  5514 | `		pSelf = 0;` |
|    11396 |  5515 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5516 | `			ph7_class_method *pMeth;` |
|        - |  5517 | `			/* Class method call */` |
|     1230 |  5518 | `			ph7_value *pTarget = &pTos[-1];` |
|     1230 |  5519 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5520 | `				/* Extract the 'this' pointer */` |
|     1230 |  5521 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5522 | `					/* Instance already loaded */` |
|     1200 |  5523 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1200 |  5524 | `					pThis->iRef++;` |
|     1200 |  5525 | `					pSelf = pThis->pClass;` |
|      599 |  5526 | `				}` |
|     1230 |  5527 | `				if( pSelf == 0 ){` |
|       31 |  5528 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5529 | `						/* "Late Static Binding" class name */` |
|       37 |  5530 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5531 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5532 | `					}` |
|       31 |  5533 | `					if( pSelf == 0 ){` |
|        7 |  5534 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5535 | `					}` |
|       15 |  5536 | `				}` |
|     1230 |  5537 | `				if( pThis == 0  ){` |
|       31 |  5538 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5539 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5540 | `						/* Safely ignore the exception frame */` |
|        3 |  5541 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5542 | `					}` |
|       31 |  5543 | `					if( pFrameLocal->pParent ){` |
|        - |  5544 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5545 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5546 | `						if( pThis ){` |
|       13 |  5547 | `							pThis->iRef++;` |
|        6 |  5548 | `						}` |
|        9 |  5549 | `					}` |
|       15 |  5550 | `				}` |
|     1230 |  5551 | `				VmPopOperand(&pTos,1);` |
|     1230 |  5552 | `				PH7_MemObjRelease(pTos);` |
|        - |  5553 | `				/* Synchronize pointers */` |
|     1230 |  5554 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5555 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5556 | `				 * user have already computed the random generated unique class method name` |
|        - |  5557 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5558 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5559 | `				 */` |
|     1230 |  5560 | `				while( pArg < pStack ){` |
|      ! 0 |  5561 | `					pArg++;` |
|      ! 0 |  5562 | `				}` |
|     1230 |  5563 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5564 | `					/* Check if the call is allowed */` |
|     1230 |  5565 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1230 |  5566 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
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
|      614 |  5577 | `				}` |
|      614 |  5578 | `			}` |
|      614 |  5579 | `		}` |
|        - |  5580 | `		/* Check The recursion limit */` |
|    11396 |  5581 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
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
|    11394 |  5593 | `		if( pVmFunc->pNextName ){` |
|        - |  5594 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5595 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5596 | `		}` |
|        - |  5597 | `		/* Extract the formal argument set */` |
|    11394 |  5598 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5599 | `		/* Create a new VM frame  */` |
|    11394 |  5600 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    11394 |  5601 | `		if( rc != SXRET_OK ){` |
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
|    11394 |  5614 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5615 | `			/* Install the '$this' variable */` |
|        - |  5616 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1210 |  5617 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1210 |  5618 | `			if( pObj ){` |
|        - |  5619 | `				/* Reflect the change */` |
|     1210 |  5620 | `				pObj->x.pOther = pThis;` |
|     1210 |  5621 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      604 |  5622 | `			}` |
|      604 |  5623 | `		}` |
|    11394 |  5624 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
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
|    11394 |  5651 | `		n = 0;` |
|    31932 |  5652 | `		while( pArg < pTos ){` |
|    20540 |  5653 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    20390 |  5654 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5655 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5656 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5657 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5658 | `						goto Abort;` |
|        - |  5659 | `					}` |
|      ! 0 |  5660 | `				}` |
|        - |  5661 | `				/* Make sure the given arguments are of the correct type */` |
|    20390 |  5662 | `				if( aFormalArg[n].nType > 0 ){` |
|     1082 |  5663 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
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
|     1082 |  5689 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5690 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5691 | `						/* Cast to the desired type */` |
|      ! 0 |  5692 | `						xCast(pArg);` |
|      ! 0 |  5693 | `					}` |
|      540 |  5694 | `				}` |
|    20390 |  5695 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
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
|    20344 |  5721 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5722 | `				}` |
|    10196 |  5723 | `			}else{` |
|        - |  5724 | `				char zName[32];` |
|        - |  5725 | `				SyString sArgName;` |
|        - |  5726 | `				/* Set a dummy name */` |
|      152 |  5727 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5728 | `				sArgName.zString = zName;` |
|        - |  5729 | `				/* Annonymous argument */` |
|      152 |  5730 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5731 | `			}` |
|    20540 |  5732 | `			if( pObj ){` |
|    20494 |  5733 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5734 | `				/* Insert argument index  */` |
|    20494 |  5735 | `				sArg.nIdx = pObj->nIdx;` |
|    20494 |  5736 | `				sArg.pUserData = 0;` |
|    20494 |  5737 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10246 |  5738 | `			}` |
|    20540 |  5739 | `			PH7_MemObjRelease(pArg);` |
|    20540 |  5740 | `			pArg++;` |
|    20540 |  5741 | `			++n;` |
|        2 |  5742 | `		}` |
|        - |  5743 | `		/* Set up closure environment */` |
|    11394 |  5744 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
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
|    13162 |  5766 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1770 |  5767 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1764 |  5768 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1764 |  5769 | `				if( pObj ){` |
|        - |  5770 | `					/* Evaluate the default value and extract it's result */` |
|     1764 |  5771 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1764 |  5772 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5773 | `						goto Abort;` |
|        - |  5774 | `					}` |
|        - |  5775 | `					/* Insert argument index */` |
|     1764 |  5776 | `					sArg.nIdx = pObj->nIdx;` |
|     1764 |  5777 | `					sArg.pUserData = 0;` |
|     1764 |  5778 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5779 | `					/* Make sure the default argument is of the correct type */` |
|     1764 |  5780 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5781 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5782 | `						/* Cast to the desired type */` |
|      ! 0 |  5783 | `						xCast(pObj);` |
|      ! 0 |  5784 | `					}` |
|      881 |  5785 | `				}` |
|      881 |  5786 | `			}` |
|     1770 |  5787 | `			++n;` |
|        2 |  5788 | `		}` |
|        - |  5789 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5790 | `		 * does not return anything.` |
|        - |  5791 | `		 */` |
|    11394 |  5792 | `		PH7_MemObjRelease(pTos);` |
|    11394 |  5793 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5794 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    11394 |  5795 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    11394 |  5796 | `		if( pFrameStack == 0 ){` |
|        - |  5797 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5798 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5799 | `				&pVmFunc->sName);` |
|      ! 0 |  5800 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5801 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5802 | `			}` |
|      ! 0 |  5803 | `			break;` |
|        - |  5804 | `		}` |
|    11394 |  5805 | `		if( pSelf ){` |
|        - |  5806 | `			/* Push class name */` |
|     1228 |  5807 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      613 |  5808 | `		}` |
|        - |  5809 | `		/* Increment nesting level */` |
|    11394 |  5810 | `		pVm->nRecursionDepth++;` |
|        - |  5811 | `		/* Execute function body */` |
|    11394 |  5812 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5813 | `		/* Decrement nesting level */` |
|    11394 |  5814 | `		pVm->nRecursionDepth--;` |
|    11394 |  5815 | `		if( pSelf ){` |
|        - |  5816 | `			/* Pop class name */` |
|     1228 |  5817 | `			(void)SySetPop(&pVm->aSelf);` |
|      613 |  5818 | `		}` |
|        - |  5819 | `		/* Cleanup the mess left behind */` |
|    11394 |  5820 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
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
|    11394 |  5848 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
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
|    11394 |  5867 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5868 | `		/* Leave the frame */` |
|    11394 |  5869 | `		VmLeaveFrame(&(*pVm));` |
|    11394 |  5870 | `		if( rc == PH7_ABORT ){` |
|        - |  5871 | `			/* Abort processing immeditaley */` |
|        7 |  5872 | `			goto Abort;` |
|    11388 |  5873 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5874 | `			goto Exception;` |
|        - |  5875 | `		}` |
|     5694 |  5876 | `	}else{` |
|        - |  5877 | `		ph7_user_func *pFunc;` |
|        - |  5878 | `		ph7_context sCtx;` |
|        - |  5879 | `		ph7_value sRet;` |
|        - |  5880 | `		/* Look for an installed foreign function */` |
|   551458 |  5881 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   551458 |  5882 | `		if( pEntry == 0 ){` |
|        - |  5883 | `			/* Call to undefined function */` |
|        5 |  5884 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5885 | `			/* Pop given arguments */` |
|        5 |  5886 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5887 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5888 | `			}` |
|        - |  5889 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5890 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5891 | `			break;` |
|        - |  5892 | `		}` |
|   551454 |  5893 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5894 | `		/* Start collecting function arguments */` |
|   551454 |  5895 | `		SySetReset(&aArg);` |
|  1463522 |  5896 | `		while( pArg < pTos ){` |
|   912070 |  5897 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   912070 |  5898 | `			pArg++;` |
|        2 |  5899 | `		}` |
|        - |  5900 | `		/* Assume a null return value */` |
|   551454 |  5901 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5902 | `		/* Init the call context */` |
|   551454 |  5903 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5904 | `		/* Call the foreign function */` |
|   551454 |  5905 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5906 | `		/* Release the call context */` |
|   551454 |  5907 | `		VmReleaseCallContext(&sCtx);` |
|   551454 |  5908 | `		if( rc == PH7_ABORT ){` |
|      437 |  5909 | `			goto Abort;` |
|   551018 |  5910 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5911 | `			goto Exception;` |
|        - |  5912 | `		}` |
|   551016 |  5913 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5914 | `			/* Pop function name and arguments */` |
|   533776 |  5915 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   266909 |  5916 | `		}` |
|        - |  5917 | `		/* Save foreign function return value */` |
|   551016 |  5918 | `		PH7_MemObjStore(&sRet,pTos);` |
|   551016 |  5919 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5920 | `	}` |
|   562400 |  5921 | `	break;` |
|        - |  5922 | `				  }` |
|        - |  5923 | `/*` |
|        - |  5924 | ` * OP_CONSUME: P1 * *` |
|        - |  5925 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5926 | ` */` |
|    10152 |  5927 | `case PH7_OP_CONSUME: {` |
|    20306 |  5928 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    20306 |  5929 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5930 |  |
|    20306 |  5931 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    20306 |  5932 | `	pCur = pOut;` |
|        - |  5933 | `	/* Start the consume process  */` |
|    40610 |  5934 | `	while( pOut <= pTos ){` |
|        - |  5935 | `		/* Force a string cast */` |
|    20306 |  5936 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      166 |  5937 | `			PH7_MemObjToString(pOut);` |
|       82 |  5938 | `		}` |
|    20306 |  5939 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5940 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5941 | `			/* Invoke the output consumer callback */` |
|    10838 |  5942 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10838 |  5943 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5944 | `				/* Increment output length */` |
|     4438 |  5945 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2218 |  5946 | `			}` |
|    10838 |  5947 | `			SyBlobRelease(&pOut->sBlob);` |
|    10838 |  5948 | `			if( rc == SXERR_ABORT ){` |
|        - |  5949 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5950 | `				goto Abort;` |
|        - |  5951 | `			}` |
|     5418 |  5952 | `		}` |
|    20306 |  5953 | `		pOut++;` |
|        2 |  5954 | `	}` |
|    20306 |  5955 | `	pTos = &pCur[-1];` |
|    20304 |  5956 | `	break;` |
|        - |  5957 | `					 }` |
|        - |  5958 |  |
|        - |  5959 | `		} /* Switch() */` |
|  9952282 |  5960 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5961 | `	} /* For(;;) */` |
|    14170 |  5962 | `Done:` |
|    28342 |  5963 | `	SySetRelease(&aArg);` |
|    28342 |  5964 | `	return SXRET_OK;` |
|      225 |  5965 | `Abort:` |
|      451 |  5966 | `	SySetRelease(&aArg);` |
|     1575 |  5967 | `	while( pTos >= pStack ){` |
|     1125 |  5968 | `		PH7_MemObjRelease(pTos);` |
|     1125 |  5969 | `		pTos--;` |
|        1 |  5970 | `	}` |
|      451 |  5971 | `	return PH7_ABORT;` |
|        2 |  5972 | `Exception:` |
|        5 |  5973 | `	SySetRelease(&aArg);` |
|        9 |  5974 | `	while( pTos >= pStack ){` |
|        5 |  5975 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5976 | `		pTos--;` |
|        1 |  5977 | `	}` |
|        5 |  5978 | `	return PH7_EXCEPTION;` |
|    14399 |  5979 |  |
|        - |  5980 | `/*` |
|        - |  5981 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5982 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5983 | ` * See block-comment on that function for additional information.` |
|        - |  5984 | ` */` |
|    13944 |  5985 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5986 |  |
|        - |  5987 | `	ph7_value *pStack;` |
|        - |  5988 | `	sxi32 rc;` |
|        - |  5989 | `	/* Allocate a new operand stack */` |
|    13946 |  5990 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    13946 |  5991 | `	if( pStack == 0 ){` |
|      ! 0 |  5992 | `		return SXERR_MEM;` |
|        - |  5993 | `	}` |
|        - |  5994 | `	/* Execute the program */` |
|    13946 |  5995 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  5996 | `	/* Free the operand stack */` |
|    13946 |  5997 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  5998 | `	/* Execution result */` |
|    13946 |  5999 | `	return rc;` |
|     6974 |  6000 |  |
|        - |  6001 | `/*` |
|        - |  6002 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6003 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6004 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6005 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6006 | ` * execution ends.` |
|        - |  6007 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6008 | ` * additional information.` |
|        - |  6009 | ` */` |
|     1926 |  6010 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6011 |  |
|        - |  6012 | `	VmShutdownCB *pEntry;` |
|        - |  6013 | `	ph7_value *apArg[10];` |
|        - |  6014 | `	sxu32 n,nEntry;` |
|        - |  6015 | `	int i;` |
|        - |  6016 | `	/* Point to the stack of registered callbacks */` |
|     1928 |  6017 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    21188 |  6018 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    19262 |  6019 | `		apArg[i] = 0;` |
|     9632 |  6020 | `	}` |
|     1930 |  6021 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6022 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6023 | `		if( pEntry ){` |
|        - |  6024 | `			/* Prepare callback arguments if any */` |
|        3 |  6025 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6026 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6027 | `					break;` |
|        - |  6028 | `				}` |
|      ! 0 |  6029 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6030 | `			}` |
|        - |  6031 | `			/* Invoke the callback */` |
|        3 |  6032 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6033 | `			/*` |
|        - |  6034 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6035 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6036 | `			 */` |
|        3 |  6037 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6038 | `			if( pEntry ){` |
|        3 |  6039 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6040 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6041 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6042 | `				}` |
|        1 |  6043 | `			}` |
|        1 |  6044 | `		}` |
|        2 |  6045 | `	}` |
|     1928 |  6046 | `	SySetReset(&pVm->aShutdown);` |
|     1928 |  6047 |  |
|        - |  6048 | `/*` |
|        - |  6049 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6050 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6051 | ` * See block-comment on that function for additional information.` |
|        - |  6052 | ` */` |
|     1934 |  6053 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6054 |  |
|        - |  6055 | `	/* Make sure we are ready to execute this program */` |
|     1936 |  6056 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6057 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6058 | `	}` |
|        - |  6059 | `	/* Set the execution magic number  */` |
|     1936 |  6060 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6061 | `	/* Execute the program */` |
|     1936 |  6062 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6063 | `	/* Invoke any shutdown callbacks */` |
|     1932 |  6064 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6065 | `	/*` |
|        - |  6066 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6067 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6068 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6069 | `	 */` |
|     1932 |  6070 | `	return SXRET_OK;` |
|      969 |  6071 |  |
|        - |  6072 | `/*` |
|        - |  6073 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6074 | ` * the desired message.` |
|        - |  6075 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6076 | ` * in 'api.c' for additional information.` |
|        - |  6077 | ` */` |
|      350 |  6078 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6079 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6080 | `	SyString *pString /* Message to output */` |
|        - |  6081 | `	)` |
|        2 |  6082 |  |
|      352 |  6083 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6084 | `	sxi32 rc = SXRET_OK;` |
|        - |  6085 | `	/* Call the output consumer */` |
|      352 |  6086 | `	if( pString->nByte > 0 ){` |
|      352 |  6087 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6088 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6089 | `			/* Increment output length */` |
|       17 |  6090 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6091 | `		}` |
|      175 |  6092 | `	}` |
|      352 |  6093 | `	return rc;` |
|        2 |  6094 |  |
|        - |  6095 | `/*` |
|        - |  6096 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6097 | ` * callback to consume the formatted message.` |
|        - |  6098 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6099 | ` * in 'api.c' for additional information.` |
|        - |  6100 | ` */` |
|        2 |  6101 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6102 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6103 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6104 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6105 | `	)` |
|        1 |  6106 |  |
|        3 |  6107 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6108 | `	sxi32 rc = SXRET_OK;` |
|        - |  6109 | `	SyBlob sWorker;` |
|        - |  6110 | `	/* Format the message and call the output consumer */` |
|        3 |  6111 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6112 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6113 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6114 | `		/* Consume the formatted message */` |
|        3 |  6115 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6116 | `	}` |
|        3 |  6117 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6118 | `		/* Increment output length */` |
|      ! 0 |  6119 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6120 | `	}` |
|        - |  6121 | `	/* Release the working buffer */` |
|        3 |  6122 | `	SyBlobRelease(&sWorker);` |
|        3 |  6123 | `	return rc;` |
|        1 |  6124 |  |
|        - |  6125 | `/*` |
|        - |  6126 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6127 | ` * This function never fail and always return a pointer` |
|        - |  6128 | ` * to a null terminated string.` |
|        - |  6129 | ` */` |
|       10 |  6130 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6131 |  |
|       11 |  6132 | `	const char *zOp = "Unknown     ";` |
|       11 |  6133 | `	switch(nOp){` |
|        3 |  6134 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6135 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6136 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6137 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6138 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6139 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6140 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6141 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6142 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6143 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6144 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6145 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6146 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6147 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6148 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6149 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6150 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6151 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6152 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6153 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6154 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6157 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6162 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6168 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6169 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6181 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6182 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6207 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6216 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6218 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6221 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6223 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6224 | `	default:` |
|      ! 0 |  6225 | `		break;` |
|        - |  6226 | `	}` |
|       11 |  6227 | `	return zOp;` |
|        1 |  6228 |  |
|        - |  6229 | `/*` |
|        - |  6230 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6231 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6232 | ` * is responsible of consuming the generated dump.` |
|        - |  6233 | ` */` |
|        2 |  6234 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6235 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6236 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6237 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6238 | `	)` |
|        1 |  6239 |  |
|        - |  6240 | `	sxi32 rc;` |
|        3 |  6241 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6242 | `	return rc;` |
|        1 |  6243 |  |
|        - |  6244 | `/*` |
|        - |  6245 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6246 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6247 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6248 | ` * in 'compile.c' for additional information.` |
|        - |  6249 | ` */` |
|        8 |  6250 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6251 |  |
|        9 |  6252 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6253 | `	/* Evaluate and expand constant value */` |
|        9 |  6254 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6255 |  |
|        - |  6256 | `/*` |
|        - |  6257 | ` * Section:` |
|        - |  6258 | ` *  Function handling functions.` |
|        - |  6259 | ` * Status:` |
|        - |  6260 | ` *    Stable.` |
|        - |  6261 | ` */` |
|        - |  6262 | `/*` |
|        - |  6263 | ` * int func_num_args(void)` |
|        - |  6264 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6265 | ` * Parameters` |
|        - |  6266 | ` *   None.` |
|        - |  6267 | ` * Return` |
|        - |  6268 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6269 | ` *  or -1 if called from the globe scope.` |
|        - |  6270 | ` */` |
|      900 |  6271 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6272 |  |
|        - |  6273 | `	VmFrame *pFrame;` |
|        - |  6274 | `	ph7_vm *pVm;` |
|        - |  6275 | `	/* Point to the target VM */` |
|      902 |  6276 | `	pVm = pCtx->pVm;` |
|        - |  6277 | `	/* Current frame */` |
|      902 |  6278 | `	pFrame = pVm->pFrame;` |
|      902 |  6279 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6280 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6281 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6282 | `	}` |
|      902 |  6283 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6284 | `		SXUNUSED(nArg);` |
|      ! 0 |  6285 | `		SXUNUSED(apArg);` |
|        - |  6286 | `		/* Global frame,return -1 */` |
|      ! 0 |  6287 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6288 | `		return SXRET_OK;` |
|        - |  6289 | `	}` |
|        - |  6290 | `	/* Total number of arguments passed to the enclosing function */` |
|      902 |  6291 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      902 |  6292 | `	ph7_result_int(pCtx,nArg);` |
|      902 |  6293 | `	return SXRET_OK;` |
|      452 |  6294 |  |
|        - |  6295 | `/*` |
|        - |  6296 | ` * value func_get_arg(int $arg_num)` |
|        - |  6297 | ` *   Return an item from the argument list.` |
|        - |  6298 | ` * Parameters` |
|        - |  6299 | ` *  Argument number(index start from zero).` |
|        - |  6300 | ` * Return` |
|        - |  6301 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6302 | ` */` |
|       22 |  6303 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6304 |  |
|       24 |  6305 | `	ph7_value *pObj = 0;` |
|       24 |  6306 | `	VmSlot *pSlot = 0;` |
|        - |  6307 | `	VmFrame *pFrame;` |
|        - |  6308 | `	ph7_vm *pVm;` |
|        - |  6309 | `	/* Point to the target VM */` |
|       24 |  6310 | `	pVm = pCtx->pVm;` |
|        - |  6311 | `	/* Current frame */` |
|       24 |  6312 | `	pFrame = pVm->pFrame;` |
|       24 |  6313 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6314 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6315 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6316 | `	}` |
|       24 |  6317 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6318 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6319 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6320 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6321 | `		return SXRET_OK;` |
|        - |  6322 | `	}` |
|        - |  6323 | `	/* Extract the desired index */` |
|       21 |  6324 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6325 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6326 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6327 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6328 | `		return SXRET_OK;` |
|        - |  6329 | `	}` |
|        - |  6330 | `	/* Extract the desired argument */` |
|       21 |  6331 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6332 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6333 | `			/* Return the desired argument */` |
|       21 |  6334 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6335 | `		}else{` |
|        - |  6336 | `			/* No such argument,return false */` |
|      ! 0 |  6337 | `			ph7_result_bool(pCtx,0);` |
|        - |  6338 | `		}` |
|       11 |  6339 | `	}else{` |
|        - |  6340 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6341 | `		ph7_result_bool(pCtx,0);` |
|        - |  6342 | `	}` |
|       21 |  6343 | `	return SXRET_OK;` |
|       13 |  6344 |  |
|        - |  6345 | `/*` |
|        - |  6346 | ` * array func_get_args_byref(void)` |
|        - |  6347 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6348 | ` * Parameters` |
|        - |  6349 | ` *  None.` |
|        - |  6350 | ` * Return` |
|        - |  6351 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6352 | ` *  member of the current user-defined function's argument list.` |
|        - |  6353 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6354 | ` * NOTE:` |
|        - |  6355 | ` *  Arguments are returned to the array by reference.` |
|        - |  6356 | ` */` |
|        2 |  6357 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6358 |  |
|        - |  6359 | `	ph7_value *pArray;` |
|        - |  6360 | `	VmFrame *pFrame;` |
|        - |  6361 | `	VmSlot *aSlot;` |
|        - |  6362 | `	sxu32 n;` |
|        - |  6363 | `	/* Point to the current frame */` |
|        3 |  6364 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6365 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6366 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6367 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6368 | `	}` |
|        3 |  6369 | `	if( pFrame->pParent == 0 ){` |
|        - |  6370 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6371 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6372 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6373 | `		return SXRET_OK;` |
|        - |  6374 | `	}` |
|        - |  6375 | `	/* Create a new array */` |
|        3 |  6376 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6377 | `	if( pArray == 0 ){` |
|      ! 0 |  6378 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6379 | `		SXUNUSED(apArg);` |
|      ! 0 |  6380 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6381 | `		return SXRET_OK;` |
|        - |  6382 | `	}` |
|        - |  6383 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6384 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6385 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6386 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6387 | `	}` |
|        - |  6388 | `	/* Return the freshly created array */` |
|        3 |  6389 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6390 | `	return SXRET_OK;` |
|        2 |  6391 |  |
|        - |  6392 | `/*` |
|        - |  6393 | ` * array func_get_args(void)` |
|        - |  6394 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6395 | ` * Parameters` |
|        - |  6396 | ` *  None.` |
|        - |  6397 | ` * Return` |
|        - |  6398 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6399 | ` *  member of the current user-defined function's argument list.` |
|        - |  6400 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6401 | ` */` |
|       62 |  6402 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6403 |  |
|       64 |  6404 | `	ph7_value *pObj = 0;` |
|        - |  6405 | `	ph7_value *pArray;` |
|        - |  6406 | `	VmFrame *pFrame;` |
|        - |  6407 | `	VmSlot *aSlot;` |
|        - |  6408 | `	sxu32 n;` |
|        - |  6409 | `	/* Point to the current frame */` |
|       64 |  6410 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6411 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6412 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6413 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6414 | `	}` |
|       64 |  6415 | `	if( pFrame->pParent == 0 ){` |
|        - |  6416 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6417 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6418 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6419 | `		return SXRET_OK;` |
|        - |  6420 | `	}` |
|        - |  6421 | `	/* Create a new array */` |
|       64 |  6422 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6423 | `	if( pArray == 0 ){` |
|      ! 0 |  6424 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6425 | `		SXUNUSED(apArg);` |
|      ! 0 |  6426 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6427 | `		return SXRET_OK;` |
|        - |  6428 | `	}` |
|        - |  6429 | `	/* Start filling the array with the given arguments */` |
|       64 |  6430 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6431 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6432 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6433 | `		if( pObj ){` |
|      130 |  6434 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6435 | `		}` |
|       66 |  6436 | `	}` |
|        - |  6437 | `	/* Return the freshly created array */` |
|       64 |  6438 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6439 | `	return SXRET_OK;` |
|       33 |  6440 |  |
|        - |  6441 | `/*` |
|        - |  6442 | ` * bool function_exists(string $name)` |
|        - |  6443 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6444 | ` * Parameters` |
|        - |  6445 | ` *  The name of the desired function.` |
|        - |  6446 | ` * Return` |
|        - |  6447 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6448 | ` */` |
|     1662 |  6449 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6450 |  |
|        - |  6451 | `	const char *zName;` |
|        - |  6452 | `	ph7_vm *pVm;` |
|        - |  6453 | `	int nLen;` |
|        - |  6454 | `	int res;` |
|     1664 |  6455 | `	if( nArg < 1 ){` |
|        - |  6456 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6457 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6458 | `		return SXRET_OK;` |
|        - |  6459 | `	}` |
|        - |  6460 | `	/* Point to the target VM */` |
|     1664 |  6461 | `	pVm = pCtx->pVm;` |
|        - |  6462 | `	/* Extract the function name */` |
|     1664 |  6463 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6464 | `	/* Assume the function is not defined */` |
|     1664 |  6465 | `	res = 0;` |
|        - |  6466 | `	/* Perform the lookup */` |
|     2493 |  6467 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1658 |  6468 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6469 | `			/* Function is defined */` |
|      206 |  6470 | `			res = 1;` |
|      102 |  6471 | `	}` |
|     1664 |  6472 | `	ph7_result_bool(pCtx,res);` |
|     1664 |  6473 | `	return SXRET_OK;` |
|      833 |  6474 |  |
|        - |  6475 | `/*` |
|        - |  6476 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6477 | ` * [i.e: Whether it is callable or not].` |
|        - |  6478 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6479 | ` */` |
|    15956 |  6480 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6481 |  |
|    15958 |  6482 | `	int res = 0;` |
|    15958 |  6483 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6484 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6485 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6486 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6487 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6488 | `		if( pMethod && CallInvoke ){` |
|        - |  6489 | `			ph7_value sResult;` |
|        - |  6490 | `			sxi32 rc;` |
|        - |  6491 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6492 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6493 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6494 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6495 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6496 | `			}` |
|      ! 0 |  6497 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6498 | `		}` |
|    15958 |  6499 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6500 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6501 | `		if( pMap->nEntry == 2 ){` |
|        - |  6502 | `			ph7_class *pClass;` |
|        - |  6503 | `			ph7_value *pV;` |
|        - |  6504 | `			/* Extract the target class */` |
|       12 |  6505 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6506 | `			if( pV ){` |
|       12 |  6507 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6508 | `				if( pClass ){` |
|        - |  6509 | `					ph7_class_method *pMethod;` |
|        - |  6510 | `					/* Extract the target method */` |
|       10 |  6511 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6512 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6513 | `						/* Perform the lookup */` |
|       10 |  6514 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6515 | `						if( pMethod ){` |
|        - |  6516 | `							/* Method is callable */` |
|        5 |  6517 | `							res = 1;` |
|        2 |  6518 | `						}` |
|        4 |  6519 | `					}` |
|        4 |  6520 | `				}` |
|        5 |  6521 | `			}` |
|        7 |  6522 | `		}` |
|    15945 |  6523 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6524 | `		const char *zName;` |
|        - |  6525 | `		int nLen;` |
|        - |  6526 | `		/* Extract the name */` |
|     4700 |  6527 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6528 | `		/* Perform the lookup */` |
|     4715 |  6529 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6530 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6531 | `				/* Function is callable */` |
|     4682 |  6532 | `				res = 1;` |
|     2340 |  6533 | `		}` |
|     2349 |  6534 | `	}` |
|    15958 |  6535 | `	return res;` |
|        2 |  6536 |  |
|        - |  6537 | `/*` |
|        - |  6538 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6539 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6540 | ` * Parameters` |
|        - |  6541 | ` * $name` |
|        - |  6542 | ` *    The callback function to check` |
|        - |  6543 | ` * $syntax_only` |
|        - |  6544 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6545 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6546 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6547 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6548 | ` *    a string.` |
|        - |  6549 | ` * Return` |
|        - |  6550 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6551 | ` */` |
|       14 |  6552 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6553 |  |
|        - |  6554 | `	ph7_vm *pVm;` |
|        - |  6555 | `	int res;` |
|       15 |  6556 | `	if( nArg < 1 ){` |
|        - |  6557 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6558 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6559 | `		return SXRET_OK;` |
|        - |  6560 | `	}` |
|        - |  6561 | `	/* Point to the target VM */` |
|       15 |  6562 | `	pVm = pCtx->pVm;` |
|        - |  6563 | `	/* Perform the requested operation */` |
|       15 |  6564 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6565 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6566 | `	return SXRET_OK;` |
|        8 |  6567 |  |
|        - |  6568 | `/*` |
|        - |  6569 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6570 | ` * defined below.` |
|        - |  6571 | ` */` |
|     1082 |  6572 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6573 |  |
|     1083 |  6574 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6575 | `	ph7_value sName;` |
|        - |  6576 | `	sxi32 rc;` |
|        - |  6577 | `	/* Prepare the function name for insertion */` |
|     1083 |  6578 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6579 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6580 | `	/* Perform the insertion */` |
|     1083 |  6581 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6582 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6583 | `	return rc;` |
|        1 |  6584 |  |
|        - |  6585 | `/*` |
|        - |  6586 | ` * array get_defined_functions(void)` |
|        - |  6587 | ` *  Returns an array of all defined functions.` |
|        - |  6588 | ` * Parameter` |
|        - |  6589 | ` *  None.` |
|        - |  6590 | ` * Return` |
|        - |  6591 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6592 | ` *  both built-in (internal) and user-defined.` |
|        - |  6593 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6594 | ` *  defined ones using $arr["user"].` |
|        - |  6595 | ` * Note:` |
|        - |  6596 | ` *  NULL is returned on failure.` |
|        - |  6597 | ` */` |
|        2 |  6598 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6599 |  |
|        - |  6600 | `	ph7_value *pArray,*pEntry;` |
|        - |  6601 | `	/* NOTE:` |
|        - |  6602 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6603 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6604 | `	 */` |
|        3 |  6605 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6606 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6607 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6608 | `		SXUNUSED(apArg);` |
|        - |  6609 | `		/* Return NULL */` |
|      ! 0 |  6610 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6611 | `		return SXRET_OK;` |
|        - |  6612 | `	}` |
|        3 |  6613 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6614 | `	if( pEntry == 0 ){` |
|        - |  6615 | `		/* Return NULL */` |
|      ! 0 |  6616 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6617 | `		return SXRET_OK;` |
|        - |  6618 | `	}` |
|        - |  6619 | `	/* Fill with the appropriate information */` |
|        3 |  6620 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6621 | `	/* Create the 'internal' index */` |
|        3 |  6622 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6623 | `	/* Create the user-func array */` |
|        3 |  6624 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6625 | `	if( pEntry == 0 ){` |
|        - |  6626 | `		/* Return NULL */` |
|      ! 0 |  6627 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6628 | `		return SXRET_OK;` |
|        - |  6629 | `	}` |
|        - |  6630 | `	/* Fill with the appropriate information */` |
|        3 |  6631 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6632 | `	/* Create the 'user' index */` |
|        3 |  6633 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6634 | `	/* Return the multi-dimensional array */` |
|        3 |  6635 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6636 | `	return SXRET_OK;` |
|        2 |  6637 |  |
|        - |  6638 | `/*` |
|        - |  6639 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6640 | ` *  Register a function for execution on shutdown.` |
|        - |  6641 | ` * Note` |
|        - |  6642 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6643 | ` *  be called in the same order as they were registered.` |
|        - |  6644 | ` * Parameters` |
|        - |  6645 | ` *  $callback` |
|        - |  6646 | ` *   The shutdown callback to register.` |
|        - |  6647 | ` * $param` |
|        - |  6648 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6649 | ` * Return` |
|        - |  6650 | ` *  Nothing.` |
|        - |  6651 | ` */` |
|        2 |  6652 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6653 |  |
|        - |  6654 | `	VmShutdownCB sEntry;` |
|        - |  6655 | `	int i,j;` |
|        3 |  6656 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6657 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6658 | `		return PH7_OK;` |
|        - |  6659 | `	}` |
|        - |  6660 | `	/* Zero the Entry */` |
|        3 |  6661 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6662 | `	/* Initialize fields */` |
|        3 |  6663 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6664 | `	/* Save the callback name for later invocation name */` |
|        3 |  6665 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6666 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6667 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6668 | `	}` |
|        - |  6669 | `	/* Copy arguments */` |
|        3 |  6670 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6671 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6672 | `			/* Limit reached */` |
|      ! 0 |  6673 | `			break;` |
|        - |  6674 | `		}` |
|      ! 0 |  6675 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6676 | `	}` |
|        3 |  6677 | `	sEntry.nArg = j;` |
|        - |  6678 | `	/* Install the callback */` |
|        3 |  6679 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6680 | `	return PH7_OK;` |
|        2 |  6681 |  |
|        - |  6682 | `/*` |
|        - |  6683 | ` * Section:` |
|        - |  6684 | ` *  Class handling functions.` |
|        - |  6685 | ` * Status:` |
|        - |  6686 | ` *    Stable.` |
|        - |  6687 | ` */` |
|        - |  6688 | `/*` |
|        - |  6689 | ` * Extract the top active class. NULL is returned` |
|        - |  6690 | ` * if the class stack is empty.` |
|        - |  6691 | ` */` |
|      484 |  6692 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6693 |  |
|      486 |  6694 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6695 | `	ph7_class **apClass;` |
|      486 |  6696 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6697 | `		/* Empty stack,return NULL */` |
|       15 |  6698 | `		return 0;` |
|        - |  6699 | `	}` |
|        - |  6700 | `	/* Peek the last entry */` |
|      472 |  6701 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      472 |  6702 | `	return apClass[pSet->nUsed - 1];` |
|      244 |  6703 |  |
|        - |  6704 | `/*` |
|        - |  6705 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6706 | ` *   Get the class that declared the currently executing method.` |
|        - |  6707 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6708 | ` *` |
|        - |  6709 | ` * Parameters` |
|        - |  6710 | ` *   pVm: Target VM` |
|        - |  6711 | ` *` |
|        - |  6712 | ` * Return` |
|        - |  6713 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6714 | ` *   - Not executing within a class method` |
|        - |  6715 | ` *` |
|        - |  6716 | ` * Note` |
|        - |  6717 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6718 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6719 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6720 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6721 | ` *   declaring class.` |
|        - |  6722 | ` */` |
|       18 |  6723 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6724 |  |
|       19 |  6725 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6726 | `	ph7_vm_func *pVmFunc;` |
|        - |  6727 |  |
|        - |  6728 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6729 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6730 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6731 | `	}` |
|        - |  6732 |  |
|        - |  6733 | `	/* Check if we're in a method context */` |
|       19 |  6734 | `	if( pFrame->pParent ){` |
|       15 |  6735 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6736 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6737 | `			/* Return the declaring class */` |
|       15 |  6738 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6739 | `		}` |
|      ! 0 |  6740 | `	}` |
|        - |  6741 |  |
|        5 |  6742 | `	return 0;` |
|       10 |  6743 |  |
|        - |  6744 |  |
|        - |  6745 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6746 | `/*` |
|        - |  6747 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6748 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6749 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6750 | ` * return value indicates failure.` |
|        - |  6751 | ` */` |
|     1086 |  6752 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6753 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6754 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6755 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6756 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6757 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6758 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6759 | `	)` |
|        2 |  6760 |  |
|        - |  6761 | `	ph7_value *aStack;` |
|        - |  6762 | `	VmInstr aInstr[2];` |
|        - |  6763 | `	int iCursor;` |
|        - |  6764 | `	int i;` |
|        - |  6765 | `	/* Create a new operand stack */` |
|     1088 |  6766 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1088 |  6767 | `	if( aStack == 0 ){` |
|      ! 0 |  6768 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6769 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6770 | `		return SXERR_MEM;` |
|        - |  6771 | `	}` |
|        - |  6772 | `	/* Fill the operand stack with the given arguments */` |
|     1602 |  6773 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      516 |  6774 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6775 | `		/*` |
|        - |  6776 | `		 * Symisc eXtension:` |
|        - |  6777 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6778 | `		 */` |
|      516 |  6779 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      259 |  6780 | `	}` |
|     1088 |  6781 | `	iCursor = nArg + 1;` |
|     1088 |  6782 | `	if( pThis ){` |
|        - |  6783 | `		/*` |
|        - |  6784 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6785 | `		 */` |
|     1082 |  6786 | `		pThis->iRef++; /* Increment reference count */` |
|     1082 |  6787 | `		aStack[i].x.pOther = pThis;` |
|     1082 |  6788 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      540 |  6789 | `	}` |
|     1088 |  6790 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1088 |  6791 | `	i++;` |
|        - |  6792 | `	/* Push method name */` |
|     1088 |  6793 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1088 |  6794 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1088 |  6795 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1088 |  6796 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6797 | `	/* Emit the CALL istruction */` |
|     1088 |  6798 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1088 |  6799 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1088 |  6800 | `	aInstr[0].iP2 = 0;` |
|     1088 |  6801 | `	aInstr[0].p3  = 0;` |
|        - |  6802 | `	/* Emit the DONE instruction */` |
|     1088 |  6803 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1088 |  6804 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1088 |  6805 | `	aInstr[1].iP2 = 0;` |
|     1088 |  6806 | `	aInstr[1].p3  = 0;` |
|        - |  6807 | `	/* Execute the method body (if available) */` |
|     1088 |  6808 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6809 | `	/* Clean up the mess left behind */` |
|     1088 |  6810 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1088 |  6811 | `	return PH7_OK;` |
|      545 |  6812 |  |
|        - |  6813 | `/*` |
|        - |  6814 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6815 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6816 | ` * in the apArg[] array.` |
|        - |  6817 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6818 | ` * return value indicates failure.` |
|        - |  6819 | ` */` |
|      898 |  6820 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6821 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6822 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6823 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6824 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6825 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6826 | `	)` |
|        2 |  6827 |  |
|        - |  6828 | `	ph7_value *aStack;` |
|        - |  6829 | `	VmInstr aInstr[2];` |
|        - |  6830 | `	int i;` |
|      900 |  6831 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6832 | `		/* Don't bother processing,it's invalid anyway */` |
|      443 |  6833 | `		if( pResult ){` |
|        - |  6834 | `			/* Assume a null return value */` |
|      ! 0 |  6835 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6836 | `		}` |
|      443 |  6837 | `		return SXERR_INVALID;` |
|        - |  6838 | `	}` |
|      458 |  6839 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6840 | `		/* Class method */` |
|       11 |  6841 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6842 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6843 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6844 | `		ph7_class *pClass = 0;` |
|        - |  6845 | `		ph7_value *pValue;` |
|        - |  6846 | `		sxi32 rc;` |
|       11 |  6847 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6848 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6849 | `			if( pResult ){` |
|        - |  6850 | `				/* Assume a null return value */` |
|      ! 0 |  6851 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6852 | `			}` |
|      ! 0 |  6853 | `			return SXRET_OK;` |
|        - |  6854 | `		}` |
|        - |  6855 | `		/* Extract the class name or an instance of it */` |
|       11 |  6856 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  6857 | `		if( pValue ){` |
|       11 |  6858 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  6859 | `		}` |
|       11 |  6860 | `		if( pClass == 0 ){` |
|        - |  6861 | `			/* No such class,return NULL */` |
|      ! 0 |  6862 | `			if( pResult ){` |
|      ! 0 |  6863 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6864 | `			}` |
|      ! 0 |  6865 | `			return SXRET_OK;` |
|        - |  6866 | `		}` |
|       11 |  6867 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6868 | `			/* Point to the class instance */` |
|        5 |  6869 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  6870 | `		}` |
|        - |  6871 | `		/* Try to extract the method */` |
|       11 |  6872 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  6873 | `		if( pValue ){` |
|       11 |  6874 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  6875 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  6876 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  6877 | `			}` |
|        5 |  6878 | `		}` |
|       11 |  6879 | `		if( pMethod == 0 ){` |
|        - |  6880 | `			/* No such method,return NULL */` |
|      ! 0 |  6881 | `			if( pResult ){` |
|      ! 0 |  6882 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6883 | `			}` |
|      ! 0 |  6884 | `			return SXRET_OK;` |
|        - |  6885 | `		}` |
|        - |  6886 | `		/* Call the class method */` |
|       11 |  6887 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  6888 | `		return rc;` |
|        - |  6889 | `	}` |
|        - |  6890 | `	/* Create a new operand stack */` |
|      448 |  6891 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  6892 | `	if( aStack == 0 ){` |
|      ! 0 |  6893 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6894 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  6895 | `		if( pResult ){` |
|        - |  6896 | `			/* Assume a null return value */` |
|      ! 0 |  6897 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6898 | `		}` |
|      ! 0 |  6899 | `		return SXERR_MEM;` |
|        - |  6900 | `	}` |
|        - |  6901 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  6902 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  6903 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6904 | `		/*` |
|        - |  6905 | `		 * Symisc eXtension:` |
|        - |  6906 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6907 | `		 */` |
|     1024 |  6908 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  6909 | `	}` |
|        - |  6910 | `	/* Push the function name */` |
|      448 |  6911 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  6912 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  6913 | `	/* Emit the CALL istruction */` |
|      448 |  6914 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  6915 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  6916 | `	aInstr[0].iP2 = 0;` |
|      448 |  6917 | `	aInstr[0].p3  = 0;` |
|        - |  6918 | `	/* Emit the DONE instruction */` |
|      448 |  6919 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  6920 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  6921 | `	aInstr[1].iP2 = 0;` |
|      448 |  6922 | `	aInstr[1].p3  = 0;` |
|        - |  6923 | `	/* Execute the function body (if available) */` |
|      448 |  6924 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  6925 | `	/* Clean up the mess left behind */` |
|      448 |  6926 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  6927 | `	return PH7_OK;` |
|      451 |  6928 |  |
|        - |  6929 | `/*` |
|        - |  6930 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  6931 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  6932 | ` * parameter.` |
|        - |  6933 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6934 | ` * return value indicates failure.` |
|        - |  6935 | ` */` |
|      236 |  6936 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  6937 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6938 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6939 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  6940 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  6941 | `	)` |
|        1 |  6942 |  |
|        - |  6943 | `	ph7_value *pArg;` |
|        - |  6944 | `	SySet aArg;` |
|        - |  6945 | `	va_list ap;` |
|        - |  6946 | `	sxi32 rc;` |
|      237 |  6947 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  6948 | `	/* Copy arguments one after one */` |
|      237 |  6949 | `	va_start(ap,pResult);` |
|      393 |  6950 | `	for(;;){` |
|      787 |  6951 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  6952 | `		if( pArg == 0 ){` |
|      237 |  6953 | `			break;` |
|        - |  6954 | `		}` |
|      551 |  6955 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  6956 | `	}` |
|        - |  6957 | `	/* Call the core routine */` |
|      237 |  6958 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  6959 | `	/* Cleanup */` |
|      237 |  6960 | `	SySetRelease(&aArg);` |
|      237 |  6961 | `	return rc;` |
|        1 |  6962 |  |
|        - |  6963 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  6964 | `/*` |
|        - |  6965 | ` * bool defined(string $name)` |
|        - |  6966 | ` *  Checks whether a given named constant exists.` |
|        - |  6967 | ` * Parameter:` |
|        - |  6968 | ` *  Name of the desired constant.` |
|        - |  6969 | ` * Return` |
|        - |  6970 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  6971 | ` */` |
|       14 |  6972 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6973 |  |
|        - |  6974 | `	const char *zName;` |
|       16 |  6975 | `	int nLen = 0;` |
|       16 |  6976 | `	int res = 0;` |
|       16 |  6977 | `	if( nArg < 1 ){` |
|        - |  6978 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  6979 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  6980 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6981 | `		return SXRET_OK;` |
|        - |  6982 | `	}` |
|        - |  6983 | `	/* Extract constant name */` |
|       16 |  6984 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6985 | `	/* Perform the lookup */` |
|       16 |  6986 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6987 | `		/* Already defined */` |
|       10 |  6988 | `		res = 1;` |
|        4 |  6989 | `	}` |
|       16 |  6990 | `	ph7_result_bool(pCtx,res);` |
|       16 |  6991 | `	return SXRET_OK;` |
|        9 |  6992 |  |
|        - |  6993 | `/*` |
|        - |  6994 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  6995 | ` * below.` |
|        - |  6996 | ` */` |
|        8 |  6997 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  6998 |  |
|       10 |  6999 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7000 | `	/* Expand constant value */` |
|       10 |  7001 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7002 |  |
|        - |  7003 | `/*` |
|        - |  7004 | ` * bool define(string $constant_name,expression value)` |
|        - |  7005 | ` *  Defines a named constant at runtime.` |
|        - |  7006 | ` * Parameter:` |
|        - |  7007 | ` *  $constant_name` |
|        - |  7008 | ` *   The name of the constant` |
|        - |  7009 | ` *  $value` |
|        - |  7010 | ` *   Constant value` |
|        - |  7011 | ` * Return:` |
|        - |  7012 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7013 | ` */` |
|       10 |  7014 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7015 |  |
|        - |  7016 | `	const char *zName;  /* Constant name */` |
|        - |  7017 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7018 | `	int nLen = 0;       /* Name length */` |
|        - |  7019 | `	sxi32 rc;` |
|       12 |  7020 | `	if( nArg < 2 ){` |
|        - |  7021 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7022 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7023 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7024 | `		return SXRET_OK;` |
|        - |  7025 | `	}` |
|       12 |  7026 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7027 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7028 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7029 | `		return SXRET_OK;` |
|        - |  7030 | `	}` |
|        - |  7031 | `	/* Extract constant name */` |
|       12 |  7032 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7033 | `	if( nLen < 1 ){` |
|      ! 0 |  7034 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7035 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7036 | `		return SXRET_OK;` |
|        - |  7037 | `	}` |
|        - |  7038 | `	/* Duplicate constant value */` |
|       12 |  7039 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7040 | `	if( pValue == 0 ){` |
|      ! 0 |  7041 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7042 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7043 | `		return SXRET_OK;` |
|        - |  7044 | `	}` |
|        - |  7045 | `	/* Initialize the memory object */` |
|       12 |  7046 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7047 | `	/* Register the constant */` |
|       12 |  7048 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7049 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7050 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7051 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7052 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7053 | `		return SXRET_OK;` |
|        - |  7054 | `	}` |
|        - |  7055 | `	/* Duplicate constant value */` |
|       12 |  7056 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7057 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7058 | `		/* Lower case the constant name */` |
|      ! 0 |  7059 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7060 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7061 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7062 | `				/* UTF-8 stream */` |
|      ! 0 |  7063 | `				zCur++;` |
|      ! 0 |  7064 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7065 | `					zCur++;` |
|      ! 0 |  7066 | `				}` |
|      ! 0 |  7067 | `				continue;` |
|        - |  7068 | `			}` |
|      ! 0 |  7069 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7070 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7071 | `				zCur[0] = (char)c;` |
|      ! 0 |  7072 | `			}` |
|      ! 0 |  7073 | `			zCur++;` |
|      ! 0 |  7074 | `		}` |
|        - |  7075 | `		/* Finally,register the constant */` |
|      ! 0 |  7076 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7077 | `	}` |
|        - |  7078 | `	/* All done,return TRUE */` |
|       12 |  7079 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7080 | `	return SXRET_OK;` |
|        7 |  7081 |  |
|        - |  7082 | `/*` |
|        - |  7083 | ` * value constant(string $name)` |
|        - |  7084 | ` *  Returns the value of a constant` |
|        - |  7085 | ` * Parameter` |
|        - |  7086 | ` *  $name` |
|        - |  7087 | ` *    Name of the constant.` |
|        - |  7088 | ` * Return` |
|        - |  7089 | ` *  Constant value or NULL if not defined.` |
|        - |  7090 | ` */` |
|        8 |  7091 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7092 |  |
|        - |  7093 | `	SyHashEntry *pEntry;` |
|        - |  7094 | `	ph7_constant *pCons;` |
|        - |  7095 | `	const char *zName; /* Constant name */` |
|        - |  7096 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7097 | `	int nLen;` |
|       10 |  7098 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7099 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7100 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7101 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7102 | `		return SXRET_OK;` |
|        - |  7103 | `	}` |
|        - |  7104 | `	/* Extract the constant name */` |
|       10 |  7105 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7106 | `	/* Perform the query */` |
|       10 |  7107 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7108 | `	if( pEntry == 0 ){` |
|        3 |  7109 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7110 | `		ph7_result_null(pCtx);` |
|        3 |  7111 | `		return SXRET_OK;` |
|        - |  7112 | `	}` |
|        8 |  7113 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7114 | `	/* Point to the structure that describe the constant */` |
|        8 |  7115 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7116 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7117 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7118 | `	/* Return that value */` |
|        8 |  7119 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7120 | `	/* Cleanup */` |
|        8 |  7121 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7122 | `	return SXRET_OK;` |
|        6 |  7123 |  |
|        - |  7124 | `/*` |
|        - |  7125 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7126 | ` * defined below.` |
|        - |  7127 | ` */` |
|      414 |  7128 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7129 |  |
|      415 |  7130 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7131 | `	ph7_value sName;` |
|        - |  7132 | `	sxi32 rc;` |
|        - |  7133 | `	/* Prepare the constant name for insertion */` |
|      415 |  7134 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  7135 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7136 | `	/* Perform the insertion */` |
|      415 |  7137 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  7138 | `	PH7_MemObjRelease(&sName);` |
|      415 |  7139 | `	return rc;` |
|        1 |  7140 |  |
|        - |  7141 | `/*` |
|        - |  7142 | ` * array get_defined_constants(void)` |
|        - |  7143 | ` *  Returns an associative array with the names of all defined` |
|        - |  7144 | ` *  constants.` |
|        - |  7145 | ` * Parameters` |
|        - |  7146 | ` *  NONE.` |
|        - |  7147 | ` * Returns` |
|        - |  7148 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7149 | ` */` |
|        2 |  7150 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7151 |  |
|        - |  7152 | `	ph7_value *pArray;` |
|        - |  7153 | `	/* Create the array first*/` |
|        3 |  7154 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7155 | `	if( pArray == 0 ){` |
|      ! 0 |  7156 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7157 | `		SXUNUSED(apArg);` |
|        - |  7158 | `		/* Return NULL */` |
|      ! 0 |  7159 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7160 | `		return SXRET_OK;` |
|        - |  7161 | `	}` |
|        - |  7162 | `	/* Fill the array with the defined constants */` |
|        3 |  7163 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7164 | `	/* Return the created array */` |
|        3 |  7165 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7166 | `	return SXRET_OK;` |
|        2 |  7167 |  |
|        - |  7168 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7169 | `/*` |
|        - |  7170 | ` * Section:` |
|        - |  7171 | ` *  Random numbers/string generators.` |
|        - |  7172 | ` * Status:` |
|        - |  7173 | ` *    Stable.` |
|        - |  7174 | ` */` |
|        - |  7175 | `/*` |
|        - |  7176 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7177 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7178 | ` * used by te SQLite3 library.` |
|        - |  7179 | ` */` |
|     2005 |  7180 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7181 |  |
|        - |  7182 | `	sxu32 iNum;` |
|     2007 |  7183 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2007 |  7184 | `	return iNum;` |
|        2 |  7185 |  |
|        - |  7186 | `/*` |
|        - |  7187 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7188 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7189 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7190 | ` * by te SQLite3 library.` |
|        - |  7191 | ` */` |
|    63342 |  7192 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7193 |  |
|        - |  7194 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7195 | `	int i;` |
|        - |  7196 | `	/* Generate a binary string first */` |
|    63344 |  7197 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7198 | `	/* Turn the binary string into english based alphabet */` |
|   696932 |  7199 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   633590 |  7200 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   316796 |  7201 | `	 }` |
|    63344 |  7202 |  |
|        - |  7203 | `/*` |
|        - |  7204 | ` * int rand()` |
|        - |  7205 | ` * int mt_rand()` |
|        - |  7206 | ` * int rand(int $min,int $max)` |
|        - |  7207 | ` * int mt_rand(int $min,int $max)` |
|        - |  7208 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7209 | ` * Parameter` |
|        - |  7210 | ` *  $min` |
|        - |  7211 | ` *    The lowest value to return (default: 0)` |
|        - |  7212 | ` *  $max` |
|        - |  7213 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7214 | ` * Return` |
|        - |  7215 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7216 | ` * Note:` |
|        - |  7217 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7218 | ` *  by te SQLite3 library.` |
|        - |  7219 | ` */` |
|       20 |  7220 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7221 |  |
|        - |  7222 | `	sxu32 iNum;` |
|        - |  7223 | `	/* Generate the random number */` |
|       21 |  7224 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7225 | `	if( nArg > 1 ){` |
|        - |  7226 | `		sxu32 iMin,iMax;` |
|        3 |  7227 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7228 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7229 | `		if( iMin < iMax ){` |
|        3 |  7230 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7231 | `			if( iDiv > 0 ){` |
|        3 |  7232 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7233 | `			}` |
|        1 |  7234 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7235 | `			iNum %= iMax;` |
|      ! 0 |  7236 | `		}` |
|        1 |  7237 | `	}` |
|        - |  7238 | `	/* Return the number */` |
|       21 |  7239 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7240 | `	return SXRET_OK;` |
|        1 |  7241 |  |
|        - |  7242 | `/*` |
|        - |  7243 | ` * int getrandmax(void)` |
|        - |  7244 | ` * int mt_getrandmax(void)` |
|        - |  7245 | ` * int rc4_getrandmax(void)` |
|        - |  7246 | ` *   Show largest possible random value` |
|        - |  7247 | ` * Return` |
|        - |  7248 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7249 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7250 | ` * Note:` |
|        - |  7251 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7252 | ` *  by te SQLite3 library.` |
|        - |  7253 | ` */` |
|        4 |  7254 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7255 |  |
|        2 |  7256 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7257 | `	SXUNUSED(apArg);` |
|        5 |  7258 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7259 | `	return SXRET_OK;` |
|        1 |  7260 |  |
|        - |  7261 | `/*` |
|        - |  7262 | ` * string rand_str()` |
|        - |  7263 | ` * string rand_str(int $len)` |
|        - |  7264 | ` *  Generate a random string (English alphabet).` |
|        - |  7265 | ` * Parameter` |
|        - |  7266 | ` *  $len` |
|        - |  7267 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7268 | ` * Return` |
|        - |  7269 | ` *   A pseudo random string.` |
|        - |  7270 | ` * Note:` |
|        - |  7271 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7272 | ` *  by te SQLite3 library.` |
|        - |  7273 | ` *  This function is a symisc extension.` |
|        - |  7274 | ` */` |
|      120 |  7275 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7276 |  |
|        - |  7277 | `	char zString[1024];` |
|      122 |  7278 | `	int iLen = 0x10;` |
|      122 |  7279 | `	if( nArg > 0 ){` |
|        - |  7280 | `		/* Get the desired length */` |
|      122 |  7281 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7282 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7283 | `			/* Default length */` |
|        3 |  7284 | `			iLen = 0x10;` |
|        1 |  7285 | `		}` |
|       60 |  7286 | `	}` |
|        - |  7287 | `	/* Generate the random string */` |
|      122 |  7288 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7289 | `	/* Return the generated string */` |
|      122 |  7290 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7291 | `	return SXRET_OK;` |
|        2 |  7292 |  |
|        - |  7293 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7294 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7295 | `/* Unique ID private data */` |
|        - |  7296 | `struct unique_id_data` |
|        - |  7297 |  |
|        - |  7298 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7299 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7300 | `};` |
|        - |  7301 | `/*` |
|        - |  7302 | ` * Binary to hex consumer callback.` |
|        - |  7303 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7304 | ` * defined below.` |
|        - |  7305 | ` */` |
|      192 |  7306 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7307 |  |
|      193 |  7308 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7309 | `	sxu32 nBuflen;` |
|        - |  7310 | `	/* Extract result buffer length */` |
|      193 |  7311 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7312 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7313 | `			/*` |
|        - |  7314 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7315 | `			 * string will be 13 characters long` |
|        - |  7316 | `			 */` |
|       25 |  7317 | `		return SXERR_ABORT;` |
|        - |  7318 | `	}` |
|      169 |  7319 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7320 | `		return SXERR_ABORT;` |
|        - |  7321 | `	}` |
|        - |  7322 | `	/* Safely Consume the hex stream */` |
|      169 |  7323 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7324 | `	return SXRET_OK;` |
|       97 |  7325 |  |
|        - |  7326 | `/*` |
|        - |  7327 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7328 | ` *  Generate a unique ID` |
|        - |  7329 | ` * Parameter` |
|        - |  7330 | ` * $prefix` |
|        - |  7331 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7332 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7333 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7334 | ` * $more_entropy` |
|        - |  7335 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7336 | ` *  that the result will be unique.` |
|        - |  7337 | ` * Return` |
|        - |  7338 | ` *  Returns the unique identifier, as a string.` |
|        - |  7339 | ` */` |
|       24 |  7340 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7341 |  |
|        - |  7342 | `	struct unique_id_data sUniq;` |
|        - |  7343 | `	unsigned char zDigest[20];` |
|       25 |  7344 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7345 | `	const char *zPrefix;` |
|        - |  7346 | `	SHA1Context sCtx;` |
|        - |  7347 | `	char zRandom[7];` |
|        - |  7348 | `	int nPrefix;` |
|        - |  7349 | `	int entropy;` |
|        - |  7350 | `	/* Generate a random string first */` |
|       25 |  7351 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7352 | `	/* Initialize fields */` |
|       25 |  7353 | `	zPrefix = 0;` |
|       25 |  7354 | `	nPrefix = 0;` |
|       25 |  7355 | `	entropy = 0;` |
|       25 |  7356 | `	if( nArg > 0 ){` |
|        - |  7357 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7358 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7359 | `		if( nArg > 1 ){` |
|      ! 0 |  7360 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7361 | `		}` |
|      ! 0 |  7362 | `	}` |
|       25 |  7363 | `	SHA1Init(&sCtx);` |
|        - |  7364 | `	/* Generate the random ID */` |
|       25 |  7365 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7366 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7367 | `	}` |
|        - |  7368 | `	/* Append the random ID */` |
|       25 |  7369 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7370 | `	/* Append the random string */` |
|       25 |  7371 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7372 | `	/* Increment the number */` |
|       25 |  7373 | `	pVm->unique_id++;` |
|       25 |  7374 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7375 | `	/* Hexify the digest */` |
|       25 |  7376 | `	sUniq.pCtx = pCtx;` |
|       25 |  7377 | `	sUniq.entropy = entropy;` |
|       25 |  7378 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7379 | `	/* All done */` |
|       25 |  7380 | `	return PH7_OK;` |
|        1 |  7381 |  |
|        - |  7382 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7383 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7384 | `/*` |
|        - |  7385 | ` * Section:` |
|        - |  7386 | ` *  Language construct implementation as foreign functions.` |
|        - |  7387 | ` * Status:` |
|        - |  7388 | ` *    Stable.` |
|        - |  7389 | ` */` |
|        - |  7390 | `/*` |
|        - |  7391 | ` * void echo($string...)` |
|        - |  7392 | ` *  Output one or more messages.` |
|        - |  7393 | ` * Parameters` |
|        - |  7394 | ` *  $string` |
|        - |  7395 | ` *   Message to output.` |
|        - |  7396 | ` * Return` |
|        - |  7397 | ` *  NULL.` |
|        - |  7398 | ` */` |
|      ! 0 |  7399 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7400 |  |
|        - |  7401 | `	const char *zData;` |
|      ! 0 |  7402 | `	int nDataLen = 0;` |
|        - |  7403 | `	ph7_vm *pVm;` |
|        - |  7404 | `	int i,rc;` |
|        - |  7405 | `	/* Point to the target VM */` |
|      ! 0 |  7406 | `	pVm = pCtx->pVm;` |
|        - |  7407 | `	/* Output */` |
|      ! 0 |  7408 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7409 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7410 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7411 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7412 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7413 | `				/* Increment output length */` |
|      ! 0 |  7414 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7415 | `			}` |
|      ! 0 |  7416 | `			if( rc == SXERR_ABORT ){` |
|        - |  7417 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7418 | `				return PH7_ABORT;` |
|        - |  7419 | `			}` |
|      ! 0 |  7420 | `		}` |
|      ! 0 |  7421 | `	}` |
|      ! 0 |  7422 | `	return SXRET_OK;` |
|      ! 0 |  7423 |  |
|        - |  7424 | `/*` |
|        - |  7425 | ` * int print($string...)` |
|        - |  7426 | ` *  Output one or more messages.` |
|        - |  7427 | ` * Parameters` |
|        - |  7428 | ` *  $string` |
|        - |  7429 | ` *   Message to output.` |
|        - |  7430 | ` * Return` |
|        - |  7431 | ` *  1 always.` |
|        - |  7432 | ` */` |
|        2 |  7433 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7434 |  |
|        - |  7435 | `	const char *zData;` |
|        3 |  7436 | `	int nDataLen = 0;` |
|        - |  7437 | `	ph7_vm *pVm;` |
|        - |  7438 | `	int i,rc;` |
|        - |  7439 | `	/* Point to the target VM */` |
|        3 |  7440 | `	pVm = pCtx->pVm;` |
|        - |  7441 | `	/* Output */` |
|        5 |  7442 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7443 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7444 | `		if( nDataLen > 0 ){` |
|        3 |  7445 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7446 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7447 | `				/* Increment output length */` |
|        3 |  7448 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7449 | `			}` |
|        3 |  7450 | `			if( rc == SXERR_ABORT ){` |
|        - |  7451 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7452 | `				return PH7_ABORT;` |
|        - |  7453 | `			}` |
|        1 |  7454 | `		}` |
|        2 |  7455 | `	}` |
|        - |  7456 | `	/* Return 1 */` |
|        3 |  7457 | `	ph7_result_int(pCtx,1);` |
|        3 |  7458 | `	return SXRET_OK;` |
|        2 |  7459 |  |
|        - |  7460 | `/*` |
|        - |  7461 | ` * void exit(string $msg)` |
|        - |  7462 | ` * void exit(int $status)` |
|        - |  7463 | ` * void die(string $ms)` |
|        - |  7464 | ` * void die(int $status)` |
|        - |  7465 | ` *   Output a message and terminate program execution.` |
|        - |  7466 | ` * Parameter` |
|        - |  7467 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7468 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7469 | ` *  and not printed` |
|        - |  7470 | ` * Return` |
|        - |  7471 | ` *  NULL` |
|        - |  7472 | ` */` |
|      ! 0 |  7473 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7474 |  |
|      ! 0 |  7475 | `	if( nArg > 0 ){` |
|      ! 0 |  7476 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7477 | `			const char *zData;` |
|      ! 0 |  7478 | `			int iLen = 0;` |
|        - |  7479 | `			/* Print exit message */` |
|      ! 0 |  7480 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7481 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7482 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7483 | `			sxi32 iExitStatus;` |
|        - |  7484 | `			/* Record exit status code */` |
|      ! 0 |  7485 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7486 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7487 | `		}` |
|      ! 0 |  7488 | `	}` |
|        - |  7489 | `	/* Check if we are in an included file */` |
|      ! 0 |  7490 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7491 | `		/* Exit the entire process */` |
|      ! 0 |  7492 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7493 | `	}` |
|        - |  7494 | `	/* Abort processing immediately */` |
|      ! 0 |  7495 | `	return PH7_ABORT;` |
|      ! 0 |  7496 |  |
|        - |  7497 | `/*` |
|        - |  7498 | ` * bool isset($var,...)` |
|        - |  7499 | ` *  Finds out whether a variable is set.` |
|        - |  7500 | ` * Parameters` |
|        - |  7501 | ` *  One or more variable to check.` |
|        - |  7502 | ` * Return` |
|        - |  7503 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7504 | ` */` |
|    67994 |  7505 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7506 |  |
|        - |  7507 | `	ph7_value *pObj;` |
|    67996 |  7508 | `	int res = 0;` |
|        - |  7509 | `	int i;` |
|    67996 |  7510 | `	if( nArg < 1 ){` |
|        - |  7511 | `		/* Missing arguments,return false */` |
|      ! 0 |  7512 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7513 | `		return SXRET_OK;` |
|        - |  7514 | `	}` |
|        - |  7515 | `	/* Iterate over available arguments */` |
|    90036 |  7516 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    67996 |  7517 | `		pObj = apArg[i];` |
|    67996 |  7518 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    45492 |  7519 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7520 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7521 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7522 | `			}` |
|    22745 |  7523 | `		}` |
|    67996 |  7524 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    67996 |  7525 | `		if( !res ){` |
|        - |  7526 | `			/* Variable not set,return FALSE */` |
|    45956 |  7527 | `			ph7_result_bool(pCtx,0);` |
|    45956 |  7528 | `			return SXRET_OK;` |
|        - |  7529 | `		}` |
|    11022 |  7530 | `	}` |
|        - |  7531 | `	/* All given variable are set,return TRUE */` |
|    22042 |  7532 | `	ph7_result_bool(pCtx,1);` |
|    22042 |  7533 | `	return SXRET_OK;` |
|    33999 |  7534 |  |
|        - |  7535 | `/*` |
|        - |  7536 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7537 | ` * frame,the reference table and discard it's contents.` |
|        - |  7538 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7539 | ` */` |
|  2943532 |  7540 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7541 |  |
|        - |  7542 | `	ph7_value *pObj;` |
|        - |  7543 | `	VmRefObj *pRef;` |
|  2943534 |  7544 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2943534 |  7545 | `	if( pObj ){` |
|        - |  7546 | `		/* Release the object */` |
|  2943534 |  7547 | `		PH7_MemObjRelease(pObj);` |
|  1471766 |  7548 | `	}` |
|        - |  7549 | `	/* Remove old reference links */` |
|  2943534 |  7550 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2943534 |  7551 | `	if( pRef ){` |
|  2943514 |  7552 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7553 | `		/* Unlink from the reference table */` |
|  2943514 |  7554 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2943514 |  7555 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7556 | `			VmSlot sFree;` |
|        - |  7557 | `			/* Restore to the free list */` |
|  2943508 |  7558 | `			sFree.nIdx = nObjIdx;` |
|  2943508 |  7559 | `			sFree.pUserData = 0;` |
|  2943508 |  7560 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1471753 |  7561 | `		}` |
|  1471756 |  7562 | `	}` |
|  2943534 |  7563 | `	return SXRET_OK;` |
|        2 |  7564 |  |
|        - |  7565 | `/*` |
|        - |  7566 | ` * void unset($var,...)` |
|        - |  7567 | ` *   Unset one or more given variable.` |
|        - |  7568 | ` * Parameters` |
|        - |  7569 | ` *  One or more variable to unset.` |
|        - |  7570 | ` * Return` |
|        - |  7571 | ` *  Nothing.` |
|        - |  7572 | ` */` |
|     3254 |  7573 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7574 |  |
|        - |  7575 | `	ph7_value *pObj;` |
|        - |  7576 | `	ph7_vm *pVm;` |
|        - |  7577 | `	int i;` |
|        - |  7578 | `	/* Point to the target VM */` |
|     3256 |  7579 | `	pVm = pCtx->pVm;` |
|        - |  7580 | `	/* Iterate and unset */` |
|     9656 |  7581 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6402 |  7582 | `		pObj = apArg[i];` |
|     6402 |  7583 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      866 |  7584 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7585 | `				/* Throw an error */` |
|      ! 0 |  7586 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7587 | `			}` |
|      434 |  7588 | `		}else{` |
|     5537 |  7589 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7590 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7591 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7592 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7593 | `			}` |
|        - |  7594 | `		}` |
|     3202 |  7595 | `	}` |
|     3256 |  7596 | `	return SXRET_OK;` |
|        2 |  7597 |  |
|        - |  7598 | `/*` |
|        - |  7599 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7600 | ` */` |
|      110 |  7601 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7602 |  |
|      111 |  7603 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7604 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7605 | `	ph7_value *pObj;` |
|        - |  7606 | `	sxu32 nIdx;` |
|        - |  7607 | `	/* Extract the memory object */` |
|      111 |  7608 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7609 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7610 | `	if( pObj ){` |
|      111 |  7611 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7612 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7613 | `				SyString sName;` |
|        - |  7614 | `				ph7_value sKey;` |
|        - |  7615 | `				/* Perform the insertion */` |
|      109 |  7616 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7617 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7618 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7619 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7620 | `			}` |
|       54 |  7621 | `		}` |
|       55 |  7622 | `	}` |
|      111 |  7623 | `	return SXRET_OK;` |
|        1 |  7624 |  |
|        - |  7625 | `/*` |
|        - |  7626 | ` * array get_defined_vars(void)` |
|        - |  7627 | ` *  Returns an array of all defined variables.` |
|        - |  7628 | ` * Parameter` |
|        - |  7629 | ` *  None` |
|        - |  7630 | ` * Return` |
|        - |  7631 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7632 | ` */` |
|        2 |  7633 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7634 |  |
|        3 |  7635 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7636 | `	ph7_value *pArray;` |
|        - |  7637 | `	/* Create a new array */` |
|        3 |  7638 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7639 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7640 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7641 | `		SXUNUSED(apArg);` |
|        - |  7642 | `		/* Return NULL */` |
|      ! 0 |  7643 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7644 | `		return SXRET_OK;` |
|        - |  7645 | `	}` |
|        - |  7646 | `	/* Superglobals first */` |
|        3 |  7647 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7648 | `	/* Then variable defined in the current frame */` |
|        3 |  7649 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7650 | `	/* Finally,return the created array */` |
|        3 |  7651 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7652 | `	return SXRET_OK;` |
|        2 |  7653 |  |
|        - |  7654 | `/*` |
|        - |  7655 | ` * bool gettype($var)` |
|        - |  7656 | ` *  Get the type of a variable` |
|        - |  7657 | ` * Parameters` |
|        - |  7658 | ` *   $var` |
|        - |  7659 | ` *    The variable being type checked.` |
|        - |  7660 | ` * Return` |
|        - |  7661 | ` *   String representation of the given variable type.` |
|        - |  7662 | ` */` |
|       32 |  7663 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7664 |  |
|       34 |  7665 | `	const char *zType = "Empty";` |
|       34 |  7666 | `	if( nArg > 0 ){` |
|       34 |  7667 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7668 | `	}` |
|        - |  7669 | `	/* Return the variable type */` |
|       34 |  7670 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7671 | `	return SXRET_OK;` |
|        2 |  7672 |  |
|        - |  7673 | `/*` |
|        - |  7674 | ` * string get_resource_type(resource $handle)` |
|        - |  7675 | ` *  This function gets the type of the given resource.` |
|        - |  7676 | ` * Parameters` |
|        - |  7677 | ` *  $handle` |
|        - |  7678 | ` *  The evaluated resource handle.` |
|        - |  7679 | ` * Return` |
|        - |  7680 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7681 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7682 | ` *  the return value will be the string Unknown.` |
|        - |  7683 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7684 | ` *  is not a resource.` |
|        - |  7685 | ` */` |
|        2 |  7686 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7687 |  |
|        3 |  7688 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7689 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7690 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7691 | `		return PH7_OK;` |
|        - |  7692 | `	}` |
|        3 |  7693 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7694 | `	return SXRET_OK;` |
|        2 |  7695 |  |
|        - |  7696 | `/*` |
|        - |  7697 | ` * void var_dump(expression,....)` |
|        - |  7698 | ` *   var_dump � Dumps information about a variable` |
|        - |  7699 | ` * Parameters` |
|        - |  7700 | ` *   One or more expression to dump.` |
|        - |  7701 | ` * Returns` |
|        - |  7702 | ` *  Nothing.` |
|        - |  7703 | ` */` |
|      218 |  7704 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7705 |  |
|        - |  7706 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7707 | `	int i;` |
|      220 |  7708 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7709 | `	/* Dump one or more expressions */` |
|      444 |  7710 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7711 | `		ph7_value *pObj = apArg[i];` |
|        - |  7712 | `		/* Reset the working buffer */` |
|      226 |  7713 | `		SyBlobReset(&sDump);` |
|        - |  7714 | `		/* Dump the given expression */` |
|      226 |  7715 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7716 | `		/* Output */` |
|      226 |  7717 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7718 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7719 | `		}` |
|      114 |  7720 | `	}` |
|        - |  7721 | `	/* Release the working buffer */` |
|      220 |  7722 | `	SyBlobRelease(&sDump);` |
|      220 |  7723 | `	return SXRET_OK;` |
|        2 |  7724 |  |
|        - |  7725 | `/*` |
|        - |  7726 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7727 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7728 | ` * Parameters` |
|        - |  7729 | ` *   expression: Expression to dump` |
|        - |  7730 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7731 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7732 | ` *            print_r() will return the information rather than print it.` |
|        - |  7733 | ` * Return` |
|        - |  7734 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7735 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7736 | ` */` |
|       16 |  7737 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7738 |  |
|       17 |  7739 | `	int ret_string = 0;` |
|        - |  7740 | `	SyBlob sDump;` |
|       17 |  7741 | `	if( nArg < 1 ){` |
|        - |  7742 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7743 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7744 | `		return SXRET_OK;` |
|        - |  7745 | `	}` |
|       17 |  7746 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7747 | `	if ( nArg > 1 ){` |
|        - |  7748 | `		/* Where to redirect output */` |
|       11 |  7749 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7750 | `	}` |
|        - |  7751 | `	/* Generate dump */` |
|       17 |  7752 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7753 | `	if( !ret_string ){` |
|        - |  7754 | `		/* Output dump */` |
|        7 |  7755 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7756 | `		/* Return true */` |
|        7 |  7757 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7758 | `	}else{` |
|        - |  7759 | `		/* Generated dump as return value */` |
|       11 |  7760 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7761 | `	}` |
|        - |  7762 | `	/* Release the working buffer */` |
|       17 |  7763 | `	SyBlobRelease(&sDump);` |
|       17 |  7764 | `	return SXRET_OK;` |
|        9 |  7765 |  |
|        - |  7766 | `/*` |
|        - |  7767 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7768 | ` * Same job as print_r. (see coment above)` |
|        - |  7769 | ` */` |
|        2 |  7770 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7771 |  |
|        3 |  7772 | `	int ret_string = 0;` |
|        - |  7773 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7774 | `	if( nArg < 1 ){` |
|        - |  7775 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7776 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7777 | `		return SXRET_OK;` |
|        - |  7778 | `	}` |
|        3 |  7779 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7780 | `	if ( nArg > 1 ){` |
|        - |  7781 | `		/* Where to redirect output */` |
|        3 |  7782 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7783 | `	}` |
|        - |  7784 | `	/* Generate dump */` |
|        3 |  7785 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7786 | `	if( !ret_string ){` |
|        - |  7787 | `		/* Output dump */` |
|      ! 0 |  7788 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7789 | `		/* Return NULL */` |
|      ! 0 |  7790 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7791 | `	}else{` |
|        - |  7792 | `		/* Generated dump as return value */` |
|        3 |  7793 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7794 | `	}` |
|        - |  7795 | `	/* Release the working buffer */` |
|        3 |  7796 | `	SyBlobRelease(&sDump);` |
|        3 |  7797 | `	return SXRET_OK;` |
|        2 |  7798 |  |
|        - |  7799 | `/*` |
|        - |  7800 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7801 | ` *  Set/get the various assert flags.` |
|        - |  7802 | ` * Parameter` |
|        - |  7803 | ` * $what` |
|        - |  7804 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7805 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  7806 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7807 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  7808 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7809 | ` * $value` |
|        - |  7810 | ` *   An optional new value for the option.` |
|        - |  7811 | ` * Return` |
|        - |  7812 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7813 | ` */` |
|        8 |  7814 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7815 |  |
|        9 |  7816 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7817 | `	int iOld,iNew,iValue;` |
|        9 |  7818 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  7819 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7820 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7821 | `		return PH7_OK;` |
|        - |  7822 | `	}` |
|        - |  7823 | `	/* Save old assertion flags */` |
|        9 |  7824 | `	iOld = pVm->iAssertFlags;` |
|        - |  7825 | `	/* Extract the new flags */` |
|        9 |  7826 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  7827 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  7828 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  7829 | `		if( nArg > 1 ){` |
|        5 |  7830 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  7831 | `			if( iValue ){` |
|        - |  7832 | `				/* Disable assertion */` |
|        3 |  7833 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  7834 | `			}` |
|        3 |  7835 | `		}` |
|        6 |  7836 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|        - |  7837 | `		/* Deprecated in PHP 8: silently accept but ignore.` |
|        - |  7838 | `		 * Assertion failure now always throws AssertionError. */` |
|        3 |  7839 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  7840 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  7841 | `		if( nArg > 1 ){` |
|        3 |  7842 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  7843 | `			if( iValue ){` |
|        - |  7844 | `				/* Terminate execution on failed assertions */` |
|        3 |  7845 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  7846 | `			}` |
|        2 |  7847 | `		}` |
|        1 |  7848 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  7849 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7850 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  7851 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  7852 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  7853 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7854 | `		}` |
|      ! 0 |  7855 | `	}` |
|        - |  7856 | `	/* Return the old flags */` |
|        9 |  7857 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  7858 | `	return PH7_OK;` |
|        5 |  7859 |  |
|        - |  7860 | `/*` |
|        - |  7861 | ` * bool assert(mixed $assertion)` |
|        - |  7862 | ` *  Checks if assertion is FALSE.` |
|        - |  7863 | ` * Parameter` |
|        - |  7864 | ` *  $assertion` |
|        - |  7865 | ` *    The assertion to test.` |
|        - |  7866 | ` * Return` |
|        - |  7867 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  7868 | ` */` |
|       30 |  7869 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7870 |  |
|       32 |  7871 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7872 | `	int iFlags,iResult;` |
|        - |  7873 | `	const char *zDesc;` |
|        - |  7874 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7875 | `	if( nArg < 1 ){` |
|        3 |  7876 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7877 | `			"ArgumentCountError",` |
|        - |  7878 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  7879 | `			);` |
|        - |  7880 | `	}` |
|       30 |  7881 | `	iFlags = pVm->iAssertFlags;` |
|       30 |  7882 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  7883 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  7884 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  7885 | `		return PH7_OK;` |
|        - |  7886 | `	}` |
|        - |  7887 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       30 |  7888 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       30 |  7889 | `	if( !iResult ){` |
|        - |  7890 | `		/* Assertion failed */` |
|        - |  7891 | `		/* Extract optional description */` |
|       15 |  7892 | `		zDesc = 0;` |
|       15 |  7893 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  7894 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  7895 | `		}` |
|       15 |  7896 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  7897 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  7898 | `			ph7_value sFile,sLine;` |
|        - |  7899 | `			ph7_value *apCbArg[3];` |
|        - |  7900 | `			SyString *pFile;` |
|        - |  7901 | `			/* Extract the processed script */` |
|      ! 0 |  7902 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  7903 | `			if( pFile == 0 ){` |
|      ! 0 |  7904 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  7905 | `			}` |
|        - |  7906 | `			/* Invoke the callback */` |
|      ! 0 |  7907 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  7908 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  7909 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  7910 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  7911 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  7912 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  7913 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  7914 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  7915 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  7916 | `		}` |
|       15 |  7917 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  7918 | `			/* Abort VM execution immediately */` |
|        3 |  7919 | `			return PH7_ABORT;` |
|        - |  7920 | `		}` |
|        - |  7921 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  7922 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  7923 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7924 | `				"AssertionError",` |
|        - |  7925 | `				"%s",` |
|        1 |  7926 | `				zDesc` |
|        - |  7927 | `				);` |
|      ! 0 |  7928 | `		}else{` |
|       11 |  7929 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7930 | `				"AssertionError",` |
|        - |  7931 | `				"assert(false)"` |
|        - |  7932 | `				);` |
|        - |  7933 | `		}` |
|        - |  7934 | `	}` |
|        - |  7935 | `	/* Assertion passed */` |
|       16 |  7936 | `	ph7_result_bool(pCtx,1);` |
|       16 |  7937 | `	return PH7_OK;` |
|       17 |  7938 |  |
|        - |  7939 | `/*` |
|        - |  7940 | ` * Section:` |
|        - |  7941 | ` *  Error reporting functions.` |
|        - |  7942 | ` * Status:` |
|        - |  7943 | ` *    Stable.` |
|        - |  7944 | ` */` |
|        - |  7945 | `/*` |
|        - |  7946 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  7947 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  7948 | ` * Parameters` |
|        - |  7949 | ` *  $error_msg` |
|        - |  7950 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  7951 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  7952 | ` * $error_type` |
|        - |  7953 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  7954 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  7955 | ` * Return` |
|        - |  7956 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  7957 | ` */` |
|       12 |  7958 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7959 |  |
|       14 |  7960 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  7961 | `	int rc = PH7_OK;` |
|       14 |  7962 | `	if( nArg > 0 ){` |
|        - |  7963 | `		const char *zErr;` |
|        - |  7964 | `		int nLen;` |
|        - |  7965 | `		/* Extract the error message */` |
|       12 |  7966 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7967 | `		if( nArg > 1 ){` |
|        - |  7968 | `			/* Extract the error type */` |
|       12 |  7969 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  7970 | `			switch( nErr ){` |
|        1 |  7971 | `			case 1:   /* E_ERROR */` |
|        - |  7972 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  7973 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  7974 | `			case 256: /* E_USER_ERROR */` |
|        3 |  7975 | `				nErr = PH7_CTX_ERR;` |
|        3 |  7976 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  7977 | `				break;` |
|        1 |  7978 | `			case 2:   /* E_WARNING */` |
|        - |  7979 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  7980 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  7981 | `			case 512: /* E_USER_WARNING */` |
|        3 |  7982 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  7983 | `				break;` |
|        3 |  7984 | `			default:` |
|        8 |  7985 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  7986 | `				break;` |
|        - |  7987 | `			}` |
|        5 |  7988 | `		}` |
|        - |  7989 | `		/* Report error */` |
|       12 |  7990 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  7991 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  7992 | `			return rc;` |
|        - |  7993 | `		}` |
|        - |  7994 | `		/* Return true */` |
|       12 |  7995 | `		ph7_result_bool(pCtx,1);` |
|        7 |  7996 | `	}else{` |
|        - |  7997 | `		/* Missing arguments,return FALSE */` |
|        3 |  7998 | `		ph7_result_bool(pCtx,0);` |
|        - |  7999 | `	}` |
|       14 |  8000 | `	return rc;` |
|        8 |  8001 |  |
|        - |  8002 | `/*` |
|        - |  8003 | ` * int error_reporting([int $level])` |
|        - |  8004 | ` *  Sets which PHP errors are reported.` |
|        - |  8005 | ` * Parameters` |
|        - |  8006 | ` *  $level` |
|        - |  8007 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8008 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8009 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8010 | ` *   levels will not always behave as expected.` |
|        - |  8011 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8012 | ` *   in the predefined constants.` |
|        - |  8013 | ` * Return` |
|        - |  8014 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8015 | ` *   parameter is given.` |
|        - |  8016 | ` */` |
|       18 |  8017 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8018 |  |
|       19 |  8019 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8020 | `	int nOld;` |
|        - |  8021 | `	/* Extract the old reporting level */` |
|       19 |  8022 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  8023 | `	if( nArg > 0 ){` |
|        - |  8024 | `		int nNew;` |
|        - |  8025 | `		/* Extract the desired error reporting level */` |
|       11 |  8026 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  8027 | `		if( !nNew ){` |
|        - |  8028 | `			/* Do not report errors at all */` |
|        5 |  8029 | `			pVm->bErrReport = 0;` |
|        3 |  8030 | `		}else{` |
|        - |  8031 | `			/* Report all errors */` |
|        7 |  8032 | `			pVm->bErrReport = 1;` |
|        - |  8033 | `		}` |
|        5 |  8034 | `	}` |
|        - |  8035 | `	/* Return the old level */` |
|       19 |  8036 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  8037 | `	return PH7_OK;` |
|        1 |  8038 |  |
|        - |  8039 | `/*` |
|        - |  8040 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8041 | ` *  Send an error message somewhere.` |
|        - |  8042 | ` * Parameter` |
|        - |  8043 | ` *  $message` |
|        - |  8044 | ` *   The error message that should be logged.` |
|        - |  8045 | ` *  $message_type` |
|        - |  8046 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8047 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8048 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8049 | ` *       This is the default option.` |
|        - |  8050 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8051 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8052 | ` *    2  No longer an option.` |
|        - |  8053 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8054 | ` *       to the end of the message string.` |
|        - |  8055 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8056 | ` *  $destination` |
|        - |  8057 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8058 | ` *  $extra_headers` |
|        - |  8059 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8060 | ` * Return` |
|        - |  8061 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8062 | ` * NOTE:` |
|        - |  8063 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8064 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8065 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8066 | ` *  Otherwise this function is no-op.` |
|        - |  8067 | ` */` |
|        4 |  8068 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8069 |  |
|        - |  8070 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8071 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8072 | `	int iType = 0;` |
|        5 |  8073 | `	if( nArg < 1 ){` |
|        - |  8074 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8075 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8076 | `		return PH7_OK;` |
|        - |  8077 | `	}` |
|        5 |  8078 | `	if( pVm->xErrLog  ){` |
|        - |  8079 | `		/* Invoke the user callback */` |
|      ! 0 |  8080 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8081 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8082 | `		if( nArg > 1 ){` |
|      ! 0 |  8083 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8084 | `			if( nArg > 2 ){` |
|      ! 0 |  8085 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8086 | `				if( nArg > 3 ){` |
|      ! 0 |  8087 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8088 | `				}` |
|      ! 0 |  8089 | `			}` |
|      ! 0 |  8090 | `		}` |
|      ! 0 |  8091 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8092 | `	}` |
|        - |  8093 | `	/* Retun TRUE */` |
|        5 |  8094 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8095 | `	return PH7_OK;` |
|        3 |  8096 |  |
|        - |  8097 | `/*` |
|        - |  8098 | ` * bool restore_exception_handler(void)` |
|        - |  8099 | ` *  Restores the previously defined exception handler function.` |
|        - |  8100 | ` * Parameter` |
|        - |  8101 | ` *  None` |
|        - |  8102 | ` * Return` |
|        - |  8103 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8104 | ` */` |
|        4 |  8105 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8106 |  |
|        5 |  8107 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8108 | `	ph7_value *pOld,*pNew;` |
|        - |  8109 | `	/* Point to the old and the new handler */` |
|        5 |  8110 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8111 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8112 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8113 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8114 | `		SXUNUSED(apArg);` |
|        - |  8115 | `		/* No installed handler,return FALSE */` |
|        5 |  8116 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8117 | `		return PH7_OK;` |
|        - |  8118 | `	}` |
|        - |  8119 | `	/* Copy the old handler */` |
|      ! 0 |  8120 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8121 | `	PH7_MemObjRelease(pOld);` |
|        - |  8122 | `	/* Return TRUE */` |
|      ! 0 |  8123 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8124 | `	return PH7_OK;` |
|        3 |  8125 |  |
|        - |  8126 | `/*` |
|        - |  8127 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8128 | ` *  Sets a user-defined exception handler function.` |
|        - |  8129 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8130 | ` * NOTE` |
|        - |  8131 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8132 | ` *  the satndard PHP engine.` |
|        - |  8133 | ` * Parameters` |
|        - |  8134 | ` *  $exception_handler` |
|        - |  8135 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8136 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8137 | ` *   that was thrown.` |
|        - |  8138 | ` *  Note:` |
|        - |  8139 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8140 | ` * Return` |
|        - |  8141 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8142 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8143 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8144 | ` */` |
|        4 |  8145 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8146 |  |
|        6 |  8147 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8148 | `	ph7_value *pOld,*pNew;` |
|        - |  8149 | `	/* Point to the old and the new handler */` |
|        6 |  8150 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8151 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8152 | `	/* Return the old handler */` |
|        6 |  8153 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8154 | `	if( nArg > 0 ){` |
|        6 |  8155 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8156 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8157 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8158 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8159 | `		}else{` |
|        6 |  8160 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8161 | `			/* Install the new handler */` |
|        6 |  8162 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8163 | `		}` |
|        2 |  8164 | `	}` |
|        6 |  8165 | `	return PH7_OK;` |
|        2 |  8166 |  |
|        - |  8167 | `/*` |
|        - |  8168 | ` * bool restore_error_handler(void)` |
|        - |  8169 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8170 | ` * Parameters:` |
|        - |  8171 | ` *  None.` |
|        - |  8172 | ` * Return` |
|        - |  8173 | ` *  Always TRUE.` |
|        - |  8174 | ` */` |
|        4 |  8175 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8176 |  |
|        5 |  8177 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8178 | `	ph7_value *pOld,*pNew;` |
|        - |  8179 | `	/* Point to the old and the new handler */` |
|        5 |  8180 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8181 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8182 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8183 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8184 | `		SXUNUSED(apArg);` |
|        - |  8185 | `		/* No installed callback,return FALSE */` |
|        5 |  8186 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8187 | `		return PH7_OK;` |
|        - |  8188 | `	}` |
|        - |  8189 | `	/* Copy the old callback */` |
|      ! 0 |  8190 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8191 | `	PH7_MemObjRelease(pOld);` |
|        - |  8192 | `	/* Return TRUE */` |
|      ! 0 |  8193 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8194 | `	return PH7_OK;` |
|        3 |  8195 |  |
|        - |  8196 | `/*` |
|        - |  8197 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8198 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8199 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8200 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8201 | ` *  Sets a user-defined error handler function.` |
|        - |  8202 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8203 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8204 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8205 | ` *  conditions (using trigger_error()).` |
|        - |  8206 | ` * Parameters` |
|        - |  8207 | ` *  $error_handler` |
|        - |  8208 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8209 | ` *   describing the error.` |
|        - |  8210 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8211 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8212 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8213 | ` *   The function can be shown as:` |
|        - |  8214 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8215 | ` *     errno` |
|        - |  8216 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8217 | ` *   errstr` |
|        - |  8218 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8219 | ` *   errfile` |
|        - |  8220 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8221 | ` *     was raised in, as a string.` |
|        - |  8222 | ` *  Note:` |
|        - |  8223 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8224 | ` * Return` |
|        - |  8225 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8226 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8227 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8228 | ` */` |
|     8722 |  8229 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8230 |  |
|     8724 |  8231 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8232 | `	ph7_value *pOld,*pNew;` |
|        - |  8233 | `	/* Point to the old and the new handler */` |
|     8724 |  8234 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8235 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8236 | `	/* Return the old handler */` |
|     8724 |  8237 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8238 | `	if( nArg > 0 ){` |
|     8724 |  8239 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8240 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8241 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8242 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8243 | `		}else{` |
|     4364 |  8244 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8245 | `			/* Install the new handler */` |
|     4364 |  8246 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8247 | `		}` |
|     4361 |  8248 | `	}` |
|     8724 |  8249 | `	return PH7_OK;` |
|        2 |  8250 |  |
|        - |  8251 | `/*` |
|        - |  8252 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8253 | ` *  Generates a backtrace.` |
|        - |  8254 | ` * Paramaeter` |
|        - |  8255 | ` *  $options` |
|        - |  8256 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8257 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8258 | ` *   all the function/method arguments, to save memory.` |
|        - |  8259 | ` * $limit` |
|        - |  8260 | ` *   (Not Used)` |
|        - |  8261 | ` * Return` |
|        - |  8262 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8263 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8264 | ` *          Name        Type      Description` |
|        - |  8265 | ` *          ------      ------     -----------` |
|        - |  8266 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8267 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8268 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8269 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8270 | ` *          object      object    The current object.` |
|        - |  8271 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8272 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8273 | ` */` |
|      460 |  8274 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8275 |  |
|      462 |  8276 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8277 | `	ph7_value *pArray;` |
|        - |  8278 | `	ph7_class *pClass;` |
|        - |  8279 | `	ph7_value *pValue;` |
|        - |  8280 | `	SyString *pFile;` |
|        - |  8281 | `	/* Create a new array */` |
|      462 |  8282 | `	pArray = ph7_context_new_array(pCtx);` |
|      462 |  8283 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      462 |  8284 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8285 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8286 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8287 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8288 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8289 | `		SXUNUSED(apArg);` |
|      ! 0 |  8290 | `		return PH7_OK;` |
|        - |  8291 | `	}` |
|        - |  8292 | `	/* Dump running function name and it's arguments  */` |
|      462 |  8293 | `	if( pVm->pFrame->pParent ){` |
|      462 |  8294 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8295 | `		ph7_vm_func *pFunc;` |
|        - |  8296 | `		ph7_value *pArg;` |
|      462 |  8297 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8298 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8299 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8300 | `		}` |
|      462 |  8301 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      462 |  8302 | `		if( pFrame->pParent && pFunc ){` |
|      462 |  8303 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      462 |  8304 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      462 |  8305 | `			ph7_value_reset_string_cursor(pValue);` |
|      230 |  8306 | `		}` |
|        - |  8307 | `		/* Function arguments */` |
|      462 |  8308 | `		pArg = ph7_context_new_array(pCtx);` |
|      462 |  8309 | `		if( pArg  ){` |
|        - |  8310 | `			ph7_value *pObj;` |
|        - |  8311 | `			VmSlot *aSlot;` |
|        - |  8312 | `			sxu32 n;` |
|        - |  8313 | `			/* Start filling the array with the given arguments */` |
|      462 |  8314 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1834 |  8315 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1374 |  8316 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1374 |  8317 | `				if( pObj ){` |
|     1374 |  8318 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      686 |  8319 | `				}` |
|      688 |  8320 | `			}` |
|        - |  8321 | `			/* Save the array */` |
|      462 |  8322 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      230 |  8323 | `		}` |
|      230 |  8324 | `	}` |
|      462 |  8325 | `	ph7_value_int(pValue,1);` |
|        - |  8326 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8327 | `	 * line numbers at run-time. )` |
|        - |  8328 | `	 */` |
|      462 |  8329 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8330 | `	/* Current processed script */` |
|      462 |  8331 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      462 |  8332 | `	if( pFile ){` |
|      462 |  8333 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      462 |  8334 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      462 |  8335 | `		ph7_value_reset_string_cursor(pValue);` |
|      230 |  8336 | `	}` |
|        - |  8337 | `	/* Top class */` |
|      462 |  8338 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      462 |  8339 | `	if( pClass ){` |
|      458 |  8340 | `		ph7_value_reset_string_cursor(pValue);` |
|      458 |  8341 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      458 |  8342 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      228 |  8343 | `	}` |
|        - |  8344 | `	/* Return the freshly created array */` |
|      462 |  8345 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8346 | `	/*` |
|        - |  8347 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8348 | `	 * as soon we return from this function.` |
|        - |  8349 | `	 */` |
|      462 |  8350 | `	return PH7_OK;` |
|      232 |  8351 |  |
|        - |  8352 | `/*` |
|        - |  8353 | ` * Generate a small backtrace.` |
|        - |  8354 | ` * Store the generated dump in the given BLOB` |
|        - |  8355 | ` */` |
|        4 |  8356 | `static int VmMiniBacktrace(` |
|        - |  8357 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8358 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8359 | `	)` |
|        1 |  8360 |  |
|        5 |  8361 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8362 | `	ph7_vm_func *pFunc;` |
|        - |  8363 | `	ph7_class *pClass;` |
|        - |  8364 | `	SyString *pFile;` |
|        - |  8365 | `	/* Called function */` |
|        5 |  8366 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8367 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8368 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8369 | `	}` |
|        5 |  8370 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8371 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8372 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8373 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8374 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8375 | `	}else{` |
|      ! 0 |  8376 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8377 | `	}` |
|        5 |  8378 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8379 | `	/* Current processed script */` |
|        5 |  8380 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8381 | `	if( pFile ){` |
|        5 |  8382 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8383 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8384 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8385 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8386 | `	}` |
|        - |  8387 | `	/* Top class */` |
|        5 |  8388 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8389 | `	if( pClass ){` |
|      ! 0 |  8390 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8391 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8392 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8393 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8394 | `	}` |
|        5 |  8395 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8396 | `	/* All done */` |
|        5 |  8397 | `	return SXRET_OK;` |
|        1 |  8398 |  |
|        - |  8399 | `/*` |
|        - |  8400 | ` * void debug_print_backtrace()` |
|        - |  8401 | ` *  Prints a backtrace` |
|        - |  8402 | ` * Parameters` |
|        - |  8403 | ` * None` |
|        - |  8404 | ` * Return` |
|        - |  8405 | ` * NULL` |
|        - |  8406 | ` */` |
|        2 |  8407 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8408 |  |
|        3 |  8409 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8410 | `	SyBlob sDump;` |
|        3 |  8411 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8412 | `	/* Generate the backtrace */` |
|        3 |  8413 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8414 | `	/* Output backtrace */` |
|        3 |  8415 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8416 | `	/* All done,cleanup */` |
|        3 |  8417 | `	SyBlobRelease(&sDump);` |
|        1 |  8418 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8419 | `	SXUNUSED(apArg);` |
|        3 |  8420 | `	return PH7_OK;` |
|        1 |  8421 |  |
|        - |  8422 | `/*` |
|        - |  8423 | ` * string debug_string_backtrace()` |
|        - |  8424 | ` *  Generate a backtrace` |
|        - |  8425 | ` * Parameters` |
|        - |  8426 | ` * None` |
|        - |  8427 | ` * Return` |
|        - |  8428 | ` *  A mini backtrace().` |
|        - |  8429 | ` * Note that this is a symisc extension.` |
|        - |  8430 | ` */` |
|        2 |  8431 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8432 |  |
|        3 |  8433 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8434 | `	SyBlob sDump;` |
|        3 |  8435 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8436 | `	/* Generate the backtrace */` |
|        3 |  8437 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8438 | `	/* Return the backtrace */` |
|        3 |  8439 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8440 | `	/* All done,cleanup */` |
|        3 |  8441 | `	SyBlobRelease(&sDump);` |
|        1 |  8442 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8443 | `	SXUNUSED(apArg);` |
|        3 |  8444 | `	return PH7_OK;` |
|        1 |  8445 |  |
|        - |  8446 | `/*` |
|        - |  8447 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8448 | ` * exception is triggered.` |
|        - |  8449 | ` */` |
|      444 |  8450 | `static sxi32 VmUncaughtException(` |
|        - |  8451 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8452 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8453 | `	)` |
|        1 |  8454 |  |
|        - |  8455 | `	ph7_value *apArg[2],sArg;` |
|      445 |  8456 | `	int nArg = 1;` |
|        - |  8457 | `	sxi32 rc;` |
|      445 |  8458 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8459 | `		/* Nesting limit reached */` |
|      ! 0 |  8460 | `		return SXRET_OK;` |
|        - |  8461 | `	}` |
|        - |  8462 | `	/* Call any exception handler if available */` |
|      445 |  8463 | `	PH7_MemObjInit(pVm,&sArg);` |
|      445 |  8464 | `	if( pThis ){` |
|        - |  8465 | `		/* Load the exception instance */` |
|      445 |  8466 | `		sArg.x.pOther = pThis;` |
|      445 |  8467 | `		pThis->iRef++;` |
|      445 |  8468 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      223 |  8469 | `	}else{` |
|      ! 0 |  8470 | `		nArg = 0;` |
|        - |  8471 | `	}` |
|      445 |  8472 | `	apArg[0] = &sArg;` |
|        - |  8473 | `	/* Call the exception handler if available */` |
|      445 |  8474 | `	pVm->nExceptDepth++;` |
|      445 |  8475 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      445 |  8476 | `	pVm->nExceptDepth--;` |
|      445 |  8477 | `	if( rc != SXRET_OK ){` |
|        - |  8478 | `		SyBlob sMsgBuf;` |
|      443 |  8479 | `		const char *zClass = "Exception";` |
|      443 |  8480 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8481 | `		const char *zMsg;` |
|        - |  8482 | `		sxu32 nMsg;` |
|        - |  8483 | `		const char *zFuncName;` |
|        - |  8484 | `		int nFuncLen;` |
|      443 |  8485 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      443 |  8486 | `		if( pThis ){` |
|        - |  8487 | `			ph7_class_method *pGetMessage;` |
|        - |  8488 | `			ph7_value sMsg;` |
|        - |  8489 | `			const char *zTmp;` |
|        - |  8490 | `			int nTmp;` |
|      443 |  8491 | `			zClass = pThis->pClass->sName.zString;` |
|      443 |  8492 | `			nClass = pThis->pClass->sName.nByte;` |
|      443 |  8493 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      443 |  8494 | `			if( pGetMessage ){` |
|      443 |  8495 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      443 |  8496 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      443 |  8497 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      443 |  8498 | `					if( zTmp && nTmp > 0 ){` |
|      443 |  8499 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      221 |  8500 | `					}` |
|      221 |  8501 | `				}` |
|      443 |  8502 | `				PH7_MemObjRelease(&sMsg);` |
|      221 |  8503 | `			}` |
|      221 |  8504 | `		}` |
|      443 |  8505 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8506 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8507 | `		}` |
|      443 |  8508 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      443 |  8509 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      443 |  8510 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      443 |  8511 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      443 |  8512 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8513 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      443 |  8514 | `		rc = SXERR_ABORT;` |
|      221 |  8515 | `	}` |
|      445 |  8516 | `	PH7_MemObjRelease(&sArg);` |
|      445 |  8517 | `	return rc;` |
|      223 |  8518 |  |
|        - |  8519 | `/*` |
|        - |  8520 | ` * Throw an user exception.` |
|        - |  8521 | ` */` |
|      458 |  8522 | `static sxi32 VmThrowException(` |
|        - |  8523 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8524 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8525 | `	)` |
|        2 |  8526 |  |
|        - |  8527 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8528 | `	ph7_exception **apException;` |
|        - |  8529 | `	ph7_exception *pException;` |
|        - |  8530 | `	/* Point to the stack of loaded exceptions */` |
|      460 |  8531 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      460 |  8532 | `	pException = 0;` |
|      460 |  8533 | `	pCatch = 0;` |
|      460 |  8534 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8535 | `		ph7_exception_block *aCatch;` |
|        - |  8536 | `		ph7_class *pClass;` |
|        - |  8537 | `		sxu32 j;` |
|        - |  8538 | `		/* Locate the appropriate block to execute */` |
|       16 |  8539 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  8540 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  8541 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  8542 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  8543 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8544 | `			/* Extract the target class */` |
|       16 |  8545 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  8546 | `			if( pClass == 0 ){` |
|        - |  8547 | `				/* No such class */` |
|      ! 0 |  8548 | `				continue;` |
|        - |  8549 | `			}` |
|       16 |  8550 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8551 | `				/* Catch block found,break immeditaley */` |
|       16 |  8552 | `				pCatch = &aCatch[j];` |
|       16 |  8553 | `				break;` |
|        - |  8554 | `			}` |
|      ! 0 |  8555 | `		}` |
|        7 |  8556 | `	}` |
|        - |  8557 | `	/* Execute the cached block if available */` |
|      460 |  8558 | `	if( pCatch == 0 ){` |
|        - |  8559 | `		sxi32 rc;` |
|      445 |  8560 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      445 |  8561 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8562 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8563 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8564 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8565 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8566 | `			}` |
|      ! 0 |  8567 | `			if( pException->pFrame == pFrame ){` |
|        - |  8568 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8569 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8570 | `			}` |
|      ! 0 |  8571 | `		}` |
|      445 |  8572 | `		return rc;` |
|      ! 0 |  8573 | `	}else{` |
|       16 |  8574 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8575 | `		sxi32 rc;` |
|       24 |  8576 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8577 | `			/* Safely ignore the exception frame */` |
|       10 |  8578 | `			pFrame = pFrame->pParent;` |
|        2 |  8579 | `		}` |
|       16 |  8580 | `		if( pException->pFrame == pFrame ){` |
|        - |  8581 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  8582 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  8583 | `		}` |
|        - |  8584 | `		/* Create a private frame first */` |
|       16 |  8585 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  8586 | `		if( rc == SXRET_OK ){` |
|        - |  8587 | `			/* Mark as catch frame */` |
|       16 |  8588 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  8589 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  8590 | `			if( pObj ){` |
|        - |  8591 | `				/* Install the exception instance */` |
|       16 |  8592 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  8593 | `				pObj->x.pOther = pThis;` |
|       16 |  8594 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  8595 | `			}` |
|        - |  8596 | `			/* Exceute the block */` |
|       16 |  8597 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8598 | `			/* Leave the frame */` |
|       16 |  8599 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  8600 | `		}` |
|        - |  8601 | `	}` |
|        - |  8602 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8603 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8604 | `	 */` |
|       16 |  8605 | `	return SXRET_OK;` |
|      231 |  8606 |  |
|        - |  8607 | `/*` |
|        - |  8608 | ` * Section:` |
|        - |  8609 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8610 | ` * Status:` |
|        - |  8611 | ` *    Stable.` |
|        - |  8612 | ` */` |
|        - |  8613 | `/*` |
|        - |  8614 | ` * string ph7version(void)` |
|        - |  8615 | ` *  Returns the running version of the PH7 version.` |
|        - |  8616 | ` * Parameters` |
|        - |  8617 | ` *  None` |
|        - |  8618 | ` * Return` |
|        - |  8619 | ` * Current PH7 version.` |
|        - |  8620 | ` */` |
|        2 |  8621 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8622 |  |
|        1 |  8623 | `	SXUNUSED(nArg);` |
|        1 |  8624 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8625 | `	/* Current engine version */` |
|        3 |  8626 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8627 | `	return PH7_OK;` |
|        1 |  8628 |  |
|        - |  8629 | `/*` |
|        - |  8630 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8631 | ` */` |
|        - |  8632 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8633 | ` "<html><head>"\` |
|        - |  8634 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8635 | ` "<style type=\"text/css\">"\` |
|        - |  8636 | ` "div {"\` |
|        - |  8637 | `     "border: 1px solid #cccccc;"\` |
|        - |  8638 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8639 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8640 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8641 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8642 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8643 | `     "-o-border-radius: 10px;"\` |
|        - |  8644 | `     "border-radius: 10px;"\` |
|        - |  8645 | `     "padding-left: 2em;"\` |
|        - |  8646 | `     "background-color: white;"\` |
|        - |  8647 | `     "margin-left: auto;"\` |
|        - |  8648 | `     "font-family: verdana;"\` |
|        - |  8649 | `     "padding-right: 2em;"\` |
|        - |  8650 | `     "margin-right: auto;"\` |
|        - |  8651 | `     "}"\` |
|        - |  8652 | `     "body {"\` |
|        - |  8653 | `     "padding: 0.2em;"\` |
|        - |  8654 | `     "font-style: normal;"\` |
|        - |  8655 | `     "font-size: medium;"\` |
|        - |  8656 | `     "background-color: #f2f2f2;"\` |
|        - |  8657 | `     "}"\` |
|        - |  8658 | `     "hr {"\` |
|        - |  8659 | `     "border-style: solid none none;"\` |
|        - |  8660 | `     "border-width: 1px medium medium;"\` |
|        - |  8661 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8662 | `     "height: 1px;"\` |
|        - |  8663 | `     "}"\` |
|        - |  8664 | `     "a {"\` |
|        - |  8665 | `     "color: #3366cc;"\` |
|        - |  8666 | `     "text-decoration: none;"\` |
|        - |  8667 | `     "}"\` |
|        - |  8668 | `     "a:hover {"\` |
|        - |  8669 | `     "color: #999999;"\` |
|        - |  8670 | `     "}"\` |
|        - |  8671 | `     "a:active {"\` |
|        - |  8672 | `     "color: #663399;"\` |
|        - |  8673 | `     "}"\` |
|        - |  8674 | `     "h1 {"\` |
|        - |  8675 | `     "margin: 0;"\` |
|        - |  8676 | `     "padding: 0;"\` |
|        - |  8677 | `     "font-family: Verdana;"\` |
|        - |  8678 | `     "font-weight: bold;"\` |
|        - |  8679 | `     "font-style: normal;"\` |
|        - |  8680 | `     "font-size: medium;"\` |
|        - |  8681 | `     "text-transform: capitalize;"\` |
|        - |  8682 | `     "color: #0a328c;"\` |
|        - |  8683 | `     "}"\` |
|        - |  8684 | `     "p {"\` |
|        - |  8685 | `     "margin: 0 auto;"\` |
|        - |  8686 | `     "font-size: medium;"\` |
|        - |  8687 | `     "font-style: normal;"\` |
|        - |  8688 | `     "font-family: verdana;"\` |
|        - |  8689 | `     "}"\` |
|        - |  8690 | `"</style></head><body>"\` |
|        - |  8691 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8692 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8693 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8694 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8695 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8696 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8697 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8698 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8699 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8700 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8701 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8702 |  |
|        - |  8703 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8704 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8705 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8706 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8707 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8708 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8709 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8710 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8711 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8712 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8713 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8714 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8715 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8716 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8717 |  |
|        - |  8718 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8719 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8720 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8721 | `"&nbsp;*<br>"\` |
|        - |  8722 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8723 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8724 | `"&nbsp;* are met:<br>"\` |
|        - |  8725 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8726 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8727 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8728 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8729 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8730 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8731 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8732 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8733 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8734 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8735 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8736 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8737 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8738 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8739 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8740 | `"&nbsp;*<br>"\` |
|        - |  8741 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8742 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8743 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8744 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8745 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8746 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8747 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8748 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8749 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8750 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8751 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8752 | `"&nbsp;*/<br>"\` |
|        - |  8753 | `"</span></small></small></p>"\` |
|        - |  8754 | `"</div></body></html>"` |
|        - |  8755 | `/*` |
|        - |  8756 | ` * bool ph7credits(void)` |
|        - |  8757 | ` * bool ph7info(void)` |
|        - |  8758 | ` * bool ph7copyright(void)` |
|        - |  8759 | ` *  Prints out the credits for PH7 engine` |
|        - |  8760 | ` * Parameters` |
|        - |  8761 | ` *  None` |
|        - |  8762 | ` * Return` |
|        - |  8763 | ` *  Always TRUE` |
|        - |  8764 | ` */` |
|        2 |  8765 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8766 |  |
|        3 |  8767 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8768 | `	/* Expand the HTML page above*/` |
|        3 |  8769 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8770 | `	ph7_context_output_format(` |
|        1 |  8771 | `		pCtx,` |
|        - |  8772 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8773 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8774 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8775 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8776 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8777 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8778 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8779 | `#ifdef __WINNT__` |
|        - |  8780 | `		"Windows NT"` |
|        - |  8781 | `#elif defined(__UNIXES__)` |
|        - |  8782 | `		"UNIX-Like"` |
|        - |  8783 | `#else` |
|        - |  8784 | `		"Other OS"` |
|        - |  8785 | `#endif` |
|        - |  8786 | `		);` |
|        3 |  8787 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8788 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8789 | `	SXUNUSED(apArg);` |
|        - |  8790 | `	/* Return TRUE */` |
|        - |  8791 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8792 | `	return PH7_OK;` |
|        1 |  8793 |  |
|        - |  8794 | `/*` |
|        - |  8795 | ` * Section:` |
|        - |  8796 | ` *    URL related routines.` |
|        - |  8797 | ` * Status:` |
|        - |  8798 | ` *    Stable.` |
|        - |  8799 | ` */` |
|        - |  8800 | `/*` |
|        - |  8801 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8802 | ` *  Parse a URL and return its fields.` |
|        - |  8803 | ` * Parameters` |
|        - |  8804 | ` *  $url` |
|        - |  8805 | ` *   The URL to parse.` |
|        - |  8806 | ` * $component` |
|        - |  8807 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8808 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8809 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8810 | ` *  in which case the return value will be an integer).` |
|        - |  8811 | ` * Return` |
|        - |  8812 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8813 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8814 | ` *  this array are:` |
|        - |  8815 | ` *   scheme - e.g. http` |
|        - |  8816 | ` *   host` |
|        - |  8817 | ` *   port` |
|        - |  8818 | ` *   user` |
|        - |  8819 | ` *   pass` |
|        - |  8820 | ` *   path` |
|        - |  8821 | ` *   query - after the question mark ?` |
|        - |  8822 | ` *   fragment - after the hashmark #` |
|        - |  8823 | ` * Note:` |
|        - |  8824 | ` *  FALSE is returned on failure.` |
|        - |  8825 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  8826 | ` *  with the standard PHP engine.` |
|        - |  8827 | ` */` |
|       28 |  8828 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8829 |  |
|        - |  8830 | `	const char *zStr; /* Input string */` |
|        - |  8831 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  8832 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  8833 | `	int nLen;` |
|        - |  8834 | `	sxi32 rc;` |
|       29 |  8835 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8836 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8837 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8838 | `		return PH7_OK;` |
|        - |  8839 | `	}` |
|        - |  8840 | `	/* Extract the given URI */` |
|       29 |  8841 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  8842 | `	if( nLen < 1 ){` |
|        - |  8843 | `		/* Nothing to process,return FALSE */` |
|        3 |  8844 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8845 | `		return PH7_OK;` |
|        - |  8846 | `	}` |
|        - |  8847 | `	/* Get a parse */` |
|       27 |  8848 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  8849 | `	if( rc != SXRET_OK ){` |
|        - |  8850 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  8851 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8852 | `		return PH7_OK;` |
|        - |  8853 | `	}` |
|       27 |  8854 | `	if( nArg > 1 ){` |
|      ! 0 |  8855 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  8856 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  8857 | `		switch(nComponent){` |
|      ! 0 |  8858 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  8859 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  8860 | `			if( pComp->nByte < 1 ){` |
|        - |  8861 | `				/* No available value,return NULL */` |
|      ! 0 |  8862 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8863 | `			}else{` |
|      ! 0 |  8864 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8865 | `			}` |
|      ! 0 |  8866 | `			break;` |
|      ! 0 |  8867 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  8868 | `			pComp = &sURI.sHost;` |
|      ! 0 |  8869 | `			if( pComp->nByte < 1 ){` |
|        - |  8870 | `				/* No available value,return NULL */` |
|      ! 0 |  8871 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8872 | `			}else{` |
|      ! 0 |  8873 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8874 | `			}` |
|      ! 0 |  8875 | `			break;` |
|      ! 0 |  8876 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  8877 | `			pComp = &sURI.sPort;` |
|      ! 0 |  8878 | `			if( pComp->nByte < 1 ){` |
|        - |  8879 | `				/* No available value,return NULL */` |
|      ! 0 |  8880 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8881 | `			}else{` |
|      ! 0 |  8882 | `				int iPort = 0;` |
|        - |  8883 | `				/* Cast the value to integer */` |
|      ! 0 |  8884 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  8885 | `				ph7_result_int(pCtx,iPort);` |
|        - |  8886 | `			}` |
|      ! 0 |  8887 | `			break;` |
|      ! 0 |  8888 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  8889 | `			pComp = &sURI.sUser;` |
|      ! 0 |  8890 | `			if( pComp->nByte < 1 ){` |
|        - |  8891 | `				/* No available value,return NULL */` |
|      ! 0 |  8892 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8893 | `			}else{` |
|      ! 0 |  8894 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8895 | `			}` |
|      ! 0 |  8896 | `			break;` |
|      ! 0 |  8897 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  8898 | `			pComp = &sURI.sPass;` |
|      ! 0 |  8899 | `			if( pComp->nByte < 1 ){` |
|        - |  8900 | `				/* No available value,return NULL */` |
|      ! 0 |  8901 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8902 | `			}else{` |
|      ! 0 |  8903 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8904 | `			}` |
|      ! 0 |  8905 | `			break;` |
|      ! 0 |  8906 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  8907 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  8908 | `			if( pComp->nByte < 1 ){` |
|        - |  8909 | `				/* No available value,return NULL */` |
|      ! 0 |  8910 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8911 | `			}else{` |
|      ! 0 |  8912 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8913 | `			}` |
|      ! 0 |  8914 | `			break;` |
|      ! 0 |  8915 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  8916 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  8917 | `			if( pComp->nByte < 1 ){` |
|        - |  8918 | `				/* No available value,return NULL */` |
|      ! 0 |  8919 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8920 | `			}else{` |
|      ! 0 |  8921 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8922 | `			}` |
|      ! 0 |  8923 | `			break;` |
|      ! 0 |  8924 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  8925 | `			pComp = &sURI.sPath;` |
|      ! 0 |  8926 | `			if( pComp->nByte < 1 ){` |
|        - |  8927 | `				/* No available value,return NULL */` |
|      ! 0 |  8928 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8929 | `			}else{` |
|      ! 0 |  8930 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8931 | `			}` |
|      ! 0 |  8932 | `			break;` |
|      ! 0 |  8933 | `		default:` |
|        - |  8934 | `			/* No such entry,return NULL */` |
|      ! 0 |  8935 | `			ph7_result_null(pCtx);` |
|      ! 0 |  8936 | `			break;` |
|        - |  8937 | `		}` |
|      ! 0 |  8938 | `	}else{` |
|        - |  8939 | `		ph7_value *pArray,*pValue;` |
|        - |  8940 | `		/* Return an associative array */` |
|       27 |  8941 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  8942 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  8943 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8944 | `			/* Out of memory */` |
|      ! 0 |  8945 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  8946 | `			/* Return false */` |
|      ! 0 |  8947 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  8948 | `			return PH7_OK;` |
|        - |  8949 | `		}` |
|        - |  8950 | `		/* Fill the array */` |
|       27 |  8951 | `		pComp = &sURI.sScheme;` |
|       27 |  8952 | `		if( pComp->nByte > 0 ){` |
|       19 |  8953 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  8954 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  8955 | `		}` |
|        - |  8956 | `		/* Reset the string cursor */` |
|       27 |  8957 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8958 | `		pComp = &sURI.sHost;` |
|       27 |  8959 | `		if( pComp->nByte > 0 ){` |
|       25 |  8960 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  8961 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  8962 | `		}` |
|        - |  8963 | `		/* Reset the string cursor */` |
|       27 |  8964 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8965 | `		pComp = &sURI.sPort;` |
|       27 |  8966 | `		if( pComp->nByte > 0 ){` |
|       11 |  8967 | `			int iPort = 0;/* cc warning */` |
|        - |  8968 | `			/* Convert to integer */` |
|       11 |  8969 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  8970 | `			ph7_value_int(pValue,iPort);` |
|       11 |  8971 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  8972 | `		}` |
|        - |  8973 | `		/* Reset the string cursor */` |
|       27 |  8974 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8975 | `		pComp = &sURI.sUser;` |
|       27 |  8976 | `		if( pComp->nByte > 0 ){` |
|        7 |  8977 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  8978 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  8979 | `		}` |
|        - |  8980 | `		/* Reset the string cursor */` |
|       27 |  8981 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8982 | `		pComp = &sURI.sPass;` |
|       27 |  8983 | `		if( pComp->nByte > 0 ){` |
|        7 |  8984 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  8985 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  8986 | `		}` |
|        - |  8987 | `		/* Reset the string cursor */` |
|       27 |  8988 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8989 | `		pComp = &sURI.sPath;` |
|       27 |  8990 | `		if( pComp->nByte > 0 ){` |
|       17 |  8991 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  8992 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  8993 | `		}` |
|        - |  8994 | `		/* Reset the string cursor */` |
|       27 |  8995 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8996 | `		pComp = &sURI.sQuery;` |
|       27 |  8997 | `		if( pComp->nByte > 0 ){` |
|        5 |  8998 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  8999 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9000 | `		}` |
|        - |  9001 | `		/* Reset the string cursor */` |
|       27 |  9002 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9003 | `		pComp = &sURI.sFragment;` |
|       27 |  9004 | `		if( pComp->nByte > 0 ){` |
|        5 |  9005 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9006 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9007 | `		}` |
|        - |  9008 | `		/* Return the created array */` |
|       27 |  9009 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9010 | `		/* NOTE:` |
|        - |  9011 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9012 | `		 * automatically as soon we return from this function.` |
|        - |  9013 | `		 */` |
|        - |  9014 | `	}` |
|        - |  9015 | `	/* All done */` |
|       27 |  9016 | `	return PH7_OK;` |
|       15 |  9017 |  |
|        - |  9018 | `/*` |
|        - |  9019 | ` * Section:` |
|        - |  9020 | ` *   Array related routines.` |
|        - |  9021 | ` * Status:` |
|        - |  9022 | ` *    Stable.` |
|        - |  9023 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9024 | ` *  Array related functions that need access to the underlying` |
|        - |  9025 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9026 | ` */` |
|        - |  9027 | `/*` |
|        - |  9028 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9029 | ` * of the following structure.` |
|        - |  9030 | ` */` |
|        - |  9031 | `struct compact_data` |
|        - |  9032 |  |
|        - |  9033 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9034 | `	int nRecCount;      /* Recursion count */` |
|        - |  9035 | `};` |
|        - |  9036 | `/*` |
|        - |  9037 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9038 | ` */` |
|      ! 0 |  9039 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9040 |  |
|      ! 0 |  9041 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9042 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9043 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9044 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9045 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9046 | `		SyString sVar;` |
|      ! 0 |  9047 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9048 | `		if( sVar.nByte > 0 ){` |
|        - |  9049 | `			/* Query the current frame */` |
|      ! 0 |  9050 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9051 | `			/* ^` |
|        - |  9052 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9053 | `			 */` |
|      ! 0 |  9054 | `			if( pKey ){` |
|        - |  9055 | `				/* Perform the insertion */` |
|      ! 0 |  9056 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9057 | `			}` |
|      ! 0 |  9058 | `		}` |
|      ! 0 |  9059 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9060 | `		int rc;` |
|        - |  9061 | `		/* Recursively traverse this array */` |
|      ! 0 |  9062 | `		pData->nRecCount++;` |
|      ! 0 |  9063 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9064 | `		pData->nRecCount--;` |
|      ! 0 |  9065 | `		return rc;` |
|        - |  9066 | `	}` |
|      ! 0 |  9067 | `	return SXRET_OK;` |
|      ! 0 |  9068 |  |
|        - |  9069 | `/*` |
|        - |  9070 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9071 | ` *  Create array containing variables and their values.` |
|        - |  9072 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9073 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9074 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9075 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9076 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9077 | ` * Parameters` |
|        - |  9078 | ` *  $varname` |
|        - |  9079 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9080 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9081 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9082 | ` *   it recursively.` |
|        - |  9083 | ` * Return` |
|        - |  9084 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9085 | ` */` |
|        2 |  9086 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9087 |  |
|        - |  9088 | `	ph7_value *pArray,*pObj;` |
|        3 |  9089 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9090 | `	const char *zName;` |
|        - |  9091 | `	SyString sVar;` |
|        - |  9092 | `	int i,nLen;` |
|        3 |  9093 | `	if( nArg < 1 ){` |
|        - |  9094 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9095 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9096 | `		return PH7_OK;` |
|        - |  9097 | `	}` |
|        - |  9098 | `	/* Create the array */` |
|        3 |  9099 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9100 | `	if( pArray == 0 ){` |
|        - |  9101 | `		/* Out of memory */` |
|      ! 0 |  9102 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9103 | `		/* Return NULL */` |
|      ! 0 |  9104 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9105 | `		return PH7_OK;` |
|        - |  9106 | `	}` |
|        - |  9107 | `	/* Perform the requested operation */` |
|        7 |  9108 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9109 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9110 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9111 | `				struct compact_data sData;` |
|      ! 0 |  9112 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9113 | `				/* Recursively walk the array */` |
|      ! 0 |  9114 | `				sData.nRecCount = 0;` |
|      ! 0 |  9115 | `				sData.pArray = pArray;` |
|      ! 0 |  9116 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9117 | `			}` |
|      ! 0 |  9118 | `		}else{` |
|        - |  9119 | `			/* Extract variable name */` |
|        5 |  9120 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9121 | `			if( nLen > 0 ){` |
|        5 |  9122 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9123 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9124 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9125 | `				if( pObj ){` |
|        5 |  9126 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9127 | `				}` |
|        2 |  9128 | `			}` |
|        - |  9129 | `		}` |
|        3 |  9130 | `	}` |
|        - |  9131 | `	/* Return the array */` |
|        3 |  9132 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9133 | `	return PH7_OK;` |
|        2 |  9134 |  |
|        - |  9135 | `/*` |
|        - |  9136 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9137 | ` * of the following structure.` |
|        - |  9138 | ` */` |
|        - |  9139 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9140 | `struct extract_aux_data` |
|        - |  9141 |  |
|        - |  9142 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9143 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9144 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9145 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9146 | `	int iFlags;           /* Control flags */` |
|        - |  9147 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9148 | `};` |
|        - |  9149 | `/* Forward declaration */` |
|        - |  9150 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9151 | `/*` |
|        - |  9152 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9153 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9154 | ` * Parameters` |
|        - |  9155 | ` * $var_array` |
|        - |  9156 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9157 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9158 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9159 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9160 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9161 | ` * $extract_type` |
|        - |  9162 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9163 | ` *  It can be one of the following values:` |
|        - |  9164 | ` *   EXTR_OVERWRITE` |
|        - |  9165 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9166 | ` *   EXTR_SKIP` |
|        - |  9167 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9168 | ` *   EXTR_PREFIX_SAME` |
|        - |  9169 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9170 | ` *   EXTR_PREFIX_ALL` |
|        - |  9171 | ` *       Prefix all variable names with prefix.` |
|        - |  9172 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9173 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9174 | ` *   EXTR_IF_EXISTS` |
|        - |  9175 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9176 | ` *       otherwise do nothing.` |
|        - |  9177 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9178 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9179 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9180 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9181 | ` *      the current symbol table.` |
|        - |  9182 | ` * $prefix` |
|        - |  9183 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9184 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9185 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9186 | ` *  underscore character.` |
|        - |  9187 | ` * Return` |
|        - |  9188 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9189 | ` */` |
|        4 |  9190 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9191 |  |
|        - |  9192 | `	extract_aux_data sAux;` |
|        - |  9193 | `	ph7_hashmap *pMap;` |
|        5 |  9194 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9195 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9196 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9197 | `		return PH7_OK;` |
|        - |  9198 | `	}` |
|        - |  9199 | `	/* Point to the target hashmap */` |
|        5 |  9200 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9201 | `	if( pMap->nEntry < 1 ){` |
|        - |  9202 | `		/* Empty map,return  0 */` |
|      ! 0 |  9203 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9204 | `		return PH7_OK;` |
|        - |  9205 | `	}` |
|        - |  9206 | `	/* Prepare the aux data */` |
|        5 |  9207 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9208 | `	if( nArg > 1 ){` |
|        3 |  9209 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9210 | `		if( nArg > 2 ){` |
|      ! 0 |  9211 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9212 | `		}` |
|        1 |  9213 | `	}` |
|        5 |  9214 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9215 | `	/* Invoke the worker callback */` |
|        5 |  9216 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9217 | `	/* Number of variables successfully imported */` |
|        5 |  9218 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9219 | `	return PH7_OK;` |
|        3 |  9220 |  |
|        - |  9221 | `/*` |
|        - |  9222 | ` * Worker callback for the [extract()] function defined` |
|        - |  9223 | ` * below.` |
|        - |  9224 | ` */` |
|        8 |  9225 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9226 |  |
|        9 |  9227 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9228 | `	int iFlags = pAux->iFlags;` |
|        9 |  9229 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9230 | `	ph7_value *pObj;` |
|        - |  9231 | `	SyString sVar;` |
|        9 |  9232 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9233 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9234 | `	}` |
|        - |  9235 | `	/* Perform a string cast */` |
|        9 |  9236 | `	PH7_MemObjToString(pKey);` |
|        9 |  9237 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9238 | `		/* Unavailable variable name */` |
|      ! 0 |  9239 | `		return SXRET_OK;` |
|        - |  9240 | `	}` |
|        9 |  9241 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9242 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9243 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9244 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9245 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9246 | `			);` |
|      ! 0 |  9247 | `	}else{` |
|       13 |  9248 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9249 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9250 | `	}` |
|        9 |  9251 | `	sVar.zString = pAux->zWorker;` |
|        - |  9252 | `	/* Try to extract the variable */` |
|        9 |  9253 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9254 | `	if( pObj ){` |
|        - |  9255 | `		/* Collision */` |
|        5 |  9256 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9257 | `			return SXRET_OK;` |
|        - |  9258 | `		}` |
|        5 |  9259 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9260 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9261 | `				/* Already prefixed */` |
|      ! 0 |  9262 | `				return SXRET_OK;` |
|        - |  9263 | `			}` |
|      ! 0 |  9264 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9265 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9266 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9267 | `				);` |
|      ! 0 |  9268 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9269 | `		}` |
|        3 |  9270 | `	}else{` |
|        - |  9271 | `		/* Create the variable */` |
|        5 |  9272 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9273 | `	}` |
|        9 |  9274 | `	if( pObj ){` |
|        - |  9275 | `		/* Overwrite the old value */` |
|        9 |  9276 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9277 | `		/* Increment counter */` |
|        9 |  9278 | `		pAux->iCount++;` |
|        4 |  9279 | `	}` |
|        9 |  9280 | `	return SXRET_OK;` |
|        5 |  9281 |  |
|        - |  9282 | `/*` |
|        - |  9283 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9284 | ` * defined below.` |
|        - |  9285 | ` */` |
|        2 |  9286 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9287 |  |
|        3 |  9288 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9289 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9290 | `	ph7_value *pObj;` |
|        - |  9291 | `	SyString sVar;` |
|        - |  9292 | `	/* Perform a string cast */` |
|        3 |  9293 | `	PH7_MemObjToString(pKey);` |
|        3 |  9294 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9295 | `		/* Unavailable variable name */` |
|      ! 0 |  9296 | `		return SXRET_OK;` |
|        - |  9297 | `	}` |
|        3 |  9298 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9299 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9300 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9301 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9302 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9303 | `			);` |
|        2 |  9304 | `	}else{` |
|      ! 0 |  9305 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9306 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9307 | `	}` |
|        3 |  9308 | `	sVar.zString = pAux->zWorker;` |
|        - |  9309 | `	/* Extract the variable */` |
|        3 |  9310 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9311 | `	if( pObj ){` |
|        3 |  9312 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9313 | `	}` |
|        3 |  9314 | `	return SXRET_OK;` |
|        2 |  9315 |  |
|        - |  9316 | `/*` |
|        - |  9317 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9318 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9319 | ` * Parameters` |
|        - |  9320 | ` * $types` |
|        - |  9321 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9322 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9323 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9324 | ` *  POST includes the POST uploaded file information.` |
|        - |  9325 | ` *  Note:` |
|        - |  9326 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9327 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9328 | ` * $prefix` |
|        - |  9329 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9330 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9331 | ` *  variable named $pref_userid.` |
|        - |  9332 | ` * Return` |
|        - |  9333 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9334 | ` */` |
|        2 |  9335 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9336 |  |
|        - |  9337 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9338 | `	extract_aux_data sAux;` |
|        - |  9339 | `	int nLen,nPrefixLen;` |
|        - |  9340 | `	ph7_value *pSuper;` |
|        - |  9341 | `	ph7_vm *pVm;` |
|        - |  9342 | `	/* By default import only $_GET variables  */` |
|        3 |  9343 | `	zImport = "G";` |
|        3 |  9344 | `	nLen = (int)sizeof(char);` |
|        3 |  9345 | `	zPrefix = 0;` |
|        3 |  9346 | `	nPrefixLen = 0;` |
|        3 |  9347 | `	if( nArg > 0 ){` |
|        3 |  9348 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9349 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9350 | `		}` |
|        3 |  9351 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9352 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9353 | `		}` |
|        1 |  9354 | `	}` |
|        - |  9355 | `	/* Point to the underlying VM */` |
|        3 |  9356 | `	pVm = pCtx->pVm;` |
|        - |  9357 | `	/* Initialize the aux data */` |
|        3 |  9358 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9359 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9360 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9361 | `	sAux.pVm = pVm;` |
|        - |  9362 | `	/* Extract */` |
|        3 |  9363 | `	zEnd = &zImport[nLen];` |
|        5 |  9364 | `	while( zImport < zEnd ){` |
|        3 |  9365 | `		int c = zImport[0];` |
|        3 |  9366 | `		pSuper = 0;` |
|        3 |  9367 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9368 | `			/* Import $_GET variables */` |
|        3 |  9369 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9370 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9371 | `			/* Import $_POST variables */` |
|      ! 0 |  9372 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9373 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9374 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9375 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9376 | `		}` |
|        3 |  9377 | `		if( pSuper ){` |
|        - |  9378 | `			/* Iterate throw array entries */` |
|        3 |  9379 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9380 | `		}` |
|        - |  9381 | `		/* Advance the cursor */` |
|        3 |  9382 | `		zImport++;` |
|        1 |  9383 | `	}` |
|        - |  9384 | `	/* All done,return TRUE*/` |
|        3 |  9385 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9386 | `	return PH7_OK;` |
|        1 |  9387 |  |
|        - |  9388 | `/*` |
|        - |  9389 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9390 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9391 | ` * information.` |
|        - |  9392 | ` */` |
|     9714 |  9393 | `static sxi32 VmEvalChunk(` |
|        - |  9394 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9395 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9396 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9397 | `	int iFlags,         /* Compile flag */` |
|        - |  9398 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9399 | `	)` |
|        2 |  9400 |  |
|        - |  9401 | `	SySet *pByteCode,aByteCode;` |
|     9716 |  9402 | `	ProcConsumer xErr = 0;` |
|     9716 |  9403 | `	void *pErrData = 0;` |
|        - |  9404 | `	/* Initialize bytecode container */` |
|     9716 |  9405 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9716 |  9406 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9407 | `	/* Reset the code generator */` |
|     9716 |  9408 | `	if( bTrueReturn ){` |
|        - |  9409 | `		/* Included file,log compile-time errors */` |
|     7529 |  9410 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7529 |  9411 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3764 |  9412 | `	}` |
|     9716 |  9413 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9414 | `	/* Swap bytecode container */` |
|     9716 |  9415 | `	pByteCode = pVm->pByteContainer;` |
|     9716 |  9416 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9417 | `	/* Compile the chunk */` |
|     9716 |  9418 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14573 |  9419 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9420 | `		/* Compilation error,return false */` |
|        3 |  9421 | `		if( pCtx ){` |
|        3 |  9422 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9423 | `		}` |
|        2 |  9424 | `	}else{` |
|        - |  9425 | `		/* Mount any newly defined classes */` |
|        - |  9426 | `		SyHashEntry *pEntry;` |
|        - |  9427 | `		ph7_class *pClass;` |
|        - |  9428 | `		ph7_value sResult; /* Return value */` |
|        - |  9429 | `		sxi32 rc;` |
|     9714 |  9430 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   271038 |  9431 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   256470 |  9432 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9433 | `			/* Only mount classes that haven't been mounted yet */` |
|   256470 |  9434 | `			if( !pClass->bMounted ){` |
|    58130 |  9435 | `				rc = VmMountUserClass(pVm,pClass);` |
|    58130 |  9436 | `				if( rc != SXRET_OK ){` |
|        - |  9437 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9438 | `					if( pCtx ){` |
|      ! 0 |  9439 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9440 | `					}` |
|      ! 0 |  9441 | `					goto Cleanup;` |
|        - |  9442 | `				}` |
|    29064 |  9443 | `			}` |
|        2 |  9444 | `		}` |
|     9714 |  9445 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9446 | `			/* Out of memory */` |
|      ! 0 |  9447 | `			if( pCtx ){` |
|      ! 0 |  9448 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9449 | `			}` |
|      ! 0 |  9450 | `			goto Cleanup;` |
|        - |  9451 | `		}` |
|     9714 |  9452 | `		if( bTrueReturn ){` |
|        - |  9453 | `			/* Assume a boolean true return value */` |
|     7529 |  9454 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3765 |  9455 | `		}else{` |
|        - |  9456 | `			/* Assume a null return value */` |
|     2186 |  9457 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9458 | `		}` |
|        - |  9459 | `		/* Execute the compiled chunk */` |
|     9714 |  9460 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9714 |  9461 | `		if( pCtx ){` |
|        - |  9462 | `			/* Set the execution result */` |
|     7542 |  9463 | `			ph7_result_value(pCtx,&sResult);` |
|     3770 |  9464 | `		}` |
|     9714 |  9465 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9466 | `	}` |
|     4857 |  9467 | `Cleanup:` |
|        - |  9468 | `	/* Cleanup the mess left behind */` |
|     9716 |  9469 | `	pVm->pByteContainer = pByteCode;` |
|     9716 |  9470 | `	SySetRelease(&aByteCode);` |
|     9716 |  9471 | `	return SXRET_OK;` |
|        2 |  9472 |  |
|        - |  9473 | `/*` |
|        - |  9474 | ` * value eval(string $code)` |
|        - |  9475 | ` *   Evaluate a string as PHP code.` |
|        - |  9476 | ` * Parameter` |
|        - |  9477 | ` *  code: PHP code to evaluate.` |
|        - |  9478 | ` * Return` |
|        - |  9479 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9480 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9481 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9482 | ` */` |
|       16 |  9483 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9484 |  |
|        - |  9485 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9486 | `	if( nArg < 1 ){` |
|        - |  9487 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9488 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9489 | `		return SXRET_OK;` |
|        - |  9490 | `	}` |
|        - |  9491 | `	/* Chunk to evaluate */` |
|       18 |  9492 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9493 | `	if( sChunk.nByte < 1 ){` |
|        - |  9494 | `		/* Empty string,return NULL */` |
|        3 |  9495 | `		ph7_result_null(pCtx);` |
|        3 |  9496 | `		return SXRET_OK;` |
|        - |  9497 | `	}` |
|        - |  9498 | `	/* Eval the chunk */` |
|       16 |  9499 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9500 | `	return SXRET_OK;` |
|       10 |  9501 |  |
|        - |  9502 | `/*` |
|        - |  9503 | ` * Check if a file path is already included.` |
|        - |  9504 | ` */` |
|    15052 |  9505 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9506 |  |
|        - |  9507 | `	SyString *aEntries;` |
|        - |  9508 | `	sxu32 n;` |
|    15053 |  9509 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9510 | `	/* Perform a linear search */` |
| 56630321 |  9511 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56615275 |  9512 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9513 | `			/* Already included */` |
|        7 |  9514 | `			return TRUE;` |
|        - |  9515 | `		}` |
| 28307635 |  9516 | `	}` |
|    15047 |  9517 | `	return FALSE;` |
|     7527 |  9518 |  |
|        - |  9519 | `/*` |
|        - |  9520 | ` * Push a file path in the appropriate VM container.` |
|        - |  9521 | ` */` |
|    17216 |  9522 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9523 |  |
|        - |  9524 | `	SyString sPath;` |
|        - |  9525 | `	char *zDup;` |
|        - |  9526 | `#ifdef __WINNT__` |
|        - |  9527 | `	char *zCur;` |
|        - |  9528 | `#endif` |
|        - |  9529 | `	sxi32 rc;` |
|    17218 |  9530 | `	if( nLen < 0 ){` |
|     2166 |  9531 | `		nLen = SyStrlen(zPath);` |
|     1082 |  9532 | `	}` |
|        - |  9533 | `	/* Duplicate the file path first */` |
|    17218 |  9534 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17218 |  9535 | `	if( zDup == 0 ){` |
|      ! 0 |  9536 | `		return SXERR_MEM;` |
|        - |  9537 | `	}` |
|        - |  9538 | `#ifdef __WINNT__` |
|        - |  9539 | `	/* Normalize path on windows` |
|        - |  9540 | `	 * Example:` |
|        - |  9541 | `	 *    Path/To/File.php` |
|        - |  9542 | `	 * becomes` |
|        - |  9543 | `	 *   path\to\file.php` |
|        - |  9544 | `	 */` |
|        2 |  9545 | `	zCur = zDup;` |
|        2 |  9546 | `	while( zCur[0] != 0 ){` |
|        2 |  9547 | `		if( zCur[0] == '/' ){` |
|        2 |  9548 | `			zCur[0] = '\\';` |
|        2 |  9549 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9550 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9551 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9552 | `		}` |
|        2 |  9553 | `		zCur++;` |
|        2 |  9554 | `	}` |
|        - |  9555 | `#endif` |
|        - |  9556 | `	/* Install the file path */` |
|    17218 |  9557 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17218 |  9558 | `	if( !bMain ){` |
|    15053 |  9559 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9560 | `			/* Already included */` |
|        7 |  9561 | `			*pNew = 0;` |
|        4 |  9562 | `		}else{` |
|        - |  9563 | `			/* Insert in the corresponding container */` |
|    15047 |  9564 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15047 |  9565 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9566 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9567 | `				return rc;` |
|        - |  9568 | `			}` |
|    15047 |  9569 | `			*pNew = 1;` |
|        - |  9570 | `		}` |
|     7526 |  9571 | `	}` |
|    17218 |  9572 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17218 |  9573 | `	return SXRET_OK;` |
|     8610 |  9574 |  |
|        - |  9575 | `/*` |
|        - |  9576 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9577 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9578 | ` * indicates failure.` |
|        - |  9579 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9580 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9581 | ` * operations.` |
|        - |  9582 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9583 | ` * this function is a no-op.` |
|        - |  9584 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9585 | ` * constructs for more information.` |
|        - |  9586 | ` */` |
|     7534 |  9587 | `static sxi32 VmExecIncludedFile(` |
|        - |  9588 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9589 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9590 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9591 | `	 )` |
|        2 |  9592 |  |
|        - |  9593 | `	sxi32 rc;` |
|        - |  9594 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9595 | `	const ph7_io_stream *pStream;` |
|        - |  9596 | `	SyBlob sContents;` |
|        - |  9597 | `	void *pHandle;` |
|        - |  9598 | `	ph7_vm *pVm;` |
|        - |  9599 | `	int isNew;` |
|        - |  9600 | `	/* Initialize fields */` |
|     7536 |  9601 | `	pVm = pCtx->pVm;` |
|     7536 |  9602 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7536 |  9603 | `	isNew = 0;` |
|        - |  9604 | `	/* Extract the associated stream */` |
|     7536 |  9605 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9606 | `	/*` |
|        - |  9607 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9608 | `	 * in a read-only mode.` |
|        - |  9609 | `	 */` |
|     7536 |  9610 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7536 |  9611 | `	if( pHandle == 0 ){` |
|        3 |  9612 | `		return SXERR_IO;` |
|        - |  9613 | `	}` |
|     7533 |  9614 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7533 |  9615 | `	if( IncludeOnce && !isNew ){` |
|        - |  9616 | `		/* Already included */` |
|        5 |  9617 | `		rc = SXERR_EXISTS;` |
|        3 |  9618 | `	}else{` |
|        - |  9619 | `		/* Read the whole file contents */` |
|     7529 |  9620 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7529 |  9621 | `		if( rc == SXRET_OK ){` |
|        - |  9622 | `			SyString sScript;` |
|        - |  9623 | `			/* Compile and execute the script */` |
|     7529 |  9624 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7529 |  9625 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3764 |  9626 | `		}` |
|        - |  9627 | `	}` |
|        - |  9628 | `	/* Pop from the set of included file */` |
|     7533 |  9629 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9630 | `	/* Close the handle */` |
|     7533 |  9631 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9632 | `	/* Release the working buffer */` |
|     7533 |  9633 | `	SyBlobRelease(&sContents);` |
|        - |  9634 | `#else` |
|        - |  9635 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9636 | `	SXUNUSED(pPath);` |
|        - |  9637 | `	SXUNUSED(IncludeOnce);` |
|        - |  9638 | `	rc = SXERR_IO;` |
|        - |  9639 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7533 |  9640 | `	return rc;` |
|     3769 |  9641 |  |
|        - |  9642 | `/*` |
|        - |  9643 | ` * string get_include_path(void)` |
|        - |  9644 | ` *  Gets the current include_path configuration option.` |
|        - |  9645 | ` * Parameter` |
|        - |  9646 | ` *  None` |
|        - |  9647 | ` * Return` |
|        - |  9648 | ` *  Included paths as a string` |
|        - |  9649 | ` */` |
|        2 |  9650 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9651 |  |
|        3 |  9652 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9653 | `	SyString *aEntry;` |
|        - |  9654 | `	int dir_sep;` |
|        - |  9655 | `	sxu32 n;` |
|        - |  9656 | `#ifdef __WINNT__` |
|        1 |  9657 | `	dir_sep = ';';` |
|        - |  9658 | `#else` |
|        - |  9659 | `	/* Assume UNIX path separator */` |
|        2 |  9660 | `	dir_sep = ':';` |
|        - |  9661 | `#endif` |
|        1 |  9662 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9663 | `	SXUNUSED(apArg);` |
|        - |  9664 | `	/* Point to the list of import paths */` |
|        3 |  9665 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9666 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9667 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9668 | `		if( n > 0 ){` |
|        - |  9669 | `			/* Append dir seprator */` |
|      ! 0 |  9670 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9671 | `		}` |
|        - |  9672 | `		/* Append path */` |
|        3 |  9673 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9674 | `	}` |
|        3 |  9675 | `	return PH7_OK;` |
|        1 |  9676 |  |
|        - |  9677 | `/*` |
|        - |  9678 | ` * string get_get_included_files(void)` |
|        - |  9679 | ` *  Gets the current include_path configuration option.` |
|        - |  9680 | ` * Parameter` |
|        - |  9681 | ` *  None` |
|        - |  9682 | ` * Return` |
|        - |  9683 | ` *  Included paths as a string` |
|        - |  9684 | ` */` |
|        2 |  9685 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9686 |  |
|        3 |  9687 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9688 | `	ph7_value *pArray,*pWorker;` |
|        - |  9689 | `	SyString *pEntry;` |
|        - |  9690 | `	int c,d;` |
|        - |  9691 | `	/* Create an array and a working value */` |
|        3 |  9692 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9693 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9694 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9695 | `		/* Out of memory,return null */` |
|      ! 0 |  9696 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9697 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9698 | `		SXUNUSED(apArg);` |
|      ! 0 |  9699 | `		return PH7_OK;` |
|        - |  9700 | `	}` |
|        3 |  9701 | `	c = d = '/';` |
|        - |  9702 | `#ifdef __WINNT__` |
|        1 |  9703 | `	d = '\\';` |
|        - |  9704 | `#endif` |
|        - |  9705 | `	/* Iterate throw entries */` |
|        3 |  9706 | `	SySetResetCursor(pFiles);` |
|     3689 |  9707 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9708 | `		const char *zBase,*zEnd;` |
|        - |  9709 | `		int iLen;` |
|        - |  9710 | `		/* reset the string cursor */` |
|     3687 |  9711 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9712 | `		/* Extract base name */` |
|     3687 |  9713 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9714 | `		/* Ignore trailing '/' */` |
|     5530 |  9715 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9716 | `			zEnd--;` |
|      ! 0 |  9717 | `		}` |
|     3687 |  9718 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113660 |  9719 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108131 |  9720 | `			zEnd--;` |
|        1 |  9721 | `		}` |
|     3687 |  9722 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 |  9723 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9724 | `		/* Copy entry name */` |
|     3687 |  9725 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9726 | `		/* Perform the insertion */` |
|     3687 |  9727 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9728 | `	}` |
|        - |  9729 | `	/* All done,return the created array */` |
|        3 |  9730 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9731 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9732 | `	 * by the engine as soon we return from this foreign` |
|        - |  9733 | `	 * function.` |
|        - |  9734 | `	 */` |
|        3 |  9735 | `	return PH7_OK;` |
|        2 |  9736 |  |
|        - |  9737 | `/*` |
|        - |  9738 | ` * include:` |
|        - |  9739 | ` * According to the PHP reference manual.` |
|        - |  9740 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9741 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9742 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9743 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9744 | ` *  and the current working directory before failing. The include()` |
|        - |  9745 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9746 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9747 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9748 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9749 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9750 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9751 | ` *  directory to find the requested file.` |
|        - |  9752 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9753 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9754 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9755 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9756 | ` */` |
|     7522 |  9757 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9758 |  |
|        - |  9759 | `	SyString sFile;` |
|        - |  9760 | `	sxi32 rc;` |
|     7524 |  9761 | `	if( nArg < 1 ){` |
|        - |  9762 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9763 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9764 | `		return SXRET_OK;` |
|        - |  9765 | `	}` |
|        - |  9766 | `	/* File to include */` |
|     7524 |  9767 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7524 |  9768 | `	if( sFile.nByte < 1 ){` |
|        - |  9769 | `		/* Empty string,return NULL */` |
|      ! 0 |  9770 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9771 | `		return SXRET_OK;` |
|        - |  9772 | `	}` |
|        - |  9773 | `	/* Open,compile and execute the desired script */` |
|     7524 |  9774 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7524 |  9775 | `	if( rc != SXRET_OK ){` |
|        - |  9776 | `		/* Emit a warning and return false */` |
|        3 |  9777 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9778 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9779 | `	}` |
|     7524 |  9780 | `	return SXRET_OK;` |
|     3763 |  9781 |  |
|        - |  9782 | `/*` |
|        - |  9783 | ` * include_once:` |
|        - |  9784 | ` *  According to the PHP reference manual.` |
|        - |  9785 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9786 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9787 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9788 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9789 | ` *   just once.` |
|        - |  9790 | ` */` |
|        4 |  9791 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9792 |  |
|        - |  9793 | `	SyString sFile;` |
|        - |  9794 | `	sxi32 rc;` |
|        5 |  9795 | `	if( nArg < 1 ){` |
|        - |  9796 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9797 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9798 | `		return SXRET_OK;` |
|        - |  9799 | `	}` |
|        - |  9800 | `	/* File to include */` |
|        5 |  9801 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9802 | `	if( sFile.nByte < 1 ){` |
|        - |  9803 | `		/* Empty string,return NULL */` |
|      ! 0 |  9804 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9805 | `		return SXRET_OK;` |
|        - |  9806 | `	}` |
|        - |  9807 | `	/* Open,compile and execute the desired script */` |
|        5 |  9808 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9809 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9810 | `		/* File already included,return TRUE */` |
|        3 |  9811 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9812 | `		return SXRET_OK;` |
|        - |  9813 | `	}` |
|        3 |  9814 | `	if( rc != SXRET_OK ){` |
|        - |  9815 | `		/* Emit a warning and return false */` |
|      ! 0 |  9816 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9817 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9818 | ` 	}` |
|        3 |  9819 | `	return SXRET_OK;` |
|        3 |  9820 |  |
|        - |  9821 | `/*` |
|        - |  9822 | ` * require.` |
|        - |  9823 | ` *  According to the PHP reference manual.` |
|        - |  9824 | ` *   require() is identical to include() except upon failure it will` |
|        - |  9825 | ` *   also produce a fatal level error.` |
|        - |  9826 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  9827 | ` *   emits a warning  which allows the script to continue.` |
|        - |  9828 | ` */` |
|        4 |  9829 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9830 |  |
|        - |  9831 | `	SyString sFile;` |
|        - |  9832 | `	sxi32 rc;` |
|        5 |  9833 | `	if( nArg < 1 ){` |
|        - |  9834 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9835 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9836 | `		return SXRET_OK;` |
|        - |  9837 | `	}` |
|        - |  9838 | `	/* File to include */` |
|        5 |  9839 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9840 | `	if( sFile.nByte < 1 ){` |
|        - |  9841 | `		/* Empty string,return NULL */` |
|      ! 0 |  9842 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9843 | `		return SXRET_OK;` |
|        - |  9844 | `	}` |
|        - |  9845 | `	/* Open,compile and execute the desired script */` |
|        5 |  9846 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 |  9847 | `	if( rc != SXRET_OK ){` |
|        - |  9848 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9849 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9850 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9851 | `		return PH7_ABORT;` |
|        - |  9852 | `	}` |
|        5 |  9853 | `	return SXRET_OK;` |
|        3 |  9854 |  |
|        - |  9855 | `/*` |
|        - |  9856 | ` * require_once:` |
|        - |  9857 | ` *  According to the PHP reference manual.` |
|        - |  9858 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  9859 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  9860 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  9861 | ` *   and how it differs from its non _once siblings.` |
|        - |  9862 | ` */` |
|        4 |  9863 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9864 |  |
|        - |  9865 | `	SyString sFile;` |
|        - |  9866 | `	sxi32 rc;` |
|        5 |  9867 | `	if( nArg < 1 ){` |
|        - |  9868 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9869 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9870 | `		return SXRET_OK;` |
|        - |  9871 | `	}` |
|        - |  9872 | `	/* File to include */` |
|        5 |  9873 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9874 | `	if( sFile.nByte < 1 ){` |
|        - |  9875 | `		/* Empty string,return NULL */` |
|      ! 0 |  9876 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9877 | `		return SXRET_OK;` |
|        - |  9878 | `	}` |
|        - |  9879 | `	/* Open,compile and execute the desired script */` |
|        5 |  9880 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9881 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9882 | `		/* File already included,return TRUE */` |
|        3 |  9883 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9884 | `		return SXRET_OK;` |
|        - |  9885 | `	}` |
|        3 |  9886 | `	if( rc != SXRET_OK ){` |
|        - |  9887 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9888 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9889 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9890 | `		return PH7_ABORT;` |
|        - |  9891 | `	}` |
|        3 |  9892 | `	return SXRET_OK;` |
|        3 |  9893 |  |
|        - |  9894 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  9895 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  9896 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  9897 | `/* Table of built-in VM functions. */` |
|        - |  9898 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9899 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9900 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9901 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9902 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9903 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9904 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9905 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9906 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9907 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9908 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9909 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9910 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9911 | `	    /* Constants management */` |
|        - |  9912 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9913 | `	{ "define",   vm_builtin_define               },` |
|        - |  9914 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9915 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9916 | `	   /* Class/Object functions */` |
|        - |  9917 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9918 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9919 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9920 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9921 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9922 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9923 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9924 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9925 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9926 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9927 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - |  9928 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - |  9929 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - |  9930 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - |  9931 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - |  9932 | `	{ "is_a", vm_builtin_is_a },` |
|        - |  9933 | `	   /* Random numbers/strings generators */` |
|        - |  9934 | `	{ "rand",          vm_builtin_rand            },` |
|        - |  9935 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - |  9936 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - |  9937 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - |  9938 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - |  9939 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9940 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9941 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - |  9942 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9943 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9944 | `	   /* Language constructs functions */` |
|        - |  9945 | `	{ "echo",  vm_builtin_echo                    },` |
|        - |  9946 | `	{ "print", vm_builtin_print                   },` |
|        - |  9947 | `	{ "exit",  vm_builtin_exit                    },` |
|        - |  9948 | `	{ "die",   vm_builtin_exit                    },` |
|        - |  9949 | `	{ "eval",  vm_builtin_eval                    },` |
|        - |  9950 | `	  /* Variable handling functions */` |
|        - |  9951 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - |  9952 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - |  9953 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - |  9954 | `	{ "isset",     vm_builtin_isset                },` |
|        - |  9955 | `	{ "unset",     vm_builtin_unset                },` |
|        - |  9956 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - |  9957 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - |  9958 | `	{ "var_export",vm_builtin_var_export           },` |
|        - |  9959 | `	  /* Ouput control functions */` |
|        - |  9960 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - |  9961 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - |  9962 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - |  9963 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - |  9964 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - |  9965 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - |  9966 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - |  9967 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - |  9968 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - |  9969 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - |  9970 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - |  9971 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - |  9972 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - |  9973 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - |  9974 | `	  /* Assertion functions */` |
|        - |  9975 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - |  9976 | `	{ "assert",          vm_builtin_assert         },` |
|        - |  9977 | `	  /* Error reporting functions */` |
|        - |  9978 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - |  9979 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - |  9980 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - |  9981 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - |  9982 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - |  9983 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - |  9984 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - |  9985 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - |  9986 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - |  9987 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - |  9988 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - |  9989 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - |  9990 | `	  /* Release info */` |
|        - |  9991 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - |  9992 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - |  9993 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - |  9994 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - |  9995 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - |  9996 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - |  9997 | `	  /* hashmap */` |
|        - |  9998 | `	{"compact",          vm_builtin_compact       },` |
|        - |  9999 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10000 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10001 | `	  /* URL related function */` |
|        - | 10002 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10003 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10004 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10005 | `	   /* XML processing functions */` |
|        - | 10006 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10007 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10008 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10009 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10010 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10011 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10012 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10013 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10014 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10015 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10016 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10017 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10018 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10019 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10020 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10021 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10022 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10023 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10024 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10025 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10026 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10027 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10028 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10029 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10030 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10031 | `	   /* Command line processing */` |
|        - | 10032 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10033 | `	   /* JSON encoding/decoding */` |
|        - | 10034 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10035 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10036 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10037 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10038 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10039 | `	   /* Files/URI inclusion facility */` |
|        - | 10040 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10041 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10042 | `	{ "include",      vm_builtin_include          },` |
|        - | 10043 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10044 | `	{ "require",      vm_builtin_require          },` |
|        - | 10045 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10046 | `};` |
|        - | 10047 | `/*` |
|        - | 10048 | ` * Register the built-in VM functions defined above.` |
|        - | 10049 | ` */` |
|     1934 | 10050 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10051 |  |
|        - | 10052 | `	sxi32 rc;` |
|        - | 10053 | `	sxu32 n;` |
|   241752 | 10054 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10055 | `		/* Note that these special functions have access` |
|        - | 10056 | `		 * to the underlying virtual machine as their` |
|        - | 10057 | `		 * private data.` |
|        - | 10058 | `		 */` |
|   239818 | 10059 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   239818 | 10060 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10061 | `			return rc;` |
|        - | 10062 | `		}` |
|   119910 | 10063 | `	}` |
|     1936 | 10064 | `	return SXRET_OK;` |
|      969 | 10065 |  |
|        - | 10066 | `/*` |
|        - | 10067 | ` * Check if the given name refer to an installed class.` |
|        - | 10068 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10069 | ` */` |
|    14176 | 10070 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10071 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10072 | `	const char *zName,  /* Name of the target class */` |
|        - | 10073 | `	sxu32 nByte,        /* zName length */` |
|        - | 10074 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10075 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10076 | `						 */` |
|        - | 10077 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10078 | `	)` |
|        2 | 10079 |  |
|        - | 10080 | `	SyHashEntry *pEntry;` |
|        - | 10081 | `	ph7_class *pClass;` |
|     7088 | 10082 | `		SXUNUSED(iNest);` |
|        - | 10083 | `	/* Perform a hash lookup */` |
|    14178 | 10084 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10085 |  |
|    14178 | 10086 | `	if( pEntry == 0 ){` |
|        - | 10087 | `		/* No such entry,return NULL */` |
|      ! 0 | 10088 | `		return 0;` |
|        - | 10089 | `	}` |
|    14178 | 10090 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    14178 | 10091 | `	if( !iLoadable ){` |
|        - | 10092 | `		/* Return the first class seen */` |
|    13196 | 10093 | `		return pClass;` |
|      ! 0 | 10094 | `	}else{` |
|        - | 10095 | `		/* Check the collision list */` |
|      984 | 10096 | `		while(pClass){` |
|      984 | 10097 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10098 | `				/* Class is loadable */` |
|      984 | 10099 | `				return pClass;` |
|        - | 10100 | `			}` |
|        - | 10101 | `			/* Point to the next entry */` |
|      ! 0 | 10102 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10103 | `		}` |
|        - | 10104 | `	}` |
|        - | 10105 | `	/* No such loadable class */` |
|      ! 0 | 10106 | `	return 0;` |
|     7090 | 10107 |  |
|        - | 10108 | `/*` |
|        - | 10109 | ` * Reference Table Implementation` |
|        - | 10110 | ` * Status: stable <chm@symisc.net>` |
|        - | 10111 | ` * Intro` |
|        - | 10112 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10113 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10114 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10115 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10116 | ` *  Refer to the official for more information on this powerful` |
|        - | 10117 | ` *  extension.` |
|        - | 10118 | ` */` |
|        - | 10119 | `/*` |
|        - | 10120 | ` * Allocate a new reference entry.` |
|        - | 10121 | ` */` |
|  2971434 | 10122 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10123 |  |
|        - | 10124 | `	VmRefObj *pRef;` |
|        - | 10125 | `	/* Allocate a new instance */` |
|  2971436 | 10126 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2971436 | 10127 | `	if( pRef == 0 ){` |
|      ! 0 | 10128 | `		return 0;` |
|        - | 10129 | `	}` |
|        - | 10130 | `	/* Zero the structure */` |
|  2971436 | 10131 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10132 | `	/* Initialize fields */` |
|  2971436 | 10133 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2971436 | 10134 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2971436 | 10135 | `	pRef->nIdx = nIdx;` |
|  2971436 | 10136 | `	return pRef;` |
|  1485719 | 10137 |  |
|        - | 10138 | `/*` |
|        - | 10139 | ` * Default hash function used by the reference table` |
|        - | 10140 | ` * for lookup/insertion operations.` |
|        - | 10141 | ` */` |
| 16506390 | 10142 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10143 |  |
|        - | 10144 | `	/* Calculate the hash based on the memory object index */` |
| 16506392 | 10145 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10146 |  |
|        - | 10147 | `/*` |
|        - | 10148 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10149 | ` * in the reference table.` |
|        - | 10150 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10151 | ` * otherwise.` |
|        - | 10152 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10153 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10154 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10155 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10156 | ` * Refer to the official for more information on this powerful` |
|        - | 10157 | ` * extension.` |
|        - | 10158 | ` */` |
|  8875090 | 10159 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10160 |  |
|        - | 10161 | `	VmRefObj *pRef;` |
|        - | 10162 | `	sxu32 nBucket;` |
|        - | 10163 | `	/* Point to the appropriate bucket */` |
|  8875092 | 10164 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10165 | `	/* Perform the lookup */` |
|  8875092 | 10166 | `	pRef = pVm->apRefObj[nBucket];` |
| 18761554 | 10167 | `	for(;;){` |
| 37527858 | 10168 | `		if( pRef == 0 ){` |
|  3042636 | 10169 | `			break;` |
|        - | 10170 | `		}` |
| 34485224 | 10171 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10172 | `			/* Entry found */` |
|  5832458 | 10173 | `			return pRef;` |
|        - | 10174 | `		}` |
|        - | 10175 | `		/* Point to the next entry */` |
| 28652768 | 10176 | `		pRef = pRef->pNextCollide;` |
|        2 | 10177 | `	}` |
|        - | 10178 | `	/* No such entry,return NULL */` |
|  3042636 | 10179 | `	return 0;` |
|  4437547 | 10180 |  |
|        - | 10181 | `/*` |
|        - | 10182 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10183 | ` *` |
|        - | 10184 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10185 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10186 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10187 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10188 | ` * Refer to the official for more information on this powerful` |
|        - | 10189 | ` * extension.` |
|        - | 10190 | ` */` |
|  2971434 | 10191 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10192 |  |
|        - | 10193 | `	sxu32 nBucket;` |
|  2971436 | 10194 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10195 | `		VmRefObj **apNew;` |
|        - | 10196 | `		sxu32 nNew;` |
|        - | 10197 | `		/* Allocate a larger table */` |
|     3038 | 10198 | `		nNew = pVm->nRefSize << 1;` |
|     3038 | 10199 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3038 | 10200 | `		if( apNew ){` |
|     3038 | 10201 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10202 | `			sxu32 n;` |
|        - | 10203 | `			/* Zero the structure */` |
|     3038 | 10204 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10205 | `			/* Rehash all referenced entries */` |
|  2830550 | 10206 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10207 | `				/* Remove old collision links */` |
|  2827514 | 10208 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10209 | `				/* Point to the appropriate bucket */` |
|  2827514 | 10210 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10211 | `				/* Insert the entry  */` |
|  2827514 | 10212 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2827514 | 10213 | `				if( apNew[nBucket] ){` |
|  2298896 | 10214 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10215 | `				}` |
|  2827514 | 10216 | `				apNew[nBucket] = pEntry;` |
|        - | 10217 | `				/* Point to the next entry */` |
|  2827514 | 10218 | `				pEntry = pEntry->pNext;` |
|  1413758 | 10219 | `			}` |
|        - | 10220 | `			/* Release the old table */` |
|     3038 | 10221 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10222 | `			/* Install the new one */` |
|     3038 | 10223 | `			pVm->apRefObj = apNew;` |
|     3038 | 10224 | `			pVm->nRefSize = nNew;` |
|     1518 | 10225 | `		}` |
|     1518 | 10226 | `	}` |
|        - | 10227 | `	/* Point to the appropriate bucket */` |
|  2971436 | 10228 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10229 | `	/* Insert the entry */` |
|  2971436 | 10230 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2971436 | 10231 | `	if( pVm->apRefObj[nBucket] ){` |
|  2457785 | 10232 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1228974 | 10233 | `	}` |
|  2971436 | 10234 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2971436 | 10235 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2971436 | 10236 | `	pVm->nRefUsed++;` |
|  2971436 | 10237 | `	return SXRET_OK;` |
|        2 | 10238 |  |
|        - | 10239 | `/*` |
|        - | 10240 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10241 | ` * the reference table.` |
|        - | 10242 | ` * This function is invoked when the user perform an unset` |
|        - | 10243 | ` * call [i.e: unset($var); ].` |
|        - | 10244 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10245 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10246 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10247 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10248 | ` * Refer to the official for more information on this powerful` |
|        - | 10249 | ` * extension.` |
|        - | 10250 | ` */` |
|  2943512 | 10251 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10252 |  |
|        - | 10253 | `	ph7_hashmap_node **apNode;` |
|        - | 10254 | `	SyHashEntry **apEntry;` |
|        - | 10255 | `	sxu32 n;` |
|        - | 10256 | `	/* Point to the reference table */` |
|  2943514 | 10257 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2943514 | 10258 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10259 | `	/* Unlink the entry from the reference table */` |
|  3019676 | 10260 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    76164 | 10261 | `		if( apEntry[n] ){` |
|    76114 | 10262 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    38056 | 10263 | `		}` |
|    38083 | 10264 | `	}` |
|  5812956 | 10265 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2869444 | 10266 | `		if( apNode[n] ){` |
|     5635 | 10267 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10268 | `		}` |
|  1434723 | 10269 | `	}` |
|  2943514 | 10270 | `	if( pRef->pPrevCollide ){` |
|  1111160 | 10271 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   555407 | 10272 | `	}else{` |
|  1832356 | 10273 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10274 | `	}` |
|  2943514 | 10275 | `	if( pRef->pNextCollide ){` |
|  1650156 | 10276 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   825182 | 10277 | `	}` |
|  2943514 | 10278 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10279 | `	/* Release the node */` |
|  2943514 | 10280 | `	SySetRelease(&pRef->aReference);` |
|  2943514 | 10281 | `	SySetRelease(&pRef->aArrEntries);` |
|  2943514 | 10282 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2943514 | 10283 | `	pVm->nRefUsed--;` |
|  2943514 | 10284 | `	return SXRET_OK;` |
|        2 | 10285 |  |
|        - | 10286 | `/*` |
|        - | 10287 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10288 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10289 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10290 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10291 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10292 | ` * Refer to the official for more information on this powerful` |
|        - | 10293 | ` * extension.` |
|        - | 10294 | ` */` |
|  2996514 | 10295 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10296 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10297 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10298 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10299 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10300 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10301 | `	)` |
|        2 | 10302 |  |
|  2996516 | 10303 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10304 | `	VmRefObj *pRef;` |
|        - | 10305 | `	/* Check if the referenced object already exists */` |
|  2996516 | 10306 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2996516 | 10307 | `	if( pRef == 0 ){` |
|        - | 10308 | `		/* Create a new entry */` |
|  2971436 | 10309 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2971436 | 10310 | `		if( pRef == 0 ){` |
|      ! 0 | 10311 | `			return SXERR_MEM;` |
|        - | 10312 | `		}` |
|  2971436 | 10313 | `		pRef->iFlags = iFlags;` |
|        - | 10314 | `		/* Install the entry */` |
|  2971436 | 10315 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1485717 | 10316 | `	}` |
|  3001428 | 10317 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10318 | `		/* Safely ignore the exception frame */` |
|     4914 | 10319 | `		pFrame = pFrame->pParent;` |
|        2 | 10320 | `	}` |
|  2996516 | 10321 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10322 | `		VmSlot sRef;` |
|        - | 10323 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10324 | `		 * be deleted when we leave this frame.` |
|        - | 10325 | `		 */` |
|    71232 | 10326 | `		sRef.nIdx = nIdx;` |
|    71232 | 10327 | `		sRef.pUserData = pEntry;` |
|    71232 | 10328 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10329 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10330 | `		}` |
|    35615 | 10331 | `	}` |
|  2996516 | 10332 | `	if( pEntry ){` |
|        - | 10333 | `		/* Address of the hash-entry */` |
|    96126 | 10334 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    48062 | 10335 | `	}` |
|  2996516 | 10336 | `	if( pMapEntry ){` |
|        - | 10337 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2895820 | 10338 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1447909 | 10339 | `	}` |
|  2996516 | 10340 | `	return SXRET_OK;` |
|  1498259 | 10341 |  |
|        - | 10342 | `/*` |
|        - | 10343 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10344 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10345 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10346 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10347 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10348 | ` * Refer to the official for more information on this powerful` |
|        - | 10349 | ` * extension.` |
|        - | 10350 | ` */` |
|  2935044 | 10351 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10352 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10353 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10354 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10355 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10356 | `	)` |
|        2 | 10357 |  |
|        - | 10358 | `	VmRefObj *pRef;` |
|        - | 10359 | `	sxu32 n;` |
|        - | 10360 | `	/* Check if the referenced object already exists */` |
|  2935046 | 10361 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2935046 | 10362 | `	if( pRef == 0 ){` |
|        - | 10363 | `		/* Not such entry */` |
|    71182 | 10364 | `		return SXERR_NOTFOUND;` |
|        - | 10365 | `	}` |
|        - | 10366 | `	/* Remove the desired entry */` |
|  2863866 | 10367 | `	if( pEntry ){` |
|        - | 10368 | `		SyHashEntry **apEntry;` |
|       51 | 10369 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 10370 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 10371 | `			if( apEntry[n] == pEntry ){` |
|        - | 10372 | `				/* Nullify the entry */` |
|       51 | 10373 | `				apEntry[n] = 0;` |
|        - | 10374 | `				/*` |
|        - | 10375 | `				 * NOTE:` |
|        - | 10376 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10377 | `				 * we avoid wasting spaces.` |
|        - | 10378 | `				 */` |
|       25 | 10379 | `			}` |
|       73 | 10380 | `		}` |
|       25 | 10381 | `	}` |
|  2863866 | 10382 | `	if( pMapEntry ){` |
|        - | 10383 | `		ph7_hashmap_node **apNode;` |
|  2863816 | 10384 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5727718 | 10385 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2863904 | 10386 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10387 | `				/* nullify the entry */` |
|  2863816 | 10388 | `				apNode[n] = 0;` |
|  1431907 | 10389 | `			}` |
|  1431953 | 10390 | `		}` |
|  1431907 | 10391 | `	}` |
|  2863866 | 10392 | `	return SXRET_OK;` |
|  1467524 | 10393 |  |
|        - | 10394 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10395 | `/*` |
|        - | 10396 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10397 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10398 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10399 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10400 | ` * For more information on how to register IO stream devices,please` |
|        - | 10401 | ` * refer to the official documentation.` |
|        - | 10402 | ` */` |
|    22488 | 10403 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10404 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10405 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10406 | `	int nByte              /* *pzDevice length*/` |
|        - | 10407 | `	)` |
|        2 | 10408 |  |
|        - | 10409 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10410 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10411 | `	SyString sDev,sCur;` |
|        - | 10412 | `	sxu32 n,nEntry;` |
|        - | 10413 | `	int rc;` |
|        - | 10414 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22490 | 10415 | `	zNext = zCur = zIn = *pzDevice;` |
|    22490 | 10416 | `	zEnd = &zIn[nByte];` |
|  1438113 | 10417 | `	while( zIn < zEnd ){` |
|  1415627 | 10418 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10419 | `			/* Got one */` |
|        3 | 10420 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10421 | `			break;` |
|        - | 10422 | `		}` |
|        - | 10423 | `		/* Advance the cursor */` |
|  1415625 | 10424 | `		zIn++;` |
|        2 | 10425 | `	}` |
|    22490 | 10426 | `	if( zIn >= zEnd ){` |
|        - | 10427 | `		/* No such scheme,return the default stream */` |
|    22488 | 10428 | `		return pVm->pDefStream;` |
|        - | 10429 | `	}` |
|        3 | 10430 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10431 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10432 | `	SyStringFullTrim(&sDev);` |
|        - | 10433 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10434 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10435 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10436 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10437 | `		pStream = apStream[n];` |
|        3 | 10438 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10439 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10440 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10441 | `		if( rc == 0 ){` |
|        - | 10442 | `			/* Stream device found */` |
|        3 | 10443 | `			*pzDevice = zNext;` |
|        3 | 10444 | `			return pStream;` |
|        - | 10445 | `		}` |
|      ! 0 | 10446 | `	}` |
|        - | 10447 | `	/* No such stream,return NULL */` |
|      ! 0 | 10448 | `	return 0;` |
|    11246 | 10449 |  |
|        - | 10450 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10451 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10452 |  |
