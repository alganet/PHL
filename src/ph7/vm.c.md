# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3897/5143 lines (75.77%)

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
|   748036 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   748038 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       23 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   748016 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   748008 |    94 | `	return FALSE;` |
|   374042 |    95 |  |
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
|   337762 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   337764 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   337764 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   337760 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   337760 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   337760 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   337760 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   337760 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   337760 |   142 | `	pCons->xExpand = xExpand;` |
|   337760 |   143 | `	pCons->pUserData = pUserData;` |
|   337760 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   337760 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   337760 |   151 | `	return SXRET_OK;` |
|   168883 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   727320 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   727322 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   727322 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   727322 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   727322 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   727322 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   727322 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   727322 |   185 | `	pFunc->pVm   = pVm;` |
|   727322 |   186 | `	pFunc->xFunc = xFunc;` |
|   727322 |   187 | `	pFunc->pUserData = pUserData;` |
|   727322 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   727322 |   190 | `	*ppOut = pFunc;` |
|   727322 |   191 | `	return SXRET_OK;` |
|   363662 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   728992 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   728994 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   728994 |   213 | `	if( pEntry ){` |
|     1674 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     1674 |   215 | `		pFunc->pUserData = pUserData;` |
|     1674 |   216 | `		pFunc->xFunc = xFunc;` |
|     1674 |   217 | `		SySetReset(&pFunc->aAux);` |
|     1674 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   727322 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   727322 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   727322 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   727322 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   727322 |   233 | `	return SXRET_OK;` |
|   364498 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    80890 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    80892 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    80892 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    80892 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    80892 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    80892 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    80892 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    80892 |   260 | `	pFunc->iFlags = iFlags;` |
|    80892 |   261 | `	pFunc->pUserData = pUserData;` |
|    80892 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    80892 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   267 | ` */` |
|   258348 |   268 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   269 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   270 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   271 | `	SyString *pName     /* Function name */` |
|        - |   272 | `	)` |
|        2 |   273 |  |
|        - |   274 | `	SyHashEntry *pEntry;` |
|        - |   275 | `	sxi32 rc;` |
|   258350 |   276 | `	if( pName == 0 ){` |
|        - |   277 | `		/* Use the built-in name */` |
|    25286 |   278 | `		pName = &pFunc->sName;` |
|    12642 |   279 | `	}` |
|        - |   280 | `	/* Check for duplicates (functions with the same name) first */` |
|   258350 |   281 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   258350 |   282 | `	if( pEntry ){` |
|   192946 |   283 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   192946 |   284 | `		if( pLink != pFunc ){` |
|        - |   285 | `			/* Link */` |
|      179 |   286 | `			pFunc->pNextName = pLink;` |
|      179 |   287 | `			pEntry->pUserData = pFunc;` |
|       89 |   288 | `		}` |
|   192946 |   289 | `		return SXRET_OK;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* First time seen */` |
|    65406 |   292 | `	pFunc->pNextName = 0;` |
|    65406 |   293 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    65406 |   294 | `	return rc;` |
|   129176 |   295 |  |
|        - |   296 | `/*` |
|        - |   297 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   298 | ` */` |
|    21268 |   299 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   300 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   301 | `	ph7_class *pClass /* Target Class */` |
|        - |   302 | `	)` |
|        2 |   303 |  |
|    21270 |   304 | `	SyString *pName = &pClass->sName;` |
|        - |   305 | `	SyHashEntry *pEntry;` |
|        - |   306 | `	sxi32 rc;` |
|        - |   307 | `	/* Check for duplicates */` |
|    21270 |   308 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    21270 |   309 | `	if( pEntry ){` |
|       31 |   310 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   311 | `		/* Link entry with the same name */` |
|       31 |   312 | `		pClass->pNextName = pLink;` |
|       31 |   313 | `		pEntry->pUserData = pClass;` |
|       31 |   314 | `		return SXRET_OK;` |
|        - |   315 | `	}` |
|    21240 |   316 | `	pClass->pNextName = 0;` |
|        - |   317 | `	/* Perform a simple hashtable insertion */` |
|    21240 |   318 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    21240 |   319 | `	return rc;` |
|    10636 |   320 |  |
|        - |   321 | `/*` |
|        - |   322 | ` * Instruction builder interface.` |
|        - |   323 | ` */` |
|  2017712 |   324 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2017714 |   336 | `	sInstr.iOp = (sxu8)iOp;` |
|  2017714 |   337 | `	sInstr.iP1 = iP1;` |
|  2017714 |   338 | `	sInstr.iP2 = iP2;` |
|  2017714 |   339 | `	sInstr.p3  = p3;` |
|  2017714 |   340 | `	if( pIndex ){` |
|        - |   341 | `		/* Instruction index in the bytecode array */` |
|   122832 |   342 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    61415 |   343 | `	}` |
|        - |   344 | `	/* Finally,record the instruction */` |
|  2017714 |   345 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2017714 |   346 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   347 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   348 | `		/* Fall throw */` |
|      ! 0 |   349 | `	}` |
|  2017714 |   350 | `	return rc;` |
|        2 |   351 |  |
|        - |   352 | `/*` |
|        - |   353 | ` * Swap the current bytecode container with the given one.` |
|        - |   354 | ` */` |
|   196656 |   355 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   356 |  |
|   196658 |   357 | `	if( pContainer == 0 ){` |
|        - |   358 | `		/* Point to the default container */` |
|      ! 0 |   359 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   360 | `	}else{` |
|        - |   361 | `		/* Change container */` |
|   196658 |   362 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   363 | `	}` |
|   196658 |   364 | `	return SXRET_OK;` |
|        2 |   365 |  |
|        - |   366 | `/*` |
|        - |   367 | ` * Return the current bytecode container.` |
|        - |   368 | ` */` |
|    98328 |   369 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   370 |  |
|    98330 |   371 | `	return pVm->pByteContainer;` |
|        2 |   372 |  |
|        - |   373 | `/*` |
|        - |   374 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   375 | ` */` |
|   120844 |   376 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   377 |  |
|        - |   378 | `	VmInstr *pInstr;` |
|   120846 |   379 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   120846 |   380 | `	return pInstr;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   384 | ` */` |
|   586254 |   385 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   386 |  |
|   586256 |   387 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   388 |  |
|        - |   389 | `/*` |
|        - |   390 | ` * Pop the last VM instruction.` |
|        - |   391 | ` */` |
|   117524 |   392 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   393 |  |
|   117526 |   394 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Peek the last VM instruction.` |
|        - |   398 | ` */` |
|   313146 |   399 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   400 |  |
|   313148 |   401 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   402 |  |
|     7866 |   403 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   404 |  |
|        - |   405 | `	VmInstr *aInstr;` |
|        - |   406 | `	sxu32 n;` |
|     7868 |   407 | `	n = SySetUsed(pVm->pByteContainer);` |
|     7868 |   408 | `	if( n < 2 ){` |
|      ! 0 |   409 | `		return 0;` |
|        - |   410 | `	}` |
|     7868 |   411 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     7868 |   412 | `	return &aInstr[n - 2];` |
|     3935 |   413 |  |
|        - |   414 | `/*` |
|        - |   415 | ` * Allocate a new virtual machine frame.` |
|        - |   416 | ` */` |
|    12606 |   417 | `static VmFrame * VmNewFrame(` |
|        - |   418 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   419 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   420 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   421 | `	)` |
|        2 |   422 |  |
|        - |   423 | `	VmFrame *pFrame;` |
|        - |   424 | `	/* Allocate a new vm frame */` |
|    12608 |   425 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    12608 |   426 | `	if( pFrame == 0 ){` |
|      ! 0 |   427 | `		return 0;` |
|        - |   428 | `	}` |
|        - |   429 | `	/* Zero the structure */` |
|    12608 |   430 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   431 | `	/* Initialize frame fields */` |
|    12608 |   432 | `	pFrame->pUserData = pUserData;` |
|    12608 |   433 | `	pFrame->pThis = pThis;` |
|    12608 |   434 | `	pFrame->pVm = pVm;` |
|    12608 |   435 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    12608 |   436 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   437 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   438 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    12608 |   439 | `	return pFrame;` |
|     6305 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Enter a VM frame.` |
|        - |   443 | ` */` |
|    12606 |   444 | `static sxi32 VmEnterFrame(` |
|        - |   445 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   446 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   447 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   448 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   449 | `	)` |
|        2 |   450 |  |
|        - |   451 | `	VmFrame *pFrame;` |
|        - |   452 | `	/* Allocate a new frame */` |
|    12608 |   453 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    12608 |   454 | `	if( pFrame == 0 ){` |
|      ! 0 |   455 | `		return SXERR_MEM;` |
|        - |   456 | `	}` |
|        - |   457 | `	/* Link to the list of active VM frame */` |
|    12608 |   458 | `	pFrame->pParent = pVm->pFrame;` |
|    12608 |   459 | `	pVm->pFrame = pFrame;` |
|    12608 |   460 | `	if( ppFrame ){` |
|        - |   461 | `		/* Write a pointer to the new VM frame */` |
|    10698 |   462 | `		*ppFrame = pFrame;` |
|     5348 |   463 | `	}` |
|    12608 |   464 | `	return SXRET_OK;` |
|     6305 |   465 |  |
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
|    10694 |   512 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   513 |  |
|    10696 |   514 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    10696 |   515 | `	if( pCurFrame ){` |
|        - |   516 | `		/* Unlink from the list of active VM frame */` |
|    10696 |   517 | `		pVm->pFrame = pCurFrame->pParent;` |
|    10696 |   518 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   519 | `			VmSlot  *aSlot;` |
|        - |   520 | `			sxu32 n;` |
|        - |   521 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    10678 |   522 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    77702 |   523 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   524 | `				/* Unset the local variable */` |
|    67026 |   525 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    33514 |   526 | `			}` |
|        - |   527 | `			/* Remove local reference */` |
|    10678 |   528 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    77754 |   529 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    67078 |   530 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    33540 |   531 | `			}` |
|     5338 |   532 | `		}` |
|        - |   533 | `		/* Release internal containers */` |
|    10696 |   534 | `		SyHashRelease(&pCurFrame->hVar);` |
|    10696 |   535 | `		SySetRelease(&pCurFrame->sArg);` |
|    10696 |   536 | `		SySetRelease(&pCurFrame->sLocal);` |
|    10696 |   537 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   538 | `		/* Release the whole structure */` |
|    10696 |   539 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5347 |   540 | `	}` |
|    10696 |   541 |  |
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
|    71332 |   658 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   659 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   660 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   661 | `	)` |
|        2 |   662 |  |
|        - |   663 | `	ph7_class_method *pMeth;` |
|        - |   664 | `	ph7_class_attr *pAttr;` |
|        - |   665 | `	SyHashEntry *pEntry;` |
|        - |   666 | `	sxi32 rc;` |
|        - |   667 | `	/* Reset the loop cursor */` |
|    71334 |   668 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   669 | `	/* Process only static and constant attribute */` |
|   251756 |   670 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   671 | `		/* Extract the current attribute */` |
|   144758 |   672 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   144758 |   673 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    71334 |   695 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   696 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   697 | `		 */` |
|    42444 |   698 | `		return SXRET_OK;` |
|        - |   699 | `	}` |
|        - |   700 | `	/* Create constructor alias if not yet done */` |
|    28892 |   701 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   702 | `		/* User constructor with the same base class name */` |
|      206 |   703 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      206 |   704 | `		if( pEntry ){` |
|      ! 0 |   705 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   706 | `			/* Create the alias */` |
|      ! 0 |   707 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   708 | `		}` |
|      102 |   709 | `	}` |
|        - |   710 | `	/* Install the methods now */` |
|    28892 |   711 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   276407 |   712 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   233072 |   713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   233072 |   714 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   233066 |   715 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   233066 |   716 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   717 | `				return rc;` |
|        - |   718 | `			}` |
|   116532 |   719 | `		}` |
|        2 |   720 | `	}` |
|        - |   721 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    28892 |   722 | `	pClass->bMounted = TRUE;` |
|    28892 |   723 | `	return SXRET_OK;` |
|    35668 |   724 |  |
|        - |   725 | `/*` |
|        - |   726 | ` * Allocate a private frame for attributes of the given` |
|        - |   727 | ` * class instance (Object in the PHP jargon).` |
|        - |   728 | ` */` |
|      916 |   729 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   731 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   732 | `	)` |
|        2 |   733 |  |
|      918 |   734 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   735 | `	ph7_class_attr *pAttr;` |
|        - |   736 | `	SyHashEntry *pEntry;` |
|        - |   737 | `	sxi32 rc;` |
|        - |   738 | `	/* Install class attribute in the private frame associated with this instance */` |
|      918 |   739 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     3704 |   740 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   741 | `		VmClassAttr *pVmAttr;` |
|        - |   742 | `		/* Extract the current attribute */` |
|     2788 |   743 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     2788 |   744 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     2788 |   745 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   746 | `			return SXERR_MEM;` |
|        - |   747 | `		}` |
|     2788 |   748 | `		pVmAttr->pAttr = pAttr;` |
|     2788 |   749 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   750 | `			ph7_value *pMemObj;` |
|        - |   751 | `			/* Reserve a memory object for this attribute */` |
|     2782 |   752 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     2782 |   753 | `			if( pMemObj == 0 ){` |
|      ! 0 |   754 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   755 | `				return SXERR_MEM;` |
|        - |   756 | `			}` |
|     2782 |   757 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     2782 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|      904 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      451 |   761 | `			}` |
|     2782 |   762 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     2782 |   763 | `			if( rc != SXRET_OK ){` |
|        - |   764 | `				VmSlot sSlot;` |
|        - |   765 | `				/* Restore memory object */` |
|      ! 0 |   766 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   767 | `				sSlot.pUserData = 0;` |
|      ! 0 |   768 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   769 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   770 | `				return SXERR_MEM;` |
|        - |   771 | `			}` |
|        - |   772 | `			/* Install attribute in the reference table */` |
|     2782 |   773 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1392 |   774 | `		}else{` |
|        - |   775 | `			/* Install static/constant attribute */` |
|        8 |   776 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   777 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   778 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   779 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   780 | `				return SXERR_MEM;` |
|        - |   781 | `			}` |
|        - |   782 | `		}` |
|        2 |   783 | `	}` |
|      918 |   784 | `	return SXRET_OK;` |
|      460 |   785 |  |
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
|   233306 |   797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   798 |  |
|        - |   799 | `	ph7_value *pObj;` |
|        - |   800 | `	sxi32 rc;` |
|   233308 |   801 | `	if( pIndex ){` |
|        - |   802 | `		/* Object index in the object table */` |
|   227578 |   803 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   113788 |   804 | `	}` |
|        - |   805 | `	/* Reserve a slot for the new object */` |
|   233308 |   806 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   233308 |   807 | `	if( rc != SXRET_OK ){` |
|        - |   808 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   809 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   810 | `		 */` |
|      ! 0 |   811 | `		return 0;` |
|        - |   812 | `	}` |
|   233308 |   813 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   233308 |   814 | `	return pObj;` |
|   116655 |   815 |  |
|        - |   816 | `/*` |
|        - |   817 | ` * Reserve a memory object.` |
|        - |   818 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   819 | ` */` |
|  2129624 |   820 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   821 |  |
|        - |   822 | `	ph7_value *pObj;` |
|        - |   823 | `	sxi32 rc;` |
|  2129626 |   824 | `	if( pIndex ){` |
|        - |   825 | `		/* Object index in the object table */` |
|  2129626 |   826 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1064812 |   827 | `	}` |
|        - |   828 | `	/* Reserve a slot for the new object */` |
|  2129626 |   829 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2129626 |   830 | `	if( rc != SXRET_OK ){` |
|        - |   831 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   832 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   833 | `		 */` |
|      ! 0 |   834 | `		return 0;` |
|        - |   835 | `	}` |
|  2129626 |   836 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2129626 |   837 | `	return pObj;` |
|  1064814 |   838 |  |
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
|        - |   894 | `	"class ErrorException extends Exception { "\` |
|        - |   895 | `	"protected $severity;"\` |
|        - |   896 | `	"public function __construct(string $message = null,"\` |
|        - |   897 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   898 | `	"   if( isset($message) ){"\` |
|        - |   899 | `	"	  $this->message = $message;"\` |
|        - |   900 | `	"   }"\` |
|        - |   901 | `	"   $this->severity = $severity;"\` |
|        - |   902 | `	"   $this->code = $code;"\` |
|        - |   903 | `	"   $this->file = $filename;"\` |
|        - |   904 | `	"   $this->line = $lineno;"\` |
|        - |   905 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   906 | `	"   if( isset($previous) ){"\` |
|        - |   907 | `	"     $this->previous = $previous;"\` |
|        - |   908 | `	"   }"\` |
|        - |   909 | `	"}"\` |
|        - |   910 | `	"public function getSeverity(){"\` |
|        - |   911 | `	"   return $this->severity;"\` |
|        - |   912 | `    "}"\` |
|        - |   913 | `	"}"\` |
|        - |   914 | `	"interface Iterator {"\` |
|        - |   915 | `	"public function current();"\` |
|        - |   916 | `	"public function key();"\` |
|        - |   917 | `	"public function next();"\` |
|        - |   918 | `	"public function rewind();"\` |
|        - |   919 | `	"public function valid();"\` |
|        - |   920 | `	"}"\` |
|        - |   921 | `	"interface IteratorAggregate {"\` |
|        - |   922 | `	"public function getIterator();"\` |
|        - |   923 | `	"}"\` |
|        - |   924 | `	"interface Serializable {"\` |
|        - |   925 | `	"public function serialize();"\` |
|        - |   926 | `	"public function unserialize(string $serialized);"\` |
|        - |   927 | `	"}"\` |
|        - |   928 | `	"/* Directory releated IO */"\` |
|        - |   929 | `	"class Directory {"\` |
|        - |   930 | `	"public $handle = null;"\` |
|        - |   931 | `	"public $path  = null;"\` |
|        - |   932 | `	"public function __construct(string $path)"\` |
|        - |   933 | `	"{"\` |
|        - |   934 | `	"   $this->handle = opendir($path);"\` |
|        - |   935 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   936 | `	"      $this->path = $path;"\` |
|        - |   937 | `	"   }"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function __destruct()"\` |
|        - |   940 | `	"{"\` |
|        - |   941 | `	"  if( $this->handle != null ){"\` |
|        - |   942 | `	"       closedir($this->handle);"\` |
|        - |   943 | `	"  }"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"public function read()"\` |
|        - |   946 | `	"{"\` |
|        - |   947 | `	"    return readdir($this->handle);"\` |
|        - |   948 | `	"}"\` |
|        - |   949 | `	"public function rewind()"\` |
|        - |   950 | `	"{"\` |
|        - |   951 | `	"    rewinddir($this->handle);"\` |
|        - |   952 | `	"}"\` |
|        - |   953 | `	"public function close()"\` |
|        - |   954 | `	"{"\` |
|        - |   955 | `	"    closedir($this->handle);"\` |
|        - |   956 | `	"    $this->handle = null;"\` |
|        - |   957 | `	"}"\` |
|        - |   958 | `	"}"\` |
|        - |   959 | `	"class stdClass{"\` |
|        - |   960 | `	"  public $value;"\` |
|        - |   961 | `	" /* Magic methods */"\` |
|        - |   962 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   963 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   964 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   965 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   966 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   967 | `	"}"\` |
|        - |   968 | `	"function dir(string $path){"\` |
|        - |   969 | `	"   return new Directory($path);"\` |
|        - |   970 | `	"}"\` |
|        - |   971 | `	"function Dir(string $path){"\` |
|        - |   972 | `	"   return new Directory($path);"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   975 | `    "{"\` |
|        - |   976 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   977 | `	"  $aDir = array();"\` |
|        - |   978 | `	"  $pHandle = opendir($directory);"\` |
|        - |   979 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   980 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   981 | `	"      $aDir[] = $pEntry;"\` |
|        - |   982 | `	"   }"\` |
|        - |   983 | `	"  closedir($pHandle);"\` |
|        - |   984 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |   985 | `	"      rsort($aDir);"\` |
|        - |   986 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |   987 | `	"      sort($aDir);"\` |
|        - |   988 | `	"  }"\` |
|        - |   989 | `	"  return $aDir;"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |   992 | `	"/* Open the target directory */"\` |
|        - |   993 | `	"$zDir = dirname($pattern);"\` |
|        - |   994 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |   995 | `	"$pHandle = opendir($zDir);"\` |
|        - |   996 | `	"if( $pHandle == FALSE ){"\` |
|        - |   997 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |   998 | `	"	return FALSE;"\` |
|        - |   999 | `	"}"\` |
|        - |  1000 | `	"$pattern = basename($pattern);"\` |
|        - |  1001 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1002 | `	"/* Loop throw available entries */"\` |
|        - |  1003 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1004 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1005 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1006 | `	"	if( $rc ){"\` |
|        - |  1007 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1008 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1009 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1010 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1011 | `	"		  }"\` |
|        - |  1012 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1013 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1014 | `	"		 continue;"\` |
|        - |  1015 | `	"	   }"\` |
|        - |  1016 | `	"	   /* Add the entry */"\` |
|        - |  1017 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1018 | `	"	}"\` |
|        - |  1019 | `	" }"\` |
|        - |  1020 | `	"/* Close the handle */"\` |
|        - |  1021 | `	"closedir($pHandle);"\` |
|        - |  1022 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1023 | `	"  /* Sort the array */"\` |
|        - |  1024 | `	"  sort($pArray);"\` |
|        - |  1025 | `	"}"\` |
|        - |  1026 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1027 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1028 | `	"  $pArray[] = $pattern;"\` |
|        - |  1029 | `	"}"\` |
|        - |  1030 | `	"/* Return the created array */"\` |
|        - |  1031 | `	"return $pArray;"\` |
|        - |  1032 | `   "}"\` |
|        - |  1033 | `   "/* Creates a temporary file */"\` |
|        - |  1034 | `   "function tmpfile(){"\` |
|        - |  1035 | `   "  /* Extract the temp directory */"\` |
|        - |  1036 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1037 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1038 | `   "    /* Use the current dir */"\` |
|        - |  1039 | `   "    $zTempDir = '.';"\` |
|        - |  1040 | `   "  }"\` |
|        - |  1041 | `   "  /* Create the file */"\` |
|        - |  1042 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1043 | `   "  return $pHandle;"\` |
|        - |  1044 | `   "}"\` |
|        - |  1045 | `   "/* Creates a temporary filename */"\` |
|        - |  1046 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1047 | `   "{"\` |
|        - |  1048 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1049 | `   "}"\` |
|        - |  1050 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1051 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1052 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1053 | `   "/* Copy arguments */"\` |
|        - |  1054 | `   "$nArgs = func_num_args();"\` |
|        - |  1055 | `   "$pNew = array();"\` |
|        - |  1056 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1057 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1058 | `    "}"\` |
|        - |  1059 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1060 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1061 | `	"/* Erase */"\` |
|        - |  1062 | `	"array_erase($pArray);"\` |
|        - |  1063 | `	"/* Unshift */"\` |
|        - |  1064 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1065 | `	"return sizeof($pArray);"\` |
|        - |  1066 | `    "}"\` |
|        - |  1067 | `	"function array_merge_recursive($array1, $array2){"\` |
|        - |  1068 | `	"if( func_num_args() < 1 ){ return NULL; }"\` |
|        - |  1069 | `    "$arrays = func_get_args();"\` |
|        - |  1070 | `    "$narrays = count($arrays);"\` |
|        - |  1071 | `    "$ret = $arrays[0];"\` |
|        - |  1072 | `    "for ($i = 1; $i < $narrays; $i++) {"\` |
|        - |  1073 | `	 " if( array_same($ret,$arrays[$i]) ){ /* Same instance */continue;}"\` |
|        - |  1074 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1075 | `     "  if (((string) $key) === ((string) intval($key))) {"\` |
|        - |  1076 | `     "   $ret[] = $value;"\` |
|        - |  1077 | `     "  }else{"\` |
|        - |  1078 | `     "  if (is_array($value) && isset($ret[$key]) ) {"\` |
|        - |  1079 | `     "   $ret[$key] = array_merge_recursive($ret[$key], $value);"\` |
|        - |  1080 | `     " }else {"\` |
|        - |  1081 | `     "   $ret[$key] = $value;"\` |
|        - |  1082 | `     "  }"\` |
|        - |  1083 | `     " }"\` |
|        - |  1084 | `     " }"\` |
|        - |  1085 | `	 "}"\` |
|        - |  1086 | `	 " return $ret;"\` |
|        - |  1087 | `    "}"\` |
|        - |  1088 | `	"function max(){"\` |
|        - |  1089 | `    "  $pArgs = func_get_args();"\` |
|        - |  1090 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1091 | `	"  return null;"\` |
|        - |  1092 | `    " }"\` |
|        - |  1093 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1094 | `    " $pArg = $pArgs[0];"\` |
|        - |  1095 | `	" if( !is_array($pArg) ){"\` |
|        - |  1096 | `	"   return $pArg; "\` |
|        - |  1097 | `	" }"\` |
|        - |  1098 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1099 | `	"   return null;"\` |
|        - |  1100 | `	" }"\` |
|        - |  1101 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1102 | `	" reset($pArg);"\` |
|        - |  1103 | `	" $max = current($pArg);"\` |
|        - |  1104 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1105 | `	"   if( $val > $max ){"\` |
|        - |  1106 | `	"     $max = $val;"\` |
|        - |  1107 | `    " }"\` |
|        - |  1108 | `	" }"\` |
|        - |  1109 | `	" return $max;"\` |
|        - |  1110 | `    " }"\` |
|        - |  1111 | `    " $max = $pArgs[0];"\` |
|        - |  1112 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1113 | `    " $val = $pArgs[$i];"\` |
|        - |  1114 | `	"if( $val > $max ){"\` |
|        - |  1115 | `	" $max = $val;"\` |
|        - |  1116 | `	"}"\` |
|        - |  1117 | `    " }"\` |
|        - |  1118 | `	" return $max;"\` |
|        - |  1119 | `    "}"\` |
|        - |  1120 | `	"function min(){"\` |
|        - |  1121 | `    "  $pArgs = func_get_args();"\` |
|        - |  1122 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1123 | `	"  return null;"\` |
|        - |  1124 | `    " }"\` |
|        - |  1125 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1126 | `    " $pArg = $pArgs[0];"\` |
|        - |  1127 | `	" if( !is_array($pArg) ){"\` |
|        - |  1128 | `	"   return $pArg; "\` |
|        - |  1129 | `	" }"\` |
|        - |  1130 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1131 | `	"   return null;"\` |
|        - |  1132 | `	" }"\` |
|        - |  1133 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1134 | `	" reset($pArg);"\` |
|        - |  1135 | `	" $min = current($pArg);"\` |
|        - |  1136 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1137 | `	"   if( $val < $min ){"\` |
|        - |  1138 | `	"     $min = $val;"\` |
|        - |  1139 | `    " }"\` |
|        - |  1140 | `	" }"\` |
|        - |  1141 | `	" return $min;"\` |
|        - |  1142 | `    " }"\` |
|        - |  1143 | `    " $min = $pArgs[0];"\` |
|        - |  1144 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1145 | `    " $val = $pArgs[$i];"\` |
|        - |  1146 | `	"if( $val < $min ){"\` |
|        - |  1147 | `	" $min = $val;"\` |
|        - |  1148 | `	" }"\` |
|        - |  1149 | `    " }"\` |
|        - |  1150 | `	" return $min;"\` |
|        - |  1151 | `	"}"\` |
|        - |  1152 | `	"function fileowner(string $file){"\` |
|        - |  1153 | `    " $a = stat($file);"\` |
|        - |  1154 | `	" if( !is_array($a) ){"\` |
|        - |  1155 | `	"	return false;"\` |
|        - |  1156 | `	" }"\` |
|        - |  1157 | `	" return $a['uid'];"\` |
|        - |  1158 | `    "}"\` |
|        - |  1159 | `    "function filegroup(string $file){"\` |
|        - |  1160 | `	" $a = stat($file);"\` |
|        - |  1161 | `	" if( !is_array($a) ){"\` |
|        - |  1162 | `	"	return false;"\` |
|        - |  1163 | `	" }"\` |
|        - |  1164 | `	" return $a['gid'];"\` |
|        - |  1165 | `    "}"\` |
|        - |  1166 | `	 "function fileinode(string $file){"\` |
|        - |  1167 | `	" $a = stat($file);"\` |
|        - |  1168 | `	" if( !is_array($a) ){"\` |
|        - |  1169 | `	"	return false;"\` |
|        - |  1170 | `	" }"\` |
|        - |  1171 | `	" return $a['ino'];"\` |
|        - |  1172 | `    "}"` |
|        - |  1173 |  |
|        - |  1174 | `/*` |
|        - |  1175 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1176 | ` * start compiling the target PHP program.` |
|        - |  1177 | ` */` |
|     1910 |  1178 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1179 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1180 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1181 | `	 )` |
|        2 |  1182 |  |
|        - |  1183 | `	SyString sBuiltin;` |
|        - |  1184 | `	ph7_value *pObj;` |
|        - |  1185 | `	sxi32 rc;` |
|        - |  1186 | `	/* Zero the structure */` |
|     1912 |  1187 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1188 | `	/* Initialize VM fields */` |
|     1912 |  1189 | `	pVm->pEngine = &(*pEngine);` |
|     1912 |  1190 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1191 | `	/* Instructions containers */` |
|     1912 |  1192 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     1912 |  1193 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     1912 |  1194 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1195 | `	/* Object containers */` |
|     1912 |  1196 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1912 |  1197 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1198 | `	/* Virtual machine internal containers */` |
|     1912 |  1199 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     1912 |  1200 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     1912 |  1201 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     1912 |  1202 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     1912 |  1203 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     1912 |  1204 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     1912 |  1205 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     1912 |  1206 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     1912 |  1207 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     1912 |  1208 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     1912 |  1209 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     1912 |  1210 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     1912 |  1211 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     1912 |  1212 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     1912 |  1213 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1214 | `	/* Configuration containers */` |
|     1912 |  1215 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1216 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1217 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     1912 |  1218 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     1912 |  1219 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1220 | `	/* Error callbacks containers */` |
|     1912 |  1221 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     1912 |  1222 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     1912 |  1223 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     1912 |  1224 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     1912 |  1225 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1226 | `	/* Set a default recursion limit */` |
|        - |  1227 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     1912 |  1228 | `	pVm->nMaxDepth = 32;` |
|        - |  1229 | `#else` |
|        - |  1230 | `	pVm->nMaxDepth = 16;` |
|        - |  1231 | `#endif` |
|        - |  1232 | `	/* Default assertion flags */` |
|     1912 |  1233 | `	pVm->iAssertFlags = PH7_ASSERT_WARNING; /* Issue a warning for each failed assertion */` |
|        - |  1234 | `	/* JSON return status */` |
|     1912 |  1235 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1236 | `	/* PRNG context */` |
|     1912 |  1237 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1238 | `	/* Install the null constant */` |
|     1912 |  1239 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1240 | `	if( pObj == 0 ){` |
|      ! 0 |  1241 | `		rc = SXERR_MEM;` |
|      ! 0 |  1242 | `		goto Err;` |
|        - |  1243 | `	}` |
|     1912 |  1244 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1245 | `	/* Install the boolean TRUE constant */` |
|     1912 |  1246 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1247 | `	if( pObj == 0 ){` |
|      ! 0 |  1248 | `		rc = SXERR_MEM;` |
|      ! 0 |  1249 | `		goto Err;` |
|        - |  1250 | `	}` |
|     1912 |  1251 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1252 | `	/* Install the boolean FALSE constant */` |
|     1912 |  1253 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     1912 |  1254 | `	if( pObj == 0 ){` |
|      ! 0 |  1255 | `		rc = SXERR_MEM;` |
|      ! 0 |  1256 | `		goto Err;` |
|        - |  1257 | `	}` |
|     1912 |  1258 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1259 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1260 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1261 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     1912 |  1262 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     1912 |  1263 | `	if( pObj == 0 ){` |
|      ! 0 |  1264 | `		rc = SXERR_MEM;` |
|      ! 0 |  1265 | `		goto Err;` |
|        - |  1266 | `	}` |
|     1912 |  1267 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1268 | `	/* Create the global frame */` |
|     1912 |  1269 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     1912 |  1270 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1271 | `		goto Err;` |
|        - |  1272 | `	}` |
|        - |  1273 | `	/* Initialize the code generator */` |
|     1912 |  1274 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1912 |  1275 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1276 | `		goto Err;` |
|        - |  1277 | `	}` |
|        - |  1278 | `	/* VM correctly initialized,set the magic number */` |
|     1912 |  1279 | `	pVm->nMagic = PH7_VM_INIT;` |
|     1912 |  1280 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1281 | `	/* Compile the built-in library */` |
|     1912 |  1282 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1283 | `	/* Reset the code generator */` |
|     1912 |  1284 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     1912 |  1285 | `	return SXRET_OK;` |
|      ! 0 |  1286 | `Err:` |
|      ! 0 |  1287 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1288 | `	return rc;` |
|      957 |  1289 |  |
|        - |  1290 | `/*` |
|        - |  1291 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1292 | ` * routine which store the output in an internal blob.` |
|        - |  1293 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1294 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1295 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1296 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1297 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1298 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1299 | ` * to finish executing and extracting the output.` |
|        - |  1300 | ` */` |
|      ! 0 |  1301 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1302 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1303 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1304 | `	void *pUserData     /* User private data */` |
|        - |  1305 | `	)` |
|      ! 0 |  1306 |  |
|        - |  1307 | `	 sxi32 rc;` |
|        - |  1308 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1309 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1310 | `	 return rc;` |
|      ! 0 |  1311 |  |
|        - |  1312 | `#define VM_STACK_GUARD 16` |
|        - |  1313 | `/*` |
|        - |  1314 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1315 | ` * our compiled PHP program.` |
|        - |  1316 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1317 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1318 | ` */` |
|    26958 |  1319 | `static ph7_value * VmNewOperandStack(` |
|        - |  1320 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1321 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1322 | `	)` |
|        2 |  1323 |  |
|        - |  1324 | `	ph7_value *pStack;` |
|        - |  1325 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1326 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1327 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1328 | `  ** on the maximum stack depth required.` |
|        - |  1329 | `  **` |
|        - |  1330 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1331 | `  */` |
|    26960 |  1332 | `	nInstr += VM_STACK_GUARD;` |
|    26960 |  1333 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    26960 |  1334 | `	if( pStack == 0 ){` |
|      ! 0 |  1335 | `		return 0;` |
|        - |  1336 | `	}` |
|        - |  1337 | `	/* Initialize the operand stack */` |
|  1720202 |  1338 | `	while( nInstr > 0 ){` |
|  1693244 |  1339 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1693244 |  1340 | `		--nInstr;` |
|        2 |  1341 | `	}` |
|        - |  1342 | `	/* Ready for bytecode execution */` |
|    26960 |  1343 | `	return pStack;` |
|    13481 |  1344 |  |
|        - |  1345 | `/* Forward declaration */` |
|        - |  1346 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1347 | `/*` |
|        - |  1348 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1349 | ` * This routine gets called by the PH7 engine after` |
|        - |  1350 | ` * successful compilation of the target PHP program.` |
|        - |  1351 | ` */` |
|     1672 |  1352 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1353 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1354 | `	)` |
|        2 |  1355 |  |
|        - |  1356 | `	SyHashEntry *pEntry;` |
|        - |  1357 | `	sxi32 rc;` |
|     1674 |  1358 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1359 | `		/* Initialize your VM first */` |
|      ! 0 |  1360 | `		return SXERR_CORRUPT;` |
|        - |  1361 | `	}` |
|        - |  1362 | `	/* Mark the VM ready for byte-code execution */` |
|     1674 |  1363 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1364 | `	/* Release the code generator now we have compiled our program */` |
|     1674 |  1365 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1366 | `	/* Emit the DONE instruction */` |
|     1674 |  1367 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     1674 |  1368 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1369 | `		return SXERR_MEM;` |
|        - |  1370 | `	}` |
|        - |  1371 | `	/* Script return value */` |
|     1674 |  1372 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1373 | `	/* Allocate a new operand stack */` |
|     1674 |  1374 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     1674 |  1375 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1376 | `		return SXERR_MEM;` |
|        - |  1377 | `	}` |
|        - |  1378 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1379 | `	 * private data. */` |
|     1674 |  1380 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     1674 |  1381 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1382 | `	/* Allocate the reference table */` |
|     1674 |  1383 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     1674 |  1384 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     1674 |  1385 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1386 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1387 | `		return SXERR_MEM;` |
|        - |  1388 | `	}` |
|        - |  1389 | `	/* Zero the reference table */` |
|     1674 |  1390 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1391 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     1674 |  1392 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     1674 |  1393 | `	if( rc != SXRET_OK ){` |
|        - |  1394 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1395 | `		return rc;` |
|        - |  1396 | `	}` |
|        - |  1397 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     1674 |  1398 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     1674 |  1399 | `	if( rc != SXRET_OK ){` |
|        - |  1400 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1401 | `		return rc;` |
|        - |  1402 | `	}` |
|        - |  1403 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     1674 |  1404 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1405 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     1674 |  1406 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1407 | `	/* Initialize and install static and constants class attributes */` |
|     1674 |  1408 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    20096 |  1409 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    18424 |  1410 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    18424 |  1411 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1412 | `			return rc;` |
|        - |  1413 | `		}` |
|        2 |  1414 | `	}` |
|        - |  1415 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     1674 |  1416 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1417 | `	/* VM is ready for bytecode execution */` |
|     1674 |  1418 | `	return SXRET_OK;` |
|      838 |  1419 |  |
|        - |  1420 | `/*` |
|        - |  1421 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1422 | ` */` |
|      ! 0 |  1423 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1424 |  |
|      ! 0 |  1425 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1426 | `		return SXERR_CORRUPT;` |
|        - |  1427 | `	}` |
|        - |  1428 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1429 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1430 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1431 | `	/* Set the ready flag */` |
|      ! 0 |  1432 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1433 | `	return SXRET_OK;` |
|      ! 0 |  1434 |  |
|        - |  1435 | `/*` |
|        - |  1436 | ` * Release a Virtual Machine.` |
|        - |  1437 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1438 | ` */` |
|     1664 |  1439 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1440 |  |
|        - |  1441 | `	/* Set the stale magic number */` |
|     1666 |  1442 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1443 | `	/* Release the private memory subsystem */` |
|     1666 |  1444 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     1666 |  1445 | `	return SXRET_OK;` |
|        2 |  1446 |  |
|        - |  1447 | `/*` |
|        - |  1448 | ` * Initialize a foreign function call context.` |
|        - |  1449 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1450 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1451 | ` * functions.` |
|        - |  1452 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1453 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1454 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1455 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1456 | ` */` |
|   527968 |  1457 | `static sxi32 VmInitCallContext(` |
|        - |  1458 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1459 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1460 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1461 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1462 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1463 | `	)` |
|        2 |  1464 |  |
|   527970 |  1465 | `	pOut->pFunc = pFunc;` |
|   527970 |  1466 | `	pOut->pVm   = pVm;` |
|   527970 |  1467 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   527970 |  1468 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1469 | `	/* Assume a null return value */` |
|   527970 |  1470 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   527970 |  1471 | `	pOut->pRet = pRet;` |
|   527970 |  1472 | `	pOut->iFlags = iFlags;` |
|   527970 |  1473 | `	return SXRET_OK;` |
|        2 |  1474 |  |
|        - |  1475 | `/*` |
|        - |  1476 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1477 | ` * left behind.` |
|        - |  1478 | ` */` |
|   527968 |  1479 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1480 |  |
|        - |  1481 | `	sxu32 n;` |
|   527970 |  1482 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6164 |  1483 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    17420 |  1484 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    11258 |  1485 | `			if( apObj[n] == 0 ){` |
|        - |  1486 | `				/* Already released */` |
|      250 |  1487 | `				continue;` |
|        - |  1488 | `			}` |
|    11010 |  1489 | `			PH7_MemObjRelease(apObj[n]);` |
|    11010 |  1490 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5506 |  1491 | `		}` |
|     6164 |  1492 | `		SySetRelease(&pCtx->sVar);` |
|     3081 |  1493 | `	}` |
|   527970 |  1494 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1495 | `		ph7_aux_data *aAux;` |
|        - |  1496 | `		void *pChunk;` |
|        - |  1497 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1498 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1499 | `		 */` |
|        9 |  1500 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1501 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1502 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1503 | `			/* Release the chunk */` |
|       25 |  1504 | `			if( pChunk ){` |
|       25 |  1505 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1506 | `			}` |
|       13 |  1507 | `		}` |
|        9 |  1508 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1509 | `	}` |
|   527970 |  1510 |  |
|        - |  1511 | `/*` |
|        - |  1512 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1513 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1514 | ` */` |
|      248 |  1515 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1516 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1517 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1518 | `	)` |
|        2 |  1519 |  |
|      250 |  1520 | `	if( pValue == 0 ){` |
|        - |  1521 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1522 | `		return;` |
|        - |  1523 | `	}` |
|      250 |  1524 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1525 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1526 | `		sxu32 n;` |
|      936 |  1527 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1528 | `			if( apObj[n] == pValue ){` |
|      250 |  1529 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1530 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1531 | `				/* Mark as released */` |
|      250 |  1532 | `				apObj[n] = 0;` |
|      250 |  1533 | `				break;` |
|        - |  1534 | `			}` |
|      345 |  1535 | `		}` |
|      124 |  1536 | `	}` |
|      126 |  1537 |  |
|        - |  1538 | `/*` |
|        - |  1539 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1540 | ` */` |
|  3141328 |  1541 | `static void VmPopOperand(` |
|        - |  1542 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1543 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1544 | `	)` |
|        2 |  1545 |  |
|  3141330 |  1546 | `	ph7_value *pTos = *ppTos;` |
|  6654662 |  1547 | `	while( nPop > 0 ){` |
|  3513334 |  1548 | `		PH7_MemObjRelease(pTos);` |
|  3513334 |  1549 | `		pTos--;` |
|  3513334 |  1550 | `		nPop--;` |
|        2 |  1551 | `	}` |
|        - |  1552 | `	/* Top of the stack */` |
|  3141330 |  1553 | `	*ppTos = pTos;` |
|  3141330 |  1554 |  |
|        - |  1555 | `/*` |
|        - |  1556 | ` * Reserve a memory object.` |
|        - |  1557 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1558 | ` */` |
|  2940358 |  1559 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1560 |  |
|  2940360 |  1561 | `	ph7_value *pObj = 0;` |
|        - |  1562 | `	VmSlot *pSlot;` |
|        - |  1563 | `	sxu32 nIdx;` |
|        - |  1564 | `	/* Check for a free slot */` |
|  2940360 |  1565 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2940360 |  1566 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2940360 |  1567 | `	if( pSlot ){` |
|   810736 |  1568 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   810736 |  1569 | `		nIdx = pSlot->nIdx;` |
|   405367 |  1570 | `	}` |
|  2940360 |  1571 | `	if( pObj == 0 ){` |
|        - |  1572 | `		/* Reserve a new memory object */` |
|  2129626 |  1573 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2129626 |  1574 | `		if( pObj == 0 ){` |
|      ! 0 |  1575 | `			return 0;` |
|        - |  1576 | `		}` |
|  1064812 |  1577 | `	}` |
|        - |  1578 | `	/* Set a null default value */` |
|  2940360 |  1579 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2940360 |  1580 | `	pObj->nIdx = nIdx;` |
|  2940360 |  1581 | `	return pObj;` |
|  1470181 |  1582 |  |
|        - |  1583 | `/*` |
|        - |  1584 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1585 | ` */` |
|    22116 |  1586 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1587 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1588 | `	const char *zKey,  /* Entry key */` |
|        - |  1589 | `	sxu32 nByte,       /* Key length */` |
|        - |  1590 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1591 | `	)` |
|        2 |  1592 |  |
|        - |  1593 | `	ph7_value sKey;` |
|        - |  1594 | `	sxi32 rc;` |
|    22118 |  1595 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    22118 |  1596 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1597 | `	/* Perform the insertion */` |
|    22118 |  1598 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    22118 |  1599 | `	PH7_MemObjRelease(&sKey);` |
|    22118 |  1600 | `	return rc;` |
|        2 |  1601 |  |
|        - |  1602 | `/*` |
|        - |  1603 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1604 | ` * Return a pointer to the variable value on success.` |
|        - |  1605 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1606 | ` */` |
|  2957242 |  1607 | `static ph7_value * VmExtractMemObj(` |
|        - |  1608 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1609 | `	const SyString *pName, /* Variable name */` |
|        - |  1610 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1611 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1612 | `	)` |
|        2 |  1613 |  |
|  2957244 |  1614 | `	int bNullify = FALSE;` |
|        - |  1615 | `	SyHashEntry *pEntry;` |
|        - |  1616 | `	VmFrame *pFrame;` |
|        - |  1617 | `	ph7_value *pObj;` |
|        - |  1618 | `	sxu32 nIdx;` |
|        - |  1619 | `	sxi32 rc;` |
|        - |  1620 | `	/* Point to the top active frame */` |
|  2957244 |  1621 | `	pFrame = pVm->pFrame;` |
|  3006596 |  1622 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1623 | `		/* Safely ignore the exception frame */` |
|    49353 |  1624 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1625 | `	}` |
|        - |  1626 | `	/* Perform the lookup */` |
|  2957244 |  1627 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1628 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1629 | `		pName = &sAnnon;` |
|        - |  1630 | `		/* Always nullify the object */` |
|      ! 0 |  1631 | `		bNullify = TRUE;` |
|      ! 0 |  1632 | `		bDup = FALSE;` |
|      ! 0 |  1633 | `	}` |
|        - |  1634 | `	/* Check the superglobals table first */` |
|  2957244 |  1635 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2957244 |  1636 | `	if( pEntry == 0 ){` |
|        - |  1637 | `		/* Query the top active frame */` |
|  2957208 |  1638 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2957208 |  1639 | `		if( pEntry == 0 ){` |
|    72972 |  1640 | `			char *zName = (char *)pName->zString;` |
|        - |  1641 | `			VmSlot sLocal;` |
|    72972 |  1642 | `			if( !bCreate ){` |
|        - |  1643 | `				/* Do not create the variable,return NULL instead */` |
|      576 |  1644 | `				return 0;` |
|        - |  1645 | `			}` |
|        - |  1646 | `			/* No such variable,automatically create a new one and install` |
|        - |  1647 | `			 * it in the current frame.` |
|        - |  1648 | `			 */` |
|    72398 |  1649 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    72398 |  1650 | `			if( pObj == 0 ){` |
|      ! 0 |  1651 | `				return 0;` |
|        - |  1652 | `			}` |
|    72398 |  1653 | `			nIdx = pObj->nIdx;` |
|    72398 |  1654 | `			if( bDup ){` |
|        - |  1655 | `				/* Duplicate name */` |
|      132 |  1656 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      132 |  1657 | `				if( zName == 0 ){` |
|      ! 0 |  1658 | `					return 0;` |
|        - |  1659 | `				}` |
|       65 |  1660 | `			}` |
|        - |  1661 | `			/* Link to the top active VM frame */` |
|    72398 |  1662 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    72398 |  1663 | `			if( rc != SXRET_OK ){` |
|        - |  1664 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1665 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1666 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1667 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1668 | `				return 0;` |
|        - |  1669 | `			}` |
|    72398 |  1670 | `			if( pFrame->pParent != 0 ){` |
|        - |  1671 | `				/* Local variable */` |
|    67026 |  1672 | `				sLocal.nIdx = nIdx;` |
|    67026 |  1673 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    33514 |  1674 | `			}else{` |
|        - |  1675 | `				/* Register in the $GLOBALS array */` |
|     5374 |  1676 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1677 | `			}` |
|        - |  1678 | `			/* Install in the reference table */` |
|    72398 |  1679 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1680 | `			/* Save object index */` |
|    72398 |  1681 | `			pObj->nIdx = nIdx;` |
|    36200 |  1682 | `		}else{` |
|        - |  1683 | `			/* Extract variable contents */` |
|  2884238 |  1684 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2884238 |  1685 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2884238 |  1686 | `			if( bNullify && pObj ){` |
|      ! 0 |  1687 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1688 | `			}` |
|        - |  1689 | `		}` |
|  1478428 |  1690 | `	}else{` |
|        - |  1691 | `		/* Superglobal */` |
|       38 |  1692 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1693 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1694 | `	}` |
|  2956670 |  1695 | `	return pObj;` |
|  1478733 |  1696 |  |
|        - |  1697 | `/*` |
|        - |  1698 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1699 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1700 | ` */` |
|     1698 |  1701 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1702 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1703 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1704 | `	sxu32 nByte        /* zName length */` |
|        - |  1705 | `	)` |
|        2 |  1706 |  |
|        - |  1707 | `	SyHashEntry *pEntry;` |
|        - |  1708 | `	ph7_value *pValue;` |
|        - |  1709 | `	sxu32 nIdx;` |
|        - |  1710 | `	/* Query the superglobal table */` |
|     1700 |  1711 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     1700 |  1712 | `	if( pEntry == 0 ){` |
|        - |  1713 | `		/* No such entry */` |
|      ! 0 |  1714 | `		return 0;` |
|        - |  1715 | `	}` |
|        - |  1716 | `	/* Extract the superglobal index in the global object pool */` |
|     1700 |  1717 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1718 | `	/* Extract the variable value  */` |
|     1700 |  1719 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     1700 |  1720 | `	return pValue;` |
|      851 |  1721 |  |
|        - |  1722 | `/*` |
|        - |  1723 | ` * Perform a raw hashmap insertion.` |
|        - |  1724 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1725 | ` */` |
|     1696 |  1726 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1727 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1728 | `	const char *zKey,   /* Entry key */` |
|        - |  1729 | `	int nKeylen,        /* zKey length*/` |
|        - |  1730 | `	const char *zData,  /* Entry data */` |
|        - |  1731 | `	int nLen            /* zData length */` |
|        - |  1732 | `	)` |
|        2 |  1733 |  |
|        - |  1734 | `	ph7_value sKey,sValue;` |
|        - |  1735 | `	sxi32 rc;` |
|     1698 |  1736 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     1698 |  1737 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     1698 |  1738 | `	if( zKey ){` |
|     1676 |  1739 | `		if( nKeylen < 0 ){` |
|     1676 |  1740 | `			nKeylen = (int)SyStrlen(zKey);` |
|      837 |  1741 | `		}` |
|     1676 |  1742 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|      837 |  1743 | `	}` |
|     1698 |  1744 | `	if( zData ){` |
|     1698 |  1745 | `		if( nLen < 0 ){` |
|        - |  1746 | `			/* Compute length automatically */` |
|      ! 0 |  1747 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1748 | `		}` |
|     1698 |  1749 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|      848 |  1750 | `	}` |
|        - |  1751 | `	/* Perform the insertion */` |
|     1698 |  1752 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     1698 |  1753 | `	PH7_MemObjRelease(&sKey);` |
|     1698 |  1754 | `	PH7_MemObjRelease(&sValue);` |
|     1698 |  1755 | `	return rc;` |
|        2 |  1756 |  |
|        - |  1757 | `/*` |
|        - |  1758 | ` * Configure a working virtual machine instance.` |
|        - |  1759 | ` *` |
|        - |  1760 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1761 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1762 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1763 | ` * The second argument to this function is an integer configuration option` |
|        - |  1764 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1765 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1766 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1767 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1768 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1769 | ` */` |
|    26776 |  1770 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1771 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1772 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1773 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1774 | `	)` |
|        2 |  1775 |  |
|    26778 |  1776 | `	sxi32 rc = SXRET_OK;` |
|    26778 |  1777 | `	switch(nOp){` |
|      836 |  1778 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     1674 |  1779 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     1674 |  1780 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1781 | `		/* VM output consumer callback */` |
|        - |  1782 | `#ifdef UNTRUST` |
|        - |  1783 | `		if( xConsumer == 0 ){` |
|        - |  1784 | `			rc = SXERR_CORRUPT;` |
|        - |  1785 | `			break;` |
|        - |  1786 | `		}` |
|        - |  1787 | `#endif` |
|        - |  1788 | `		/* Install the output consumer */` |
|     1674 |  1789 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     1674 |  1790 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     1674 |  1791 | `		break;` |
|        - |  1792 | `							   }` |
|      836 |  1793 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1794 | `		/* Import path */` |
|        - |  1795 | `		  const char *zPath;` |
|        - |  1796 | `		  SyString sPath;` |
|     1674 |  1797 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1798 | `#if defined(UNTRUST)` |
|        - |  1799 | `		  if( zPath == 0 ){` |
|        - |  1800 | `			  rc = SXERR_EMPTY;` |
|        - |  1801 | `			  break;` |
|        - |  1802 | `		  }` |
|        - |  1803 | `#endif` |
|     1674 |  1804 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1805 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1806 | `#ifdef __WINNT__` |
|        2 |  1807 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1808 | `#endif` |
|     3346 |  1809 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1810 | `		  /* Remove leading and trailing white spaces */` |
|     1674 |  1811 | `		  SyStringFullTrim(&sPath);` |
|     1674 |  1812 | `		  if( sPath.nByte > 0 ){` |
|        - |  1813 | `			  /* Store the path in the corresponding conatiner */` |
|     1674 |  1814 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      836 |  1815 | `		  }` |
|     1674 |  1816 | `		  break;` |
|        - |  1817 | `									 }` |
|      836 |  1818 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1819 | `		/* Run-Time Error report */` |
|     1674 |  1820 | `		pVm->bErrReport = 1;` |
|     1674 |  1821 | `		break;` |
|      ! 0 |  1822 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1823 | `		/* Recursion depth */` |
|      ! 0 |  1824 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1825 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1826 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1827 | `		}` |
|      ! 0 |  1828 | `		break;` |
|        - |  1829 | `									   }` |
|      ! 0 |  1830 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1831 | `		/* VM output length in bytes */` |
|      ! 0 |  1832 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1833 | `#ifdef UNTRUST` |
|        - |  1834 | `		if( pOut == 0 ){` |
|        - |  1835 | `			rc = SXERR_CORRUPT;` |
|        - |  1836 | `			break;` |
|        - |  1837 | `		}` |
|        - |  1838 | `#endif` |
|      ! 0 |  1839 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1840 | `		break;` |
|        - |  1841 | `							   }` |
|        - |  1842 |  |
|     8360 |  1843 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1844 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1845 | `		/* Create a new superglobal/global variable */` |
|    16722 |  1846 | `		const char *zName = va_arg(ap,const char *);` |
|    16722 |  1847 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1848 | `		SyHashEntry *pEntry;` |
|        - |  1849 | `		ph7_value *pObj;` |
|        - |  1850 | `		sxu32 nByte;` |
|        - |  1851 | `		sxu32 nIdx;` |
|        - |  1852 | `#ifdef UNTRUST` |
|        - |  1853 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1854 | `			rc = SXERR_CORRUPT;` |
|        - |  1855 | `			break;` |
|        - |  1856 | `		}` |
|        - |  1857 | `#endif` |
|    16722 |  1858 | `		nByte = SyStrlen(zName);` |
|    16722 |  1859 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1860 | `			/* Check if the superglobal is already installed */` |
|    16722 |  1861 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     8362 |  1862 | `		}else{` |
|        - |  1863 | `			/* Query the top active VM frame */` |
|      ! 0 |  1864 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1865 | `		}` |
|    16722 |  1866 | `		if( pEntry ){` |
|        - |  1867 | `			/* Variable already installed */` |
|      ! 0 |  1868 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1869 | `			/* Extract contents */` |
|      ! 0 |  1870 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1871 | `			if( pObj ){` |
|        - |  1872 | `				/* Overwrite old contents */` |
|      ! 0 |  1873 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1874 | `			}` |
|      ! 0 |  1875 | `		}else{` |
|        - |  1876 | `			/* Install a new variable */` |
|    16722 |  1877 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    16722 |  1878 | `			if( pObj == 0 ){` |
|      ! 0 |  1879 | `				rc = SXERR_MEM;` |
|      ! 0 |  1880 | `				break;` |
|        - |  1881 | `			}` |
|    16722 |  1882 | `			nIdx = pObj->nIdx;` |
|        - |  1883 | `			/* Copy value */` |
|    16722 |  1884 | `			PH7_MemObjStore(pValue,pObj);` |
|    16722 |  1885 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1886 | `				/* Install the superglobal */` |
|    16722 |  1887 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     8362 |  1888 | `			}else{` |
|        - |  1889 | `				/* Install in the current frame */` |
|      ! 0 |  1890 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1891 | `			}` |
|    16722 |  1892 | `			if( rc == SXRET_OK ){` |
|        - |  1893 | `				SyHashEntry *pRef;` |
|    16722 |  1894 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    16722 |  1895 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     8362 |  1896 | `				}else{` |
|      ! 0 |  1897 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1898 | `				}` |
|        - |  1899 | `				/* Install in the reference table */` |
|    16722 |  1900 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    16722 |  1901 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1902 | `					/* Register in the $GLOBALS array */` |
|    16722 |  1903 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     8360 |  1904 | `				}` |
|     8360 |  1905 | `			}` |
|        - |  1906 | `		}` |
|    16722 |  1907 | `		break;` |
|        - |  1908 | `									}` |
|      837 |  1909 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1910 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1911 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1912 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1913 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1914 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1915 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     1676 |  1916 | `		const char *zKey   = va_arg(ap,const char *);` |
|     1676 |  1917 | `		const char *zValue = va_arg(ap,const char *);` |
|     1676 |  1918 | `		int nLen = va_arg(ap,int);` |
|        - |  1919 | `		ph7_hashmap *pMap;` |
|        - |  1920 | `		ph7_value *pValue;` |
|     1676 |  1921 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1922 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1923 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     1675 |  1924 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1925 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1926 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     1674 |  1927 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1928 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1929 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     1674 |  1930 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1931 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1932 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     1674 |  1933 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1934 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1935 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     1674 |  1936 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1937 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1938 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1939 | `		}else{` |
|        - |  1940 | `			/* Extract the $_SERVER superglobal */` |
|     1674 |  1941 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1942 | `		}` |
|     1676 |  1943 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1944 | `			/* No such entry */` |
|      ! 0 |  1945 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1946 | `			break;` |
|        - |  1947 | `		}` |
|        - |  1948 | `		/* Point to the hashmap */` |
|     1676 |  1949 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1950 | `		/* Perform the insertion */` |
|     1676 |  1951 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     1676 |  1952 | `		break;` |
|        - |  1953 | `								   }` |
|       11 |  1954 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1955 | `		/* Script arguments */` |
|       24 |  1956 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1957 | `		ph7_hashmap *pMap;` |
|        - |  1958 | `		ph7_value *pValue;` |
|        - |  1959 | `		sxu32 n;` |
|       24 |  1960 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1961 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1962 | `			break;` |
|        - |  1963 | `		}` |
|        - |  1964 | `		/* Extract the $argv array */` |
|       24 |  1965 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1966 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1967 | `			/* No such entry */` |
|      ! 0 |  1968 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1969 | `			break;` |
|        - |  1970 | `		}` |
|        - |  1971 | `		/* Point to the hashmap */` |
|       24 |  1972 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1973 | `		/* Perform the insertion */` |
|       24 |  1974 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  1975 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  1976 | `		if( rc == SXRET_OK ){` |
|       24 |  1977 | `			if( pMap->nEntry > 1 ){` |
|        - |  1978 | `				/* Append space separator first */` |
|       18 |  1979 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  1980 | `			}` |
|       24 |  1981 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  1982 | `		}` |
|       24 |  1983 | `		break;` |
|        - |  1984 | `								  }` |
|      ! 0 |  1985 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  1986 | `		/* error_log() consumer */` |
|      ! 0 |  1987 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  1988 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  1989 | `		break;` |
|        - |  1990 | `										}` |
|      ! 0 |  1991 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  1992 | `		/* Script return value */` |
|      ! 0 |  1993 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  1994 | `#ifdef UNTRUST` |
|        - |  1995 | `		if( ppValue == 0 ){` |
|        - |  1996 | `			rc = SXERR_CORRUPT;` |
|        - |  1997 | `			break;` |
|        - |  1998 | `		}` |
|        - |  1999 | `#endif` |
|      ! 0 |  2000 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2001 | `		break;` |
|        - |  2002 | `								   }` |
|     1672 |  2003 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2004 | `		/* Register an IO stream device */` |
|     3346 |  2005 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2006 | `		/* Make sure we are dealing with a valid IO stream */` |
|     5016 |  2007 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     3346 |  2008 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2009 | `				/* Invalid stream */` |
|      ! 0 |  2010 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2011 | `				break;` |
|        - |  2012 | `		}` |
|     3346 |  2013 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2014 | `			/* Make the 'file://' stream the defaut stream device */` |
|     1674 |  2015 | `			pVm->pDefStream = pStream;` |
|      836 |  2016 | `		}` |
|        - |  2017 | `		/* Insert in the appropriate container */` |
|     3346 |  2018 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     3346 |  2019 | `		break;` |
|        - |  2020 | `								  }` |
|      ! 0 |  2021 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2022 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2023 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2024 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2025 | `#ifdef UNTRUST` |
|        - |  2026 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2027 | `			rc = SXERR_CORRUPT;` |
|        - |  2028 | `			break;` |
|        - |  2029 | `		}` |
|        - |  2030 | `#endif` |
|      ! 0 |  2031 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2032 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2033 | `		break;` |
|        - |  2034 | `									   }` |
|      ! 0 |  2035 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2036 | `		/* Raw HTTP request*/` |
|      ! 0 |  2037 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2038 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2039 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2040 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2041 | `			break;` |
|        - |  2042 | `		}` |
|      ! 0 |  2043 | `		if( nByte < 0 ){` |
|        - |  2044 | `			/* Compute length automatically */` |
|      ! 0 |  2045 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2046 | `		}` |
|        - |  2047 | `		/* Process the request */` |
|      ! 0 |  2048 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2049 | `		break;` |
|        - |  2050 | `									}` |
|      ! 0 |  2051 | `	default:` |
|        - |  2052 | `		/* Unknown configuration option */` |
|      ! 0 |  2053 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2054 | `		break;` |
|        - |  2055 | `	}` |
|    26778 |  2056 | `	return rc;` |
|        2 |  2057 |  |
|        - |  2058 | `/* Forward declaration */` |
|        - |  2059 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2060 | `/*` |
|        - |  2061 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2062 | ` * format.` |
|        - |  2063 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2064 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2065 | ` * (STDOUT).` |
|        - |  2066 | ` */` |
|        2 |  2067 | `static sxi32 VmByteCodeDump(` |
|        - |  2068 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2069 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2070 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2071 | `	)` |
|        1 |  2072 |  |
|        - |  2073 | `	static const char zDump[] = {` |
|        - |  2074 | `		"====================================================\n"` |
|        - |  2075 | `		"PH7 VM Dump\n"` |
|        - |  2076 | `		"====================================================\n"` |
|        - |  2077 | `	};` |
|        - |  2078 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2079 | `	sxi32 rc = SXRET_OK;` |
|        - |  2080 | `	sxu32 n;` |
|        - |  2081 | `	/* Point to the PH7 instructions */` |
|        3 |  2082 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2083 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2084 | `	n = 0;` |
|        3 |  2085 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2086 | `	/* Dump instructions */` |
|        6 |  2087 | `	for(;;){` |
|       13 |  2088 | `		if( pInstr >= pEnd ){` |
|        - |  2089 | `			/* No more instructions */` |
|        3 |  2090 | `			break;` |
|        - |  2091 | `		}` |
|        - |  2092 | `		/* Format and call the consumer callback */` |
|       16 |  2093 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2094 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2095 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2096 | `		if( rc != SXRET_OK ){` |
|        - |  2097 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2098 | `			return rc;` |
|        - |  2099 | `		}` |
|       11 |  2100 | `		++n;` |
|       11 |  2101 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2102 | `	}` |
|        3 |  2103 | `	return rc;` |
|        2 |  2104 |  |
|        - |  2105 | `/* Forward declaration */` |
|        - |  2106 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2107 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2108 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2109 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2110 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2111 | `/*` |
|        - |  2112 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2113 | ` * consumer callback.` |
|        - |  2114 | ` */` |
|      436 |  2115 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2116 |  |
|      437 |  2117 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      437 |  2118 | `	sxi32 rc = SXRET_OK;` |
|        - |  2119 | `	/* Append a new line */` |
|        - |  2120 | `#ifdef __WINNT__` |
|        1 |  2121 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2122 | `#else` |
|      436 |  2123 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2124 | `#endif` |
|        - |  2125 | `	/* Invoke the output consumer callback */` |
|      437 |  2126 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      437 |  2127 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2128 | `		/* Increment output length */` |
|      437 |  2129 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      218 |  2130 | `	}` |
|      437 |  2131 | `	return rc;` |
|        1 |  2132 |  |
|        - |  2133 | `/*` |
|        - |  2134 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2135 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2136 | ` * information.` |
|        - |  2137 | ` */` |
|      138 |  2138 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2139 |  |
|      140 |  2140 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2141 | `		ph7_value apArg[4];` |
|        - |  2142 | `		ph7_value *apArgPtr[4];` |
|        - |  2143 | `		ph7_value sResult;` |
|        - |  2144 | `		SyString sErr;` |
|        - |  2145 | `		/* Prepare arguments */` |
|       61 |  2146 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2147 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2148 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2149 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2150 | `		if( pFile ){` |
|       61 |  2151 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2152 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2153 | `		}else{` |
|      ! 0 |  2154 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2155 | `		}` |
|       61 |  2156 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2157 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2158 | `		/* Set up pointer array */` |
|       61 |  2159 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2160 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2161 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2162 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2163 | `		/* Call the handler */` |
|       61 |  2164 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2165 | `		/* Check return value */` |
|       61 |  2166 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2167 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2168 | `		}` |
|        - |  2169 | `		/* Release */` |
|       61 |  2170 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2171 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2172 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2173 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2174 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2175 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2176 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2177 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2178 | `	}` |
|        - |  2179 | `	/* No handler, always call error handler */` |
|       79 |  2180 | `	return TRUE;` |
|       71 |  2181 |  |
|      102 |  2182 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2183 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2184 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2185 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2186 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2187 | `	)` |
|        2 |  2188 |  |
|      104 |  2189 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2190 | `	SyString *pFile;` |
|        - |  2191 | `	char *zErr;` |
|      104 |  2192 | `	sxi32 rc = SXRET_OK;` |
|      104 |  2193 | `	if( !pVm->bErrReport ){` |
|        - |  2194 | `		/* Don't bother reporting errors */` |
|        3 |  2195 | `		return SXRET_OK;` |
|        - |  2196 | `	}` |
|        - |  2197 | `	/* Reset the working buffer */` |
|      102 |  2198 | `	SyBlobReset(pWorker);` |
|        - |  2199 | `	/* Peek the processed file if available */` |
|      102 |  2200 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      102 |  2201 | `	if( pFile ){` |
|        - |  2202 | `		/* Append file name */` |
|      102 |  2203 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      102 |  2204 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       50 |  2205 | `	}` |
|        - |  2206 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2207 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2208 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2209 | `	 * E_DEPRECATED). */` |
|      102 |  2210 | `	zErr = "Error:  ";` |
|      102 |  2211 | `	switch(iErr){` |
|       21 |  2212 | `	case PH7_CTX_WARNING:` |
|       44 |  2213 | `		zErr = "Warning:  ";` |
|       44 |  2214 | `		break;` |
|        6 |  2215 | `	case PH7_CTX_NOTICE:` |
|       14 |  2216 | `		zErr = "Notice:  ";` |
|       12 |  2217 | `		break;` |
|       23 |  2218 | `	default:` |
|        - |  2219 | `		/* keep iErr unchanged */` |
|       46 |  2220 | `		break;` |
|        - |  2221 | `	}` |
|      102 |  2222 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      102 |  2223 | `	if( pFuncName ){` |
|        - |  2224 | `		/* Append function name first */` |
|       29 |  2225 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       29 |  2226 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       14 |  2227 | `	}` |
|      102 |  2228 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2229 | `	/* Check for user error handler.  compute length of C string */` |
|      102 |  2230 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       53 |  2231 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       26 |  2232 | `	}` |
|      102 |  2233 | `	return rc;` |
|       53 |  2234 |  |
|        - |  2235 | `/*` |
|        - |  2236 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2237 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2238 | ` * information.` |
|        - |  2239 | ` */` |
|       38 |  2240 | `static sxi32 VmThrowErrorAp(` |
|        - |  2241 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2242 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2243 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2244 | `	const char *zFormat, /* Format message */` |
|        - |  2245 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2246 | `	)` |
|        2 |  2247 |  |
|       40 |  2248 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2249 | `	SyBlob sMsg;` |
|        - |  2250 | `	SyString *pFile;` |
|        - |  2251 | `	char *zErr;` |
|       40 |  2252 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2253 | `	if( !pVm->bErrReport ){` |
|        - |  2254 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2255 | `		return SXRET_OK;` |
|        - |  2256 | `	}` |
|        - |  2257 | `	/* Reset the working buffer */` |
|       40 |  2258 | `	SyBlobReset(pWorker);` |
|        - |  2259 | `	/* Peek the processed file if available */` |
|       40 |  2260 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2261 | `	if( pFile ){` |
|        - |  2262 | `		/* Append file name */` |
|       40 |  2263 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2264 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2265 | `	}` |
|        - |  2266 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2267 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2268 | `	 * the correct errno value. */` |
|       40 |  2269 | `	zErr = "Error:  ";` |
|       40 |  2270 | `	switch(iErr){` |
|        4 |  2271 | `	case PH7_CTX_WARNING:` |
|        9 |  2272 | `		zErr = "Warning:  ";` |
|        9 |  2273 | `		break;` |
|        3 |  2274 | `	case PH7_CTX_NOTICE:` |
|        7 |  2275 | `		zErr = "Notice:  ";` |
|        6 |  2276 | `		break;` |
|       12 |  2277 | `	default:` |
|        - |  2278 | `		/* do not change iErr */` |
|       24 |  2279 | `		break;` |
|        - |  2280 | `	}` |
|       40 |  2281 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2282 | `	if( pFuncName ){` |
|        - |  2283 | `		/* Append function name first */` |
|       26 |  2284 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2285 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2286 | `	}` |
|        - |  2287 | `	/* Format the raw message */` |
|       40 |  2288 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2289 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2290 | `	/* Check if a user error handler is installed */` |
|       40 |  2291 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2292 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2293 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2294 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2295 | `	}` |
|       40 |  2296 | `	SyBlobRelease(&sMsg);` |
|       40 |  2297 | `	return rc;` |
|       21 |  2298 |  |
|        - |  2299 | `/*` |
|        - |  2300 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2301 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2302 | ` * information.` |
|        - |  2303 | ` * ------------------------------------` |
|        - |  2304 | ` * Simple boring wrapper function.` |
|        - |  2305 | ` * ------------------------------------` |
|        - |  2306 | ` */` |
|       14 |  2307 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2308 |  |
|        - |  2309 | `	va_list ap;` |
|        - |  2310 | `	sxi32 rc;` |
|       15 |  2311 | `	va_start(ap,zFormat);` |
|       15 |  2312 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2313 | `	va_end(ap);` |
|       15 |  2314 | `	return rc;` |
|        1 |  2315 |  |
|        - |  2316 | `/*` |
|        - |  2317 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2318 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2319 | ` * information.` |
|        - |  2320 | ` * ------------------------------------` |
|        - |  2321 | ` * Simple boring wrapper function.` |
|        - |  2322 | ` * ------------------------------------` |
|        - |  2323 | ` */` |
|       24 |  2324 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2325 |  |
|        - |  2326 | `	sxi32 rc;` |
|       26 |  2327 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2328 | `	return rc;` |
|        2 |  2329 |  |
|        - |  2330 | `/*` |
|        - |  2331 | ` * Resolve function context from the current frame.` |
|        - |  2332 | ` */` |
|      712 |  2333 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2334 |  |
|        - |  2335 | `	VmFrame *pFrame;` |
|        - |  2336 | `	ph7_vm_func *pFunc;` |
|      713 |  2337 | `	*pzFuncName = 0;` |
|      713 |  2338 | `	*pnFuncLen = 0;` |
|      713 |  2339 | `	pFrame = pVm->pFrame;` |
|      713 |  2340 | `	if( pFrame == 0 ){` |
|      ! 0 |  2341 | `		return;` |
|        - |  2342 | `	}` |
|      713 |  2343 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2344 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2345 | `	}` |
|      713 |  2346 | `	if( pFrame->pParent == 0 ){` |
|      709 |  2347 | `		return;` |
|        - |  2348 | `	}` |
|        5 |  2349 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  2350 | `	if( pFunc == 0 ){` |
|      ! 0 |  2351 | `		return;` |
|        - |  2352 | `	}` |
|        5 |  2353 | `	*pzFuncName = pFunc->sName.zString;` |
|        5 |  2354 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      357 |  2355 |  |
|        - |  2356 | `/*` |
|        - |  2357 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2358 | ` */` |
|      358 |  2359 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2360 |  |
|        - |  2361 | `	SyBlob sOut;` |
|        - |  2362 | `	SyString *pFile;` |
|      359 |  2363 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2364 | `		return PH7_OK;` |
|        - |  2365 | `	}` |
|      359 |  2366 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2367 | `		zClass = "Exception";` |
|      ! 0 |  2368 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2369 | `	}` |
|      359 |  2370 | `	if( zMsg == 0 ){` |
|      ! 0 |  2371 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2372 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2373 | `	}` |
|      359 |  2374 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      355 |  2375 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      177 |  2376 | `	}` |
|      359 |  2377 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      359 |  2378 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      359 |  2379 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      359 |  2380 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      359 |  2381 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      359 |  2382 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      359 |  2383 | `	if( pFile ){` |
|      359 |  2384 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      359 |  2385 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2386 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      179 |  2387 | `	}` |
|      359 |  2388 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      359 |  2389 | `	if( pFile ){` |
|      359 |  2390 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      359 |  2391 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2392 | `		if( zFuncName && nFuncLen > 0 ){` |
|        5 |  2393 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        3 |  2394 | `		}else{` |
|      355 |  2395 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2396 | `		}` |
|      179 |  2397 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2398 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2399 | `	}else{` |
|      ! 0 |  2400 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2401 | `	}` |
|      359 |  2402 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      359 |  2403 | `	if( pFile ){` |
|      359 |  2404 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      359 |  2405 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      359 |  2406 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      359 |  2407 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      179 |  2408 | `	}` |
|      359 |  2409 | `	VmCallErrorHandler(pVm,&sOut);` |
|      359 |  2410 | `	SyBlobRelease(&sOut);` |
|      359 |  2411 | `	return PH7_ABORT;` |
|      180 |  2412 |  |
|        - |  2413 | `/*` |
|        - |  2414 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2415 | ` */` |
|      354 |  2416 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2417 |  |
|        - |  2418 | `	ph7_vm *pVm;` |
|        - |  2419 | `	ph7_class *pClass;` |
|        - |  2420 | `	ph7_class_instance *pThis;` |
|        - |  2421 | `	ph7_class_method *pCons;` |
|        - |  2422 | `	ph7_value sArg;` |
|        - |  2423 | `	ph7_value *apArg[1];` |
|        - |  2424 | `	SyBlob sMsg;` |
|        - |  2425 | `	SyString sMsgStr;` |
|        - |  2426 | `	VmFrame *pFrame;` |
|        - |  2427 | `	va_list ap;` |
|        - |  2428 | `	sxi32 rc;` |
|        - |  2429 |  |
|      356 |  2430 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2431 | `		return PH7_ABORT;` |
|        - |  2432 | `	}` |
|      356 |  2433 | `	pVm = pCtx->pVm;` |
|      356 |  2434 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2435 | `		zClass = "Error";` |
|      ! 0 |  2436 | `	}` |
|      356 |  2437 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      356 |  2438 | `	if( pClass == 0 ){` |
|      ! 0 |  2439 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2440 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2441 | `			zClass` |
|        - |  2442 | `			);` |
|        - |  2443 | `	}` |
|      356 |  2444 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      356 |  2445 | `	if( pThis == 0 ){` |
|      ! 0 |  2446 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2447 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2448 | `			);` |
|        - |  2449 | `	}` |
|        - |  2450 |  |
|      356 |  2451 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      356 |  2452 | `	va_start(ap,zFormat);` |
|      356 |  2453 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      356 |  2454 | `	va_end(ap);` |
|        - |  2455 |  |
|      356 |  2456 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      356 |  2457 | `	if( pCons ){` |
|      356 |  2458 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      356 |  2459 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      356 |  2460 | `		apArg[0] = &sArg;` |
|      356 |  2461 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      356 |  2462 | `		PH7_MemObjRelease(&sArg);` |
|      177 |  2463 | `	}` |
|      356 |  2464 | `	SyBlobRelease(&sMsg);` |
|        - |  2465 |  |
|      356 |  2466 | `	pFrame = pVm->pFrame;` |
|      356 |  2467 | `	if( pFrame ){` |
|      358 |  2468 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  2469 | `			pFrame = pFrame->pParent;` |
|        1 |  2470 | `		}` |
|      356 |  2471 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      177 |  2472 | `	}` |
|      356 |  2473 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      356 |  2474 | `	PH7_ClassInstanceUnref(pThis);` |
|      356 |  2475 | `	if( rc == SXERR_ABORT ){` |
|      353 |  2476 | `		return PH7_ABORT;` |
|        - |  2477 | `	}` |
|        3 |  2478 | `	return PH7_EXCEPTION;` |
|      179 |  2479 |  |
|        - |  2480 | `/*` |
|        - |  2481 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2482 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2483 | ` */` |
|      ! 0 |  2484 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2485 |  |
|        - |  2486 | `	ph7_vm *pVm;` |
|        - |  2487 | `	SyBlob sMsg;` |
|      ! 0 |  2488 | `	const char *zFuncName = 0;` |
|      ! 0 |  2489 | `	int nFuncLen = 0;` |
|        - |  2490 | `	va_list ap;` |
|        - |  2491 | `	sxi32 rc;` |
|        - |  2492 |  |
|      ! 0 |  2493 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2494 | `		return PH7_OK;` |
|        - |  2495 | `	}` |
|      ! 0 |  2496 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2497 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2498 | `		zClass = "Error";` |
|      ! 0 |  2499 | `	}` |
|        - |  2500 |  |
|      ! 0 |  2501 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2502 |  |
|      ! 0 |  2503 | `	va_start(ap,zFormat);` |
|      ! 0 |  2504 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2505 | `	va_end(ap);` |
|        - |  2506 |  |
|      ! 0 |  2507 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2508 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2509 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2510 | `	}` |
|      ! 0 |  2511 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2512 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2513 | `	}` |
|      ! 0 |  2514 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2515 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2516 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2517 | `	return rc;` |
|      ! 0 |  2518 |  |
|        - |  2519 | `/*` |
|        - |  2520 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2521 | ` *` |
|        - |  2522 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2523 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2524 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2525 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2526 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2527 | ` * then the program execution is halted.` |
|        - |  2528 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2529 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2530 | ` * or to reset the VM to it's initial state.` |
|        - |  2531 | ` */` |
|    26958 |  2532 | `static sxi32 VmByteCodeExec(` |
|        - |  2533 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2534 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2535 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2536 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2537 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2538 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2539 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2540 | `	)` |
|        2 |  2541 |  |
|        - |  2542 | `	VmInstr *pInstr;` |
|        - |  2543 | `	ph7_value *pTos;` |
|        - |  2544 | `	SySet aArg;` |
|        - |  2545 | `	sxi32 pc;` |
|        - |  2546 | `	sxi32 rc;` |
|        - |  2547 | `	/* Argument container */` |
|    26960 |  2548 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    26960 |  2549 | `	if( nTos < 0 ){` |
|    25610 |  2550 | `		pTos = &pStack[-1];` |
|    12806 |  2551 | `	}else{` |
|     1352 |  2552 | `		pTos = &pStack[nTos];` |
|        - |  2553 | `	}` |
|    26960 |  2554 | `	pc = 0;` |
|        - |  2555 | `	/* Execute as much as we can */` |
|  4710173 |  2556 | `	for(;;){` |
|        - |  2557 | `		/* Fetch the instruction to execute */` |
|  9419644 |  2558 | `		pInstr = &aInstr[pc];` |
|  9419644 |  2559 | `		rc = SXRET_OK;` |
|        - |  2560 | `/*` |
|        - |  2561 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2562 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2563 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2564 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2565 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2566 | ` */` |
|  9419644 |  2567 | `		switch(pInstr->iOp){` |
|        - |  2568 | `/*` |
|        - |  2569 | ` * DONE: P1 * *` |
|        - |  2570 | ` *` |
|        - |  2571 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2572 | ` * and return immediately.` |
|        - |  2573 | ` */` |
|    13291 |  2574 | `case PH7_OP_DONE:` |
|    26584 |  2575 | `	if( pInstr->iP1 ){` |
|        - |  2576 | `#ifdef UNTRUST` |
|        - |  2577 | `		if( pTos < pStack ){` |
|        - |  2578 | `			goto Abort;` |
|        - |  2579 | `		}` |
|        - |  2580 | `#endif` |
|    15128 |  2581 | `		if( pLastRef ){` |
|     9900 |  2582 | `			*pLastRef = pTos->nIdx;` |
|     4949 |  2583 | `		}` |
|    15128 |  2584 | `		if( pResult ){` |
|        - |  2585 | `			/* Execution result */` |
|    14452 |  2586 | `			PH7_MemObjStore(pTos,pResult);` |
|     7225 |  2587 | `		}` |
|    15128 |  2588 | `		VmPopOperand(&pTos,1);` |
|    19021 |  2589 | `	}else if( pLastRef ){` |
|        - |  2590 | `		/* Nothing referenced */` |
|      762 |  2591 | `		*pLastRef = SXU32_HIGH;` |
|      380 |  2592 | `	}` |
|    26584 |  2593 | `	goto Done;` |
|        - |  2594 | `/*` |
|        - |  2595 | ` * HALT: P1 * *` |
|        - |  2596 | ` *` |
|        - |  2597 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2598 | ` * and abort immediately.` |
|        - |  2599 | ` */` |
|        4 |  2600 | `case PH7_OP_HALT:` |
|        9 |  2601 | `	if( pInstr->iP1 ){` |
|        - |  2602 | `#ifdef UNTRUST` |
|        - |  2603 | `		if( pTos < pStack ){` |
|        - |  2604 | `			goto Abort;` |
|        - |  2605 | `		}` |
|        - |  2606 | `#endif` |
|        9 |  2607 | `		if( pLastRef ){` |
|      ! 0 |  2608 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2609 | `		}` |
|        9 |  2610 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2611 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2612 | `				/* Output the exit message */` |
|        7 |  2613 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2614 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2615 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2616 | `					/* Increment output length */` |
|        5 |  2617 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2618 | `				}` |
|        3 |  2619 | `			}` |
|        7 |  2620 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2621 | `			/* Record exit status */` |
|        5 |  2622 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2623 | `		}` |
|        9 |  2624 | `		VmPopOperand(&pTos,1);` |
|        4 |  2625 | `	}else if( pLastRef ){` |
|        - |  2626 | `		/* Nothing referenced */` |
|      ! 0 |  2627 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2628 | `	}` |
|        - |  2629 | `	/* Check if we're in an included file context */` |
|        9 |  2630 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2631 | `		/* Terminate the entire process */` |
|        9 |  2632 | `		exit(pVm->iExitStatus);` |
|        - |  2633 | `	}` |
|      ! 0 |  2634 | `	goto Abort;` |
|        - |  2635 | `/*` |
|        - |  2636 | ` * JMP: * P2 *` |
|        - |  2637 | ` *` |
|        - |  2638 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2639 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2640 | ` */` |
|   206992 |  2641 | `case PH7_OP_JMP:` |
|   414030 |  2642 | `	pc = pInstr->iP2 - 1;` |
|   414030 |  2643 | `	break;` |
|        - |  2644 | `/*` |
|        - |  2645 | ` * JZ: P1 P2 *` |
|        - |  2646 | ` *` |
|        - |  2647 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2648 | ` * entry in the stack if P1 is zero.` |
|        - |  2649 | ` */` |
|   478032 |  2650 | `case PH7_OP_JZ:` |
|        - |  2651 | `#ifdef UNTRUST` |
|        - |  2652 | `	if( pTos < pStack ){` |
|        - |  2653 | `		goto Abort;` |
|        - |  2654 | `	}` |
|        - |  2655 | `#endif` |
|        - |  2656 | `	/* Get a boolean value */` |
|   956154 |  2657 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2658 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2659 | `	}` |
|   956154 |  2660 | `	if( !pTos->x.iVal ){` |
|        - |  2661 | `		/* Take the jump */` |
|   458792 |  2662 | `		pc = pInstr->iP2 - 1;` |
|   229395 |  2663 | `	}` |
|   956154 |  2664 | `	if( !pInstr->iP1 ){` |
|   752614 |  2665 | `		VmPopOperand(&pTos,1);` |
|   376328 |  2666 | `	}` |
|   956154 |  2667 | `	break;` |
|        - |  2668 | `/*` |
|        - |  2669 | ` * JNZ: P1 P2 *` |
|        - |  2670 | ` *` |
|        - |  2671 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2672 | ` * entry in the stack if P1 is zero.` |
|        - |  2673 | ` */` |
|    51022 |  2674 | `case PH7_OP_JNZ:` |
|        - |  2675 | `#ifdef UNTRUST` |
|        - |  2676 | `	if( pTos < pStack ){` |
|        - |  2677 | `		goto Abort;` |
|        - |  2678 | `	}` |
|        - |  2679 | `#endif` |
|        - |  2680 | `	/* Get a boolean value */` |
|   102046 |  2681 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2682 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2683 | `	}` |
|   102046 |  2684 | `	if( pTos->x.iVal ){` |
|        - |  2685 | `		/* Take the jump */` |
|     3938 |  2686 | `		pc = pInstr->iP2 - 1;` |
|     1968 |  2687 | `	}` |
|   102046 |  2688 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2689 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2690 | `	}` |
|   102046 |  2691 | `	break;` |
|        - |  2692 | `/*` |
|        - |  2693 | ` * NOOP: * * *` |
|        - |  2694 | ` *` |
|        - |  2695 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2696 | ` * destination.` |
|        - |  2697 | ` */` |
|      ! 0 |  2698 | `case PH7_OP_NOOP:` |
|      ! 0 |  2699 | `	break;` |
|        - |  2700 | `/*` |
|        - |  2701 | ` * POP: P1 * *` |
|        - |  2702 | ` *` |
|        - |  2703 | ` * Pop P1 elements from the operand stack.` |
|        - |  2704 | ` */` |
|   368584 |  2705 | `case PH7_OP_POP: {` |
|   737214 |  2706 | `	sxi32 n = pInstr->iP1;` |
|   737214 |  2707 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2708 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2709 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2710 | `	}` |
|   737214 |  2711 | `	VmPopOperand(&pTos,n);` |
|   737214 |  2712 | `	break;` |
|        - |  2713 | `				 }` |
|        - |  2714 | `/*` |
|        - |  2715 | ` * CVT_INT: * * *` |
|        - |  2716 | ` *` |
|        - |  2717 | ` * Force the top of the stack to be an integer.` |
|        - |  2718 | ` */` |
|       35 |  2719 | `case PH7_OP_CVT_INT:` |
|        - |  2720 | `#ifdef UNTRUST` |
|        - |  2721 | `	if( pTos < pStack ){` |
|        - |  2722 | `		goto Abort;` |
|        - |  2723 | `	}` |
|        - |  2724 | `#endif` |
|       72 |  2725 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2726 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2727 | `	}` |
|        - |  2728 | `	/* Invalidate any prior representation */` |
|       72 |  2729 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2730 | `	break;` |
|        - |  2731 | `/*` |
|        - |  2732 | ` * CVT_REAL: * * *` |
|        - |  2733 | ` *` |
|        - |  2734 | ` * Force the top of the stack to be a real.` |
|        - |  2735 | ` */` |
|        4 |  2736 | `case PH7_OP_CVT_REAL:` |
|        - |  2737 | `#ifdef UNTRUST` |
|        - |  2738 | `	if( pTos < pStack ){` |
|        - |  2739 | `		goto Abort;` |
|        - |  2740 | `	}` |
|        - |  2741 | `#endif` |
|        9 |  2742 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2743 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2744 | `	}` |
|        - |  2745 | `	/* Invalidate any prior representation */` |
|        9 |  2746 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2747 | `	break;` |
|        - |  2748 | `/*` |
|        - |  2749 | ` * CVT_STR: * * *` |
|        - |  2750 | ` *` |
|        - |  2751 | ` * Force the top of the stack to be a string.` |
|        - |  2752 | ` */` |
|      136 |  2753 | `case PH7_OP_CVT_STR:` |
|        - |  2754 | `#ifdef UNTRUST` |
|        - |  2755 | `	if( pTos < pStack ){` |
|        - |  2756 | `		goto Abort;` |
|        - |  2757 | `	}` |
|        - |  2758 | `#endif` |
|      274 |  2759 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      274 |  2760 | `		PH7_MemObjToString(pTos);` |
|      136 |  2761 | `	}` |
|      274 |  2762 | `	break;` |
|        - |  2763 | `/*` |
|        - |  2764 | ` * CVT_BOOL: * * *` |
|        - |  2765 | ` *` |
|        - |  2766 | ` * Force the top of the stack to be a boolean.` |
|        - |  2767 | ` */` |
|        5 |  2768 | `case PH7_OP_CVT_BOOL:` |
|        - |  2769 | `#ifdef UNTRUST` |
|        - |  2770 | `	if( pTos < pStack ){` |
|        - |  2771 | `		goto Abort;` |
|        - |  2772 | `	}` |
|        - |  2773 | `#endif` |
|       11 |  2774 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2775 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2776 | `	}` |
|       11 |  2777 | `	break;` |
|        - |  2778 | `/*` |
|        - |  2779 | ` * CVT_NULL: * * *` |
|        - |  2780 | ` *` |
|        - |  2781 | ` * Nullify the top of the stack.` |
|        - |  2782 | ` */` |
|        3 |  2783 | `case PH7_OP_CVT_NULL:` |
|        - |  2784 | `#ifdef UNTRUST` |
|        - |  2785 | `	if( pTos < pStack ){` |
|        - |  2786 | `		goto Abort;` |
|        - |  2787 | `	}` |
|        - |  2788 | `#endif` |
|        7 |  2789 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2790 | `	break;` |
|        - |  2791 | `/*` |
|        - |  2792 | ` * CVT_NUMC: * * *` |
|        - |  2793 | ` *` |
|        - |  2794 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2795 | ` */` |
|      ! 0 |  2796 | `case PH7_OP_CVT_NUMC:` |
|        - |  2797 | `#ifdef UNTRUST` |
|        - |  2798 | `	if( pTos < pStack ){` |
|        - |  2799 | `		goto Abort;` |
|        - |  2800 | `	}` |
|        - |  2801 | `#endif` |
|        - |  2802 | `	/* Force a numeric cast */` |
|      ! 0 |  2803 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2804 | `	break;` |
|        - |  2805 | `/*` |
|        - |  2806 | ` * CVT_ARRAY: * * *` |
|        - |  2807 | ` *` |
|        - |  2808 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2809 | ` */` |
|       10 |  2810 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2811 | `#ifdef UNTRUST` |
|        - |  2812 | `	if( pTos < pStack ){` |
|        - |  2813 | `		goto Abort;` |
|        - |  2814 | `	}` |
|        - |  2815 | `#endif` |
|        - |  2816 | `	/* Force a hashmap cast */` |
|       21 |  2817 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2818 | `	if( rc != SXRET_OK ){` |
|        - |  2819 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2820 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2821 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2822 | `	}` |
|       21 |  2823 | `	break;` |
|        - |  2824 | `/*` |
|        - |  2825 | ` * CVT_OBJ: * * *` |
|        - |  2826 | ` *` |
|        - |  2827 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2828 | ` */` |
|        8 |  2829 | `case PH7_OP_CVT_OBJ:` |
|        - |  2830 | `#ifdef UNTRUST` |
|        - |  2831 | `	if( pTos < pStack ){` |
|        - |  2832 | `		goto Abort;` |
|        - |  2833 | `	}` |
|        - |  2834 | `#endif` |
|       17 |  2835 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2836 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2837 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2838 | `	}` |
|       17 |  2839 | `	break;` |
|        - |  2840 | `/*` |
|        - |  2841 | ` * ERR_CTRL * * *` |
|        - |  2842 | ` *` |
|        - |  2843 | ` * Error control operator.` |
|        - |  2844 | ` */` |
|    11428 |  2845 | `case PH7_OP_ERR_CTRL:` |
|        - |  2846 | `	/*` |
|        - |  2847 | `	 * TICKET 1433-038:` |
|        - |  2848 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2849 | `	 * use the public API,to control error output.` |
|        - |  2850 | `	 */` |
|    22856 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * IS_A * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2856 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2857 | ` * holding a class name or an object).` |
|        - |  2858 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2859 | ` */` |
|       11 |  2860 | `case PH7_OP_IS_A:{` |
|       23 |  2861 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2862 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2863 | `#ifdef UNTRUST` |
|        - |  2864 | `	if( pNos < pStack ){` |
|        - |  2865 | `		goto Abort;` |
|        - |  2866 | `	}` |
|        - |  2867 | `#endif` |
|       23 |  2868 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2869 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2870 | `		ph7_class *pClass = 0;` |
|        - |  2871 | `		/* Extract the target class */` |
|       21 |  2872 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2873 | `			/* Instance already loaded */` |
|      ! 0 |  2874 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2875 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2876 | `			/* Perform the query */` |
|       31 |  2877 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2878 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2879 | `		}` |
|       21 |  2880 | `		if( pClass ){` |
|        - |  2881 | `			/* Perform the query */` |
|       21 |  2882 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2883 | `		}` |
|       10 |  2884 | `	}` |
|        - |  2885 | `	/* Push result */` |
|       23 |  2886 | `	VmPopOperand(&pTos,1);` |
|       23 |  2887 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2888 | `	pTos->x.iVal = iRes;` |
|       23 |  2889 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2890 | `	break;` |
|        - |  2891 | `				 }` |
|        - |  2892 |  |
|        - |  2893 | `/*` |
|        - |  2894 | ` * LOADC P1 P2 *` |
|        - |  2895 | ` *` |
|        - |  2896 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2897 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2898 | ` */` |
|   756005 |  2899 | `case PH7_OP_LOADC: {` |
|        - |  2900 | `	ph7_value *pObj;` |
|        - |  2901 | `	/* Reserve a room */` |
|  1512056 |  2902 | `	pTos++;` |
|  1512056 |  2903 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1512056 |  2904 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2905 | `			SyHashEntry *pEntry;` |
|        - |  2906 | `			/* Candidate for expansion via user defined callbacks */` |
|    17292 |  2907 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17292 |  2908 | `			if( pEntry ){` |
|    14210 |  2909 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2910 | `				/* Set a NULL default value */` |
|    14210 |  2911 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    14210 |  2912 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2913 | `				/* Invoke the callback and deal with the expanded value */` |
|    14210 |  2914 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2915 | `				/* Mark as constant */` |
|    14210 |  2916 | `				pTos->nIdx = SXU32_HIGH;` |
|    14210 |  2917 | `				break;` |
|        - |  2918 | `			}` |
|     1541 |  2919 | `		}` |
|  1497848 |  2920 | `		PH7_MemObjLoad(pObj,pTos);` |
|   748947 |  2921 | `	}else{` |
|        - |  2922 | `		/* Set a NULL value */` |
|      ! 0 |  2923 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2924 | `	}` |
|        - |  2925 | `	/* Mark as constant */` |
|  1497848 |  2926 | `	pTos->nIdx = SXU32_HIGH;` |
|  1497848 |  2927 | `	break;` |
|        - |  2928 | `				  }` |
|        - |  2929 | `/*` |
|        - |  2930 | ` * LOAD: P1 * P3` |
|        - |  2931 | ` *` |
|        - |  2932 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2933 | ` * from the P3 operand.` |
|        - |  2934 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2935 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2936 | ` */` |
|  1301715 |  2937 | `case PH7_OP_LOAD:{` |
|        - |  2938 | `	ph7_value *pObj;` |
|        - |  2939 | `	SyString sName;` |
|  2603652 |  2940 | `	if( pInstr->p3 == 0 ){` |
|        - |  2941 | `		/* Take the variable name from the top of the stack */` |
|        - |  2942 | `#ifdef UNTRUST` |
|        - |  2943 | `		if( pTos < pStack ){` |
|        - |  2944 | `			goto Abort;` |
|        - |  2945 | `		}` |
|        - |  2946 | `#endif` |
|        - |  2947 | `		/* Force a string cast */` |
|       19 |  2948 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2949 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2950 | `		}` |
|       19 |  2951 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  2952 | `	}else{` |
|  2603634 |  2953 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2954 | `		/* Reserve a room for the target object */` |
|  2603634 |  2955 | `		pTos++;` |
|        - |  2956 | `	}` |
|        - |  2957 | `	/* Extract the requested memory object */` |
|  2603652 |  2958 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2603652 |  2959 | `	if( pObj == 0 ){` |
|      568 |  2960 | `		if( pInstr->iP1 ){` |
|        - |  2961 | `			/* Variable not found,load NULL */` |
|      568 |  2962 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2963 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2964 | `			}else{` |
|      568 |  2965 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2966 | `			}` |
|      568 |  2967 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1302000 |  2968 | `			break;` |
|      ! 0 |  2969 | `		}else{` |
|        - |  2970 | `			/* Fatal error */` |
|      ! 0 |  2971 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2972 | `			goto Abort;` |
|        - |  2973 | `		}` |
|        - |  2974 | `	}` |
|        - |  2975 | `	/* Load variable contents */` |
|  2603086 |  2976 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2603086 |  2977 | `	pTos->nIdx = pObj->nIdx;` |
|  2603086 |  2978 | `	break;` |
|        - |  2979 | `				   }` |
|        - |  2980 | `/*` |
|        - |  2981 | ` * LOAD_MAP P1 * *` |
|        - |  2982 | ` *` |
|        - |  2983 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2984 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2985 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2986 | ` */` |
|    16446 |  2987 | `case PH7_OP_LOAD_MAP: {` |
|        - |  2988 | `	ph7_hashmap *pMap;` |
|        - |  2989 | `	/* Allocate a new hashmap instance */` |
|    32894 |  2990 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    32894 |  2991 | `	if( pMap == 0 ){` |
|      ! 0 |  2992 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  2993 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  2994 | `		goto Abort;` |
|        - |  2995 | `	}` |
|    32894 |  2996 | `	if( pInstr->iP1 > 0 ){` |
|     1934 |  2997 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  2998 | `		/* Perform the insertion */` |
|     5898 |  2999 | `		while( pEntry < pTos ){` |
|     3966 |  3000 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3001 | `				/* Insertion by reference */` |
|      142 |  3002 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3003 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3004 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3005 | `					);` |
|       48 |  3006 | `			}else{` |
|        - |  3007 | `				/* Standard insertion */` |
|     5807 |  3008 | `				PH7_HashmapInsert(pMap,` |
|     3870 |  3009 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     1935 |  3010 | `					&pEntry[1]` |
|        - |  3011 | `				);` |
|        - |  3012 | `			}` |
|        - |  3013 | `			/* Next pair on the stack */` |
|     3966 |  3014 | `			pEntry += 2;` |
|        2 |  3015 | `		}` |
|        - |  3016 | `		/* Pop P1 elements */` |
|     1934 |  3017 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|      966 |  3018 | `	}` |
|        - |  3019 | `	/* Push the hashmap */` |
|    32894 |  3020 | `	pTos++;` |
|    32894 |  3021 | `	pTos->nIdx = SXU32_HIGH;` |
|    32894 |  3022 | `	pTos->x.pOther = pMap;` |
|    32894 |  3023 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    32894 |  3024 | `	break;` |
|        - |  3025 | `					  }` |
|        - |  3026 | `/*` |
|        - |  3027 | ` * LOAD_LIST: P1 * *` |
|        - |  3028 | ` *` |
|        - |  3029 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3030 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3031 | ` * Caveats:` |
|        - |  3032 | ` *  This implementation support only a single nesting level.` |
|        - |  3033 | ` */` |
|       17 |  3034 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3035 | `	ph7_value *pEntry;` |
|       35 |  3036 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3037 | `		/* Empty list,break immediately */` |
|      ! 0 |  3038 | `		break;` |
|        - |  3039 | `	}` |
|       35 |  3040 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3041 | `#ifdef UNTRUST` |
|        - |  3042 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3043 | `		goto Abort;` |
|        - |  3044 | `	}` |
|        - |  3045 | `#endif` |
|       35 |  3046 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3047 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3048 | `		ph7_hashmap_node *pNode;` |
|        - |  3049 | `		ph7_value sKey,*pObj;` |
|        - |  3050 | `		/* Start Copying */` |
|       31 |  3051 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3052 | `		while( pEntry <= pTos ){` |
|       69 |  3053 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3054 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3055 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3056 | `					if( rc == SXRET_OK ){` |
|        - |  3057 | `						/* Store node value */` |
|       65 |  3058 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3059 | `					}else{` |
|        - |  3060 | `						/* Nullify the variable */` |
|      ! 0 |  3061 | `						PH7_MemObjRelease(pObj);` |
|        - |  3062 | `					}` |
|       32 |  3063 | `				}` |
|       32 |  3064 | `			}` |
|       69 |  3065 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3066 | `			pEntry++;` |
|        1 |  3067 | `		}` |
|       15 |  3068 | `	}` |
|       35 |  3069 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3070 | `	break;` |
|        - |  3071 | `					   }` |
|        - |  3072 | `/*` |
|        - |  3073 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3074 | ` *` |
|        - |  3075 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3076 | ` * from the stack.` |
|        - |  3077 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3078 | ` * instead.` |
|        - |  3079 | ` */` |
|   213799 |  3080 | `case PH7_OP_LOAD_IDX: {` |
|   427644 |  3081 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   427644 |  3082 | `	ph7_hashmap *pMap = 0;` |
|        - |  3083 | `	ph7_value *pIdx;` |
|   427644 |  3084 | `	pIdx = 0;` |
|   427644 |  3085 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3086 | `		if( !pInstr->iP2){` |
|        - |  3087 | `			/* No available index,load NULL */` |
|      ! 0 |  3088 | `			if( pTos >= pStack ){` |
|      ! 0 |  3089 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3090 | `			}else{` |
|        - |  3091 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3092 | `				pTos++;` |
|      ! 0 |  3093 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3094 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3095 | `			}` |
|        - |  3096 | `			/* Emit a notice */` |
|      ! 0 |  3097 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3098 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3099 | `			break;` |
|        - |  3100 | `		}` |
|      ! 0 |  3101 | `	}else{` |
|   427644 |  3102 | `		pIdx = pTos;` |
|   427644 |  3103 | `		pTos--;` |
|        - |  3104 | `	}` |
|   427644 |  3105 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3106 | `		/* String access */` |
|   346124 |  3107 | `		if( pIdx ){` |
|        - |  3108 | `			sxu32 nOfft;` |
|   346124 |  3109 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3110 | `				/* Force an int cast */` |
|      ! 0 |  3111 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3112 | `			}` |
|   346124 |  3113 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   346124 |  3114 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3115 | `				/* Invalid offset,load null */` |
|      ! 0 |  3116 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3117 | `			}else{` |
|   346124 |  3118 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   346124 |  3119 | `				int c = zData[nOfft];` |
|   346124 |  3120 | `				PH7_MemObjRelease(pTos);` |
|   346124 |  3121 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   346124 |  3122 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3123 | `			}` |
|   173085 |  3124 | `		}else{` |
|        - |  3125 | `			/* No available index,load NULL */` |
|      ! 0 |  3126 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3127 | `		}` |
|   346124 |  3128 | `		break;` |
|        - |  3129 | `	}` |
|    81522 |  3130 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3131 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3132 | `			ph7_value *pObj;` |
|      ! 0 |  3133 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3134 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3135 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3136 | `			}` |
|      ! 0 |  3137 | `		}` |
|      ! 0 |  3138 | `	}` |
|    81522 |  3139 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    81522 |  3140 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3141 | `		/* Point to the hashmap */` |
|    81522 |  3142 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    81522 |  3143 | `		if( pIdx ){` |
|        - |  3144 | `			/* Load the desired entry */` |
|    81522 |  3145 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    40760 |  3146 | `		}` |
|    81522 |  3147 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3148 | `			/* Create a new empty entry */` |
|      ! 0 |  3149 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3150 | `			if( rc == SXRET_OK ){` |
|        - |  3151 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3152 | `				pNode = pMap->pLast;` |
|      ! 0 |  3153 | `			}` |
|      ! 0 |  3154 | `		}` |
|    40760 |  3155 | `	}` |
|    81522 |  3156 | `	if( pIdx ){` |
|    81522 |  3157 | `		PH7_MemObjRelease(pIdx);` |
|    40760 |  3158 | `	}` |
|    81522 |  3159 | `	if( rc == SXRET_OK ){` |
|        - |  3160 | `		/* Load entry contents */` |
|    37674 |  3161 | `		if( pMap->iRef < 2 ){` |
|        - |  3162 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3163 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3164 | `			 */` |
|        7 |  3165 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3166 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3167 | `		}else{` |
|    37668 |  3168 | `			pTos->nIdx = pNode->nValIdx;` |
|    37668 |  3169 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    37668 |  3170 | `			PH7_HashmapUnref(pMap);` |
|        - |  3171 | `		}` |
|    18838 |  3172 | `	}else{` |
|        - |  3173 | `		/* No such entry,load NULL */` |
|    43850 |  3174 | `		PH7_MemObjRelease(pTos);` |
|    43850 |  3175 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3176 | `	}` |
|    81522 |  3177 | `	break;` |
|        - |  3178 | `					  }` |
|        - |  3179 | `/*` |
|        - |  3180 | ` * LOAD_CLOSURE * * P3` |
|        - |  3181 | ` *` |
|        - |  3182 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3183 | ` * name in the stack.` |
|        - |  3184 | ` */` |
|        2 |  3185 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3186 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3187 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3188 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3189 | `		ph7_vm_func *pClosure;` |
|        - |  3190 | `		char *zName;` |
|        - |  3191 | `		sxu32 mLen;` |
|        - |  3192 | `		sxu32 n;` |
|        - |  3193 | `		/* Create a new VM function */` |
|        5 |  3194 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3195 | `		/* Generate an unique closure name */` |
|        5 |  3196 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3197 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3198 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3199 | `			goto Abort;` |
|        - |  3200 | `		}` |
|        5 |  3201 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3202 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3203 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3204 | `		}` |
|        - |  3205 | `		/* Zero the stucture */` |
|        5 |  3206 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3207 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3208 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3209 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3210 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3211 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3212 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3213 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3214 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3215 | `		/* Register the closure */` |
|        5 |  3216 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3217 | `		/* Set up closure environment */` |
|        5 |  3218 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3219 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3220 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3221 | `			ph7_value *pValue;` |
|        9 |  3222 | `			pEnv = &aEnv[n];` |
|        9 |  3223 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3224 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3225 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3226 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3227 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3228 | `				/* Pass by reference */` |
|      ! 0 |  3229 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3230 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3231 | `					);` |
|      ! 0 |  3232 | `			}` |
|        - |  3233 | `			/* Standard pass by value */` |
|        9 |  3234 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3235 | `			if( pValue ){` |
|        - |  3236 | `				/* Copy imported value */` |
|        5 |  3237 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3238 | `			}` |
|        - |  3239 | `			/* Insert the imported variable */` |
|        9 |  3240 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3241 | `		}` |
|        - |  3242 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3243 | `		pTos++;` |
|        5 |  3244 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3245 | `	}` |
|        5 |  3246 | `	break;` |
|        - |  3247 | `						 }` |
|        - |  3248 | `/*` |
|        - |  3249 | ` * STORE * P2 P3` |
|        - |  3250 | ` *` |
|        - |  3251 | ` * Perform a store (Assignment) operation.` |
|        - |  3252 | ` */` |
|   100230 |  3253 | `case PH7_OP_STORE: {` |
|        - |  3254 | `	ph7_value *pObj;` |
|        - |  3255 | `	SyString sName;` |
|        - |  3256 | `#ifdef UNTRUST` |
|        - |  3257 | `	if( pTos < pStack ){` |
|        - |  3258 | `		goto Abort;` |
|        - |  3259 | `	}` |
|        - |  3260 | `#endif` |
|   200462 |  3261 | `	if( pInstr->iP2 ){` |
|        - |  3262 | `		sxu32 nIdx;` |
|        - |  3263 | `		/* Member store operation */` |
|     2258 |  3264 | `		nIdx = pTos->nIdx;` |
|     2258 |  3265 | `		VmPopOperand(&pTos,1);` |
|     2258 |  3266 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3267 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3268 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3269 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3270 | `		}else{` |
|        - |  3271 | `			/* Point to the desired memory object */` |
|     2254 |  3272 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2254 |  3273 | `			if( pObj ){` |
|        - |  3274 | `				/* Perform the store operation */` |
|     2254 |  3275 | `				PH7_MemObjStore(pTos,pObj);` |
|     1126 |  3276 | `			}` |
|        - |  3277 | `		}` |
|   101360 |  3278 | `		break;` |
|   198206 |  3279 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3280 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3281 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3282 | `			/* Force a string cast */` |
|      ! 0 |  3283 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3284 | `		}` |
|        7 |  3285 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3286 | `		pTos--;` |
|        - |  3287 | `#ifdef UNTRUST` |
|        - |  3288 | `		if( pTos < pStack  ){` |
|        - |  3289 | `			goto Abort;` |
|        - |  3290 | `		}` |
|        - |  3291 | `#endif` |
|        4 |  3292 | `	}else{` |
|   198200 |  3293 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3294 | `	}` |
|        - |  3295 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   198206 |  3296 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   198206 |  3297 | `	if( pObj == 0 ){` |
|      ! 0 |  3298 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3299 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3300 | `		goto Abort;` |
|        - |  3301 | `	}` |
|   198206 |  3302 | `	if( !pInstr->p3 ){` |
|        7 |  3303 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3304 | `	}` |
|        - |  3305 | `	/* Perform the store operation */` |
|   198206 |  3306 | `	PH7_MemObjStore(pTos,pObj);` |
|   198206 |  3307 | `	break;` |
|        - |  3308 | `				   }` |
|        - |  3309 | `/*` |
|        - |  3310 | ` * STORE_IDX:   P1 * P3` |
|        - |  3311 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3312 | ` *` |
|        - |  3313 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3314 | ` */` |
|    75936 |  3315 | `case PH7_OP_STORE_IDX:` |
|        - |  3316 | `case PH7_OP_STORE_IDX_REF: {` |
|   151874 |  3317 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3318 | `	ph7_value *pKey;` |
|        - |  3319 | `	sxu32 nIdx;` |
|   151874 |  3320 | `	if( pInstr->iP1 ){` |
|        - |  3321 | `		/* Key is next on stack */` |
|    54948 |  3322 | `		pKey = pTos;` |
|    54948 |  3323 | `		pTos--;` |
|    27475 |  3324 | `	}else{` |
|    96928 |  3325 | `		pKey = 0;` |
|        - |  3326 | `	}` |
|   151874 |  3327 | `	nIdx = pTos->nIdx;` |
|   151874 |  3328 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3329 | `		/* Hashmap already loaded */` |
|   151822 |  3330 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   151822 |  3331 | `		if( pMap->iRef < 2 ){` |
|        - |  3332 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3333 | `			pMap->iRef = 2;` |
|      ! 0 |  3334 | `		}` |
|    75912 |  3335 | `	}else{` |
|        - |  3336 | `		ph7_value *pObj;` |
|       53 |  3337 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3338 | `		if( pObj == 0 ){` |
|      ! 0 |  3339 | `			if( pKey ){` |
|      ! 0 |  3340 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3341 | `			}` |
|      ! 0 |  3342 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3343 | `			break;` |
|        - |  3344 | `		}` |
|        - |  3345 | `		/* Phase#1: Load the array */` |
|       53 |  3346 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3347 | `			VmPopOperand(&pTos,1);` |
|       53 |  3348 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3349 | `				/* Force a string cast */` |
|      ! 0 |  3350 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3351 | `			}` |
|       53 |  3352 | `			if( pKey == 0 ){` |
|        - |  3353 | `				/* Append string */` |
|        3 |  3354 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3355 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3356 | `				}` |
|        2 |  3357 | `			}else{` |
|        - |  3358 | `				sxu32 nOfft;` |
|       51 |  3359 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3360 | `					/* Force an int cast */` |
|       51 |  3361 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3362 | `				}` |
|       51 |  3363 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3364 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3365 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3366 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3367 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3368 | `				}else{` |
|      ! 0 |  3369 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3370 | `						/* Perform an append operation */` |
|      ! 0 |  3371 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3372 | `					}` |
|        - |  3373 | `				}` |
|        - |  3374 | `			}` |
|       53 |  3375 | `			if( pKey ){` |
|       51 |  3376 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3377 | `			}` |
|       53 |  3378 | `			break;` |
|      ! 0 |  3379 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3380 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3381 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3382 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3383 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3384 | `				goto Abort;` |
|        - |  3385 | `			}` |
|      ! 0 |  3386 | `		}` |
|      ! 0 |  3387 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3388 | `	}` |
|   151822 |  3389 | `	VmPopOperand(&pTos,1);` |
|        - |  3390 | `	/* Phase#2: Perform the insertion */` |
|   151822 |  3391 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3392 | `		/* Insertion by reference */` |
|       15 |  3393 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3394 | `	}else{` |
|   151808 |  3395 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3396 | `	}` |
|   151822 |  3397 | `	if( pKey ){` |
|    54898 |  3398 | `		PH7_MemObjRelease(pKey);` |
|    27448 |  3399 | `	}` |
|   151822 |  3400 | `	break;` |
|        - |  3401 | `					   }` |
|        - |  3402 | `/*` |
|        - |  3403 | ` * INCR: P1 * *` |
|        - |  3404 | ` *` |
|        - |  3405 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3406 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3407 | ` * the stack and increment after that.` |
|        - |  3408 | ` */` |
|   155751 |  3409 | `case PH7_OP_INCR:` |
|        - |  3410 | `#ifdef UNTRUST` |
|        - |  3411 | `	if( pTos < pStack ){` |
|        - |  3412 | `		goto Abort;` |
|        - |  3413 | `	}` |
|        - |  3414 | `#endif` |
|   311548 |  3415 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   311548 |  3416 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3417 | `			ph7_value *pObj;` |
|   311548 |  3418 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3419 | `				/* Force a numeric cast */` |
|   311548 |  3420 | `				PH7_MemObjToNumeric(pObj);` |
|   311548 |  3421 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3422 | `					pObj->rVal++;` |
|        - |  3423 | `					/* Try to get an integer representation */` |
|      ! 0 |  3424 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3425 | `				}else{` |
|   311548 |  3426 | `					pObj->x.iVal++;` |
|   311548 |  3427 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3428 | `				}` |
|   311548 |  3429 | `				if( pInstr->iP1 ){` |
|        - |  3430 | `					/* Pre-icrement */` |
|       71 |  3431 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3432 | `				}` |
|   155795 |  3433 | `			}` |
|   155797 |  3434 | `		}else{` |
|      ! 0 |  3435 | `			if( pInstr->iP1 ){` |
|        - |  3436 | `				/* Force a numeric cast */` |
|      ! 0 |  3437 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3438 | `				/* Pre-increment */` |
|      ! 0 |  3439 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3440 | `					pTos->rVal++;` |
|        - |  3441 | `					/* Try to get an integer representation */` |
|      ! 0 |  3442 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3443 | `				}else{` |
|      ! 0 |  3444 | `					pTos->x.iVal++;` |
|      ! 0 |  3445 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3446 | `				}` |
|      ! 0 |  3447 | `			}` |
|        - |  3448 | `		}` |
|   155795 |  3449 | `	}` |
|   311548 |  3450 | `	break;` |
|        - |  3451 | `/*` |
|        - |  3452 | ` * DECR: P1 * *` |
|        - |  3453 | ` *` |
|        - |  3454 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3455 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3456 | ` * and decrement after that.` |
|        - |  3457 | ` */` |
|        2 |  3458 | `case PH7_OP_DECR:` |
|        - |  3459 | `#ifdef UNTRUST` |
|        - |  3460 | `	if( pTos < pStack ){` |
|        - |  3461 | `		goto Abort;` |
|        - |  3462 | `	}` |
|        - |  3463 | `#endif` |
|        5 |  3464 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3465 | `		/* Force a numeric cast */` |
|        5 |  3466 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3467 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3468 | `			ph7_value *pObj;` |
|        5 |  3469 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3470 | `				/* Force a numeric cast */` |
|        5 |  3471 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3472 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3473 | `					pObj->rVal--;` |
|        - |  3474 | `					/* Try to get an integer representation */` |
|      ! 0 |  3475 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3476 | `				}else{` |
|        5 |  3477 | `					pObj->x.iVal--;` |
|        5 |  3478 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3479 | `				}` |
|        5 |  3480 | `				if( pInstr->iP1 ){` |
|        - |  3481 | `					/* Pre-icrement */` |
|      ! 0 |  3482 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3483 | `				}` |
|        2 |  3484 | `			}` |
|        3 |  3485 | `		}else{` |
|      ! 0 |  3486 | `			if( pInstr->iP1 ){` |
|        - |  3487 | `				/* Pre-increment */` |
|      ! 0 |  3488 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3489 | `					pTos->rVal--;` |
|        - |  3490 | `					/* Try to get an integer representation */` |
|      ! 0 |  3491 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3492 | `				}else{` |
|      ! 0 |  3493 | `					pTos->x.iVal--;` |
|      ! 0 |  3494 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3495 | `				}` |
|      ! 0 |  3496 | `			}` |
|        - |  3497 | `		}` |
|        2 |  3498 | `	}` |
|        5 |  3499 | `	break;` |
|        - |  3500 | `/*` |
|        - |  3501 | ` * UMINUS: * * *` |
|        - |  3502 | ` *` |
|        - |  3503 | ` * Perform a unary minus operation.` |
|        - |  3504 | ` */` |
|    21293 |  3505 | `case PH7_OP_UMINUS:` |
|        - |  3506 | `#ifdef UNTRUST` |
|        - |  3507 | `	if( pTos < pStack ){` |
|        - |  3508 | `		goto Abort;` |
|        - |  3509 | `	}` |
|        - |  3510 | `#endif` |
|        - |  3511 | `	/* Force a numeric (integer,real or both) cast */` |
|    42588 |  3512 | `	PH7_MemObjToNumeric(pTos);` |
|    42588 |  3513 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       25 |  3514 | `		pTos->rVal = -pTos->rVal;` |
|       12 |  3515 | `	}` |
|    42588 |  3516 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    42564 |  3517 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    21281 |  3518 | `	}` |
|    42588 |  3519 | `	break;` |
|        - |  3520 | `/*` |
|        - |  3521 | ` * UPLUS: * * *` |
|        - |  3522 | ` *` |
|        - |  3523 | ` * Perform a unary plus operation.` |
|        - |  3524 | ` */` |
|       16 |  3525 | `case PH7_OP_UPLUS:` |
|        - |  3526 | `#ifdef UNTRUST` |
|        - |  3527 | `	if( pTos < pStack ){` |
|        - |  3528 | `		goto Abort;` |
|        - |  3529 | `	}` |
|        - |  3530 | `#endif` |
|        - |  3531 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3532 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3533 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3534 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3535 | `	}` |
|       33 |  3536 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3537 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3538 | `	}` |
|       33 |  3539 | `	break;` |
|        - |  3540 | `/*` |
|        - |  3541 | ` * OP_LNOT: * * *` |
|        - |  3542 | ` *` |
|        - |  3543 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3544 | ` * with its complement.` |
|        - |  3545 | ` */` |
|    46513 |  3546 | `case PH7_OP_LNOT:` |
|        - |  3547 | `#ifdef UNTRUST` |
|        - |  3548 | `	if( pTos < pStack ){` |
|        - |  3549 | `		goto Abort;` |
|        - |  3550 | `	}` |
|        - |  3551 | `#endif` |
|        - |  3552 | `	/* Force a boolean cast */` |
|    93072 |  3553 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3554 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3555 | `	}` |
|    93072 |  3556 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    93072 |  3557 | `	break;` |
|        - |  3558 | `/*` |
|        - |  3559 | ` * OP_BITNOT: * * *` |
|        - |  3560 | ` *` |
|        - |  3561 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3562 | ` * with its ones-complement.` |
|        - |  3563 | ` */` |
|        3 |  3564 | `case PH7_OP_BITNOT:` |
|        - |  3565 | `#ifdef UNTRUST` |
|        - |  3566 | `	if( pTos < pStack ){` |
|        - |  3567 | `		goto Abort;` |
|        - |  3568 | `	}` |
|        - |  3569 | `#endif` |
|        - |  3570 | `	/* Force an integer cast */` |
|        7 |  3571 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3572 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3573 | `	}` |
|        7 |  3574 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|        7 |  3575 | `	break;` |
|        - |  3576 | `/* OP_MUL * * *` |
|        - |  3577 | ` * OP_MUL_STORE * * *` |
|        - |  3578 | ` *` |
|        - |  3579 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3580 | ` * and push the result back onto the stack.` |
|        - |  3581 | ` */` |
|     1234 |  3582 | `case PH7_OP_MUL:` |
|        - |  3583 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3584 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3585 | `	/* Force the operand to be numeric */` |
|        - |  3586 | `#ifdef UNTRUST` |
|        - |  3587 | `	if( pNos < pStack ){` |
|        - |  3588 | `		goto Abort;` |
|        - |  3589 | `	}` |
|        - |  3590 | `#endif` |
|     2470 |  3591 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3592 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3593 | `	/* Perform the requested operation */` |
|     2470 |  3594 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3595 | `		/* Floating point arithemic */` |
|        - |  3596 | `		ph7_real a,b,r;` |
|       17 |  3597 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3598 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3599 | `		}` |
|       17 |  3600 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3601 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3602 | `		}` |
|       17 |  3603 | `		a = pNos->rVal;` |
|       17 |  3604 | `		b = pTos->rVal;` |
|       17 |  3605 | `		r = a * b;` |
|        - |  3606 | `		/* Push the result */` |
|       17 |  3607 | `		pNos->rVal = r;` |
|       17 |  3608 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3609 | `		/* Try to get an integer representation */` |
|       17 |  3610 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3611 | `	}else{` |
|        - |  3612 | `		/* Integer arithmetic */` |
|        - |  3613 | `		sxi64 a,b,r;` |
|     2454 |  3614 | `		a = pNos->x.iVal;` |
|     2454 |  3615 | `		b = pTos->x.iVal;` |
|     2454 |  3616 | `		r = a * b;` |
|        - |  3617 | `		/* Push the result */` |
|     2454 |  3618 | `		pNos->x.iVal = r;` |
|     2454 |  3619 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3620 | `	}` |
|     2470 |  3621 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3622 | `		ph7_value *pObj;` |
|       19 |  3623 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3624 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3625 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3626 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3627 | `		}` |
|        9 |  3628 | `	}` |
|     2470 |  3629 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3630 | `	break;` |
|        - |  3631 | `				 }` |
|        - |  3632 | `/* OP_ADD * * *` |
|        - |  3633 | ` *` |
|        - |  3634 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3635 | ` * and push the result back onto the stack.` |
|        - |  3636 | ` */` |
|      425 |  3637 | `case PH7_OP_ADD:{` |
|      852 |  3638 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3639 | `#ifdef UNTRUST` |
|        - |  3640 | `	if( pNos < pStack ){` |
|        - |  3641 | `		goto Abort;` |
|        - |  3642 | `	}` |
|        - |  3643 | `#endif` |
|        - |  3644 | `	/* Perform the addition */` |
|      852 |  3645 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      852 |  3646 | `	VmPopOperand(&pTos,1);` |
|      852 |  3647 | `	break;` |
|        - |  3648 | `				}` |
|        - |  3649 | `/*` |
|        - |  3650 | ` * OP_ADD_STORE * * *` |
|        - |  3651 | ` *` |
|        - |  3652 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3653 | ` * and push the result back onto the stack.` |
|        - |  3654 | ` */` |
|      481 |  3655 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3656 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3657 | `	ph7_value *pObj;` |
|        - |  3658 | `	sxu32 nIdx;` |
|        - |  3659 | `#ifdef UNTRUST` |
|        - |  3660 | `	if( pNos < pStack ){` |
|        - |  3661 | `		goto Abort;` |
|        - |  3662 | `	}` |
|        - |  3663 | `#endif` |
|        - |  3664 | `	/* Perform the addition */` |
|      963 |  3665 | `	nIdx = pTos->nIdx;` |
|      963 |  3666 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3667 | `	/* Peform the store operation */` |
|      963 |  3668 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3669 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3670 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3671 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3672 | `	}` |
|        - |  3673 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3674 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3675 | `	VmPopOperand(&pTos,1);` |
|      963 |  3676 | `	break;` |
|        - |  3677 | `				}` |
|        - |  3678 | `/* OP_SUB * * *` |
|        - |  3679 | ` *` |
|        - |  3680 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3681 | ` * first (what was next on the stack) from the second (the` |
|        - |  3682 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3683 | ` */` |
|      294 |  3684 | `case PH7_OP_SUB: {` |
|      589 |  3685 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3686 | `#ifdef UNTRUST` |
|        - |  3687 | `	if( pNos < pStack ){` |
|        - |  3688 | `		goto Abort;` |
|        - |  3689 | `	}` |
|        - |  3690 | `#endif` |
|      589 |  3691 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3692 | `		/* Floating point arithemic */` |
|        - |  3693 | `		ph7_real a,b,r;` |
|       95 |  3694 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3695 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3696 | `		}` |
|       95 |  3697 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3698 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3699 | `		}` |
|       95 |  3700 | `		a = pNos->rVal;` |
|       95 |  3701 | `		b = pTos->rVal;` |
|       95 |  3702 | `		r = a - b;` |
|        - |  3703 | `		/* Push the result */` |
|       95 |  3704 | `		pNos->rVal = r;` |
|       95 |  3705 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3706 | `		/* Try to get an integer representation */` |
|       95 |  3707 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3708 | `	}else{` |
|        - |  3709 | `		/* Integer arithmetic */` |
|        - |  3710 | `		sxi64 a,b,r;` |
|      495 |  3711 | `		a = pNos->x.iVal;` |
|      495 |  3712 | `		b = pTos->x.iVal;` |
|      495 |  3713 | `		r = a - b;` |
|        - |  3714 | `		/* Push the result */` |
|      495 |  3715 | `		pNos->x.iVal = r;` |
|      495 |  3716 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3717 | `	}` |
|      589 |  3718 | `	VmPopOperand(&pTos,1);` |
|      589 |  3719 | `	break;` |
|        - |  3720 | `				 }` |
|        - |  3721 | `/* OP_SUB_STORE * * *` |
|        - |  3722 | ` *` |
|        - |  3723 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3724 | ` * first (what was next on the stack) from the second (the` |
|        - |  3725 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3726 | ` */` |
|        1 |  3727 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3728 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3729 | `	ph7_value *pObj;` |
|        - |  3730 | `#ifdef UNTRUST` |
|        - |  3731 | `	if( pNos < pStack ){` |
|        - |  3732 | `		goto Abort;` |
|        - |  3733 | `	}` |
|        - |  3734 | `#endif` |
|        3 |  3735 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3736 | `		/* Floating point arithemic */` |
|        - |  3737 | `		ph7_real a,b,r;` |
|      ! 0 |  3738 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3739 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3740 | `		}` |
|      ! 0 |  3741 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3742 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3743 | `		}` |
|      ! 0 |  3744 | `		a = pTos->rVal;` |
|      ! 0 |  3745 | `		b = pNos->rVal;` |
|      ! 0 |  3746 | `		r = a - b;` |
|        - |  3747 | `		/* Push the result */` |
|      ! 0 |  3748 | `		pNos->rVal = r;` |
|      ! 0 |  3749 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3750 | `		/* Try to get an integer representation */` |
|      ! 0 |  3751 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3752 | `	}else{` |
|        - |  3753 | `		/* Integer arithmetic */` |
|        - |  3754 | `		sxi64 a,b,r;` |
|        3 |  3755 | `		a = pTos->x.iVal;` |
|        3 |  3756 | `		b = pNos->x.iVal;` |
|        3 |  3757 | `		r = a - b;` |
|        - |  3758 | `		/* Push the result */` |
|        3 |  3759 | `		pNos->x.iVal = r;` |
|        3 |  3760 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3761 | `	}` |
|        3 |  3762 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3763 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3764 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3765 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3766 | `	}` |
|        3 |  3767 | `	VmPopOperand(&pTos,1);` |
|        3 |  3768 | `	break;` |
|        - |  3769 | `				 }` |
|        - |  3770 |  |
|        - |  3771 | `/*` |
|        - |  3772 | ` * OP_MOD * * *` |
|        - |  3773 | ` *` |
|        - |  3774 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3775 | ` * first (what was next on the stack) from the second (the` |
|        - |  3776 | ` * top of the stack) and push the remainder after division` |
|        - |  3777 | ` * onto the stack.` |
|        - |  3778 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3779 | ` */` |
|      296 |  3780 | `case PH7_OP_MOD:{` |
|      594 |  3781 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3782 | `	sxi64 a,b,r;` |
|        - |  3783 | `#ifdef UNTRUST` |
|        - |  3784 | `	if( pNos < pStack ){` |
|        - |  3785 | `		goto Abort;` |
|        - |  3786 | `	}` |
|        - |  3787 | `#endif` |
|        - |  3788 | `	/* Force the operands to be integer */` |
|      594 |  3789 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3790 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3791 | `	}` |
|      594 |  3792 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3793 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3794 | `	}` |
|        - |  3795 | `	/* Perform the requested operation */` |
|      594 |  3796 | `	a = pNos->x.iVal;` |
|      594 |  3797 | `	b = pTos->x.iVal;` |
|      594 |  3798 | `	if( b == 0 ){` |
|        3 |  3799 | `		r = 0;` |
|        3 |  3800 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3801 | `		/* goto Abort; */` |
|        2 |  3802 | `	}else{` |
|      591 |  3803 | `		r = a%b;` |
|        - |  3804 | `	}` |
|        - |  3805 | `	/* Push the result */` |
|      594 |  3806 | `	pNos->x.iVal = r;` |
|      594 |  3807 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3808 | `	VmPopOperand(&pTos,1);` |
|      594 |  3809 | `	break;` |
|        - |  3810 | `				}` |
|        - |  3811 | `/*` |
|        - |  3812 | ` * OP_MOD_STORE * * *` |
|        - |  3813 | ` *` |
|        - |  3814 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3815 | ` * first (what was next on the stack) from the second (the` |
|        - |  3816 | ` * top of the stack) and push the remainder after division` |
|        - |  3817 | ` * onto the stack.` |
|        - |  3818 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3819 | ` */` |
|        1 |  3820 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3821 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3822 | `	ph7_value *pObj;` |
|        - |  3823 | `	sxi64 a,b,r;` |
|        - |  3824 | `#ifdef UNTRUST` |
|        - |  3825 | `	if( pNos < pStack ){` |
|        - |  3826 | `		goto Abort;` |
|        - |  3827 | `	}` |
|        - |  3828 | `#endif` |
|        - |  3829 | `	/* Force the operands to be integer */` |
|        3 |  3830 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3831 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3832 | `	}` |
|        3 |  3833 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3834 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3835 | `	}` |
|        - |  3836 | `	/* Perform the requested operation */` |
|        3 |  3837 | `	a = pTos->x.iVal;` |
|        3 |  3838 | `	b = pNos->x.iVal;` |
|        3 |  3839 | `	if( b == 0 ){` |
|      ! 0 |  3840 | `		r = 0;` |
|      ! 0 |  3841 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3842 | `		/* goto Abort; */` |
|      ! 0 |  3843 | `	}else{` |
|        3 |  3844 | `		r = a%b;` |
|        - |  3845 | `	}` |
|        - |  3846 | `	/* Push the result */` |
|        3 |  3847 | `	pNos->x.iVal = r;` |
|        3 |  3848 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3849 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3850 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3851 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3852 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3853 | `	}` |
|        3 |  3854 | `	VmPopOperand(&pTos,1);` |
|        3 |  3855 | `	break;` |
|        - |  3856 | `				}` |
|        - |  3857 | `/*` |
|        - |  3858 | ` * OP_DIV * * *` |
|        - |  3859 | ` *` |
|        - |  3860 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3861 | ` * first (what was next on the stack) from the second (the` |
|        - |  3862 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3863 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3864 | ` */` |
|       28 |  3865 | `case PH7_OP_DIV:{` |
|       58 |  3866 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3867 | `	ph7_real a,b,r;` |
|        - |  3868 | `#ifdef UNTRUST` |
|        - |  3869 | `	if( pNos < pStack ){` |
|        - |  3870 | `		goto Abort;` |
|        - |  3871 | `	}` |
|        - |  3872 | `#endif` |
|        - |  3873 | `	/* Force the operands to be real */` |
|       58 |  3874 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3875 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3876 | `	}` |
|       58 |  3877 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3878 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3879 | `	}` |
|        - |  3880 | `	/* Perform the requested operation */` |
|       58 |  3881 | `	a = pNos->rVal;` |
|       58 |  3882 | `	b = pTos->rVal;` |
|       58 |  3883 | `	if( b == 0 ){` |
|        - |  3884 | `		/* Division by zero */` |
|        3 |  3885 | `		pNos->rVal = 0;` |
|        3 |  3886 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3887 | `		/* goto Abort; */` |
|        2 |  3888 | `	}else{` |
|       55 |  3889 | `		r = a/b;` |
|        - |  3890 | `		/* Push the result */` |
|       55 |  3891 | `		pNos->rVal = r;` |
|       55 |  3892 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3893 | `		/* Try to get an integer representation */` |
|       55 |  3894 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3895 | `	}` |
|       58 |  3896 | `	VmPopOperand(&pTos,1);` |
|       58 |  3897 | `	break;` |
|        - |  3898 | `				}` |
|        - |  3899 | `/*` |
|        - |  3900 | ` * OP_DIV_STORE * * *` |
|        - |  3901 | ` *` |
|        - |  3902 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3903 | ` * first (what was next on the stack) from the second (the` |
|        - |  3904 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3905 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3906 | ` */` |
|        1 |  3907 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3908 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3909 | `	ph7_value *pObj;` |
|        - |  3910 | `	ph7_real a,b,r;` |
|        - |  3911 | `#ifdef UNTRUST` |
|        - |  3912 | `	if( pNos < pStack ){` |
|        - |  3913 | `		goto Abort;` |
|        - |  3914 | `	}` |
|        - |  3915 | `#endif` |
|        - |  3916 | `	/* Force the operands to be real */` |
|        3 |  3917 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3918 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3919 | `	}` |
|        3 |  3920 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3921 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3922 | `	}` |
|        - |  3923 | `	/* Perform the requested operation */` |
|        3 |  3924 | `	a = pTos->rVal;` |
|        3 |  3925 | `	b = pNos->rVal;` |
|        3 |  3926 | `	if( b == 0 ){` |
|        - |  3927 | `		/* Division by zero */` |
|      ! 0 |  3928 | `		r = 0;` |
|      ! 0 |  3929 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3930 | `		/* goto Abort; */` |
|      ! 0 |  3931 | `	}else{` |
|        3 |  3932 | `		r = a/b;` |
|        - |  3933 | `		/* Push the result */` |
|        3 |  3934 | `		pNos->rVal = r;` |
|        3 |  3935 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3936 | `		/* Try to get an integer representation */` |
|        3 |  3937 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3938 | `	}` |
|        3 |  3939 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3940 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3941 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3942 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3943 | `	}` |
|        3 |  3944 | `	VmPopOperand(&pTos,1);` |
|        3 |  3945 | `	break;` |
|        - |  3946 | `				}` |
|        - |  3947 | `/* OP_BAND * * *` |
|        - |  3948 | ` *` |
|        - |  3949 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3950 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3951 | ` * two elements.` |
|        - |  3952 | `*/` |
|        - |  3953 | `/* OP_BOR * * *` |
|        - |  3954 | ` *` |
|        - |  3955 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3956 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3957 | ` * two elements.` |
|        - |  3958 | ` */` |
|        - |  3959 | `/* OP_BXOR * * *` |
|        - |  3960 | ` *` |
|        - |  3961 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3962 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3963 | ` * two elements.` |
|        - |  3964 | ` */` |
|       19 |  3965 | `case PH7_OP_BAND:` |
|        - |  3966 | `case PH7_OP_BOR:` |
|        - |  3967 | `case PH7_OP_BXOR:{` |
|       39 |  3968 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3969 | `	sxi64 a,b,r;` |
|        - |  3970 | `#ifdef UNTRUST` |
|        - |  3971 | `	if( pNos < pStack ){` |
|        - |  3972 | `		goto Abort;` |
|        - |  3973 | `	}` |
|        - |  3974 | `#endif` |
|        - |  3975 | `	/* Force the operands to be integer */` |
|       39 |  3976 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3977 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3978 | `	}` |
|       39 |  3979 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3980 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3981 | `	}` |
|        - |  3982 | `	/* Perform the requested operation */` |
|       39 |  3983 | `	a = pNos->x.iVal;` |
|       39 |  3984 | `	b = pTos->x.iVal;` |
|       39 |  3985 | `	switch(pInstr->iOp){` |
|        6 |  3986 | `	case PH7_OP_BOR_STORE:` |
|       13 |  3987 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  3988 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  3989 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        7 |  3990 | `	case PH7_OP_BAND_STORE:` |
|        7 |  3991 | `	case PH7_OP_BAND:` |
|       15 |  3992 | `	default:          r = a&b; break;` |
|        - |  3993 | `	}` |
|        - |  3994 | `	/* Push the result */` |
|       39 |  3995 | `	pNos->x.iVal = r;` |
|       39 |  3996 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       39 |  3997 | `	VmPopOperand(&pTos,1);` |
|       39 |  3998 | `	break;` |
|        - |  3999 | `				 }` |
|        - |  4000 | `/* OP_BAND_STORE * * *` |
|        - |  4001 | ` *` |
|        - |  4002 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4003 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4004 | ` * two elements.` |
|        - |  4005 | `*/` |
|        - |  4006 | `/* OP_BOR_STORE * * *` |
|        - |  4007 | ` *` |
|        - |  4008 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4009 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4010 | ` * two elements.` |
|        - |  4011 | ` */` |
|        - |  4012 | `/* OP_BXOR_STORE * * *` |
|        - |  4013 | ` *` |
|        - |  4014 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4015 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4016 | ` * two elements.` |
|        - |  4017 | ` */` |
|        7 |  4018 | `case PH7_OP_BAND_STORE:` |
|        - |  4019 | `case PH7_OP_BOR_STORE:` |
|        - |  4020 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4021 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4022 | `	ph7_value *pObj;` |
|        - |  4023 | `	sxi64 a,b,r;` |
|        - |  4024 | `#ifdef UNTRUST` |
|        - |  4025 | `	if( pNos < pStack ){` |
|        - |  4026 | `		goto Abort;` |
|        - |  4027 | `	}` |
|        - |  4028 | `#endif` |
|        - |  4029 | `	/* Force the operands to be integer */` |
|       15 |  4030 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4031 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4032 | `	}` |
|       15 |  4033 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4034 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4035 | `	}` |
|        - |  4036 | `	/* Perform the requested operation */` |
|       15 |  4037 | `	a = pTos->x.iVal;` |
|       15 |  4038 | `	b = pNos->x.iVal;` |
|       15 |  4039 | `	switch(pInstr->iOp){` |
|        2 |  4040 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4041 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4042 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4043 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4044 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4045 | `	case PH7_OP_BAND:` |
|        5 |  4046 | `	default:          r = a&b; break;` |
|        - |  4047 | `	}` |
|        - |  4048 | `	/* Push the result */` |
|       15 |  4049 | `	pNos->x.iVal = r;` |
|       15 |  4050 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4051 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4052 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4053 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4054 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4055 | `	}` |
|       15 |  4056 | `	VmPopOperand(&pTos,1);` |
|       15 |  4057 | `	break;` |
|        - |  4058 | `				 }` |
|        - |  4059 | `/* OP_SHL * * *` |
|        - |  4060 | ` *` |
|        - |  4061 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4062 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4063 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4064 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4065 | ` */` |
|        - |  4066 | `/* OP_SHR * * *` |
|        - |  4067 | ` *` |
|        - |  4068 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4069 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4070 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4071 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4072 | ` */` |
|        9 |  4073 | `case PH7_OP_SHL:` |
|        - |  4074 | `case PH7_OP_SHR: {` |
|       19 |  4075 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4076 | `	sxi64 a,r;` |
|        - |  4077 | `	sxi32 b;` |
|        - |  4078 | `#ifdef UNTRUST` |
|        - |  4079 | `	if( pNos < pStack ){` |
|        - |  4080 | `		goto Abort;` |
|        - |  4081 | `	}` |
|        - |  4082 | `#endif` |
|        - |  4083 | `	/* Force the operands to be integer */` |
|       19 |  4084 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4085 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4086 | `	}` |
|       19 |  4087 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4088 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4089 | `	}` |
|        - |  4090 | `	/* Perform the requested operation */` |
|       19 |  4091 | `	a = pNos->x.iVal;` |
|       19 |  4092 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4093 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4094 | `		r = a << b;` |
|        6 |  4095 | `	}else{` |
|        9 |  4096 | `		r = a >> b;` |
|        - |  4097 | `	}` |
|        - |  4098 | `	/* Push the result */` |
|       19 |  4099 | `	pNos->x.iVal = r;` |
|       19 |  4100 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4101 | `	VmPopOperand(&pTos,1);` |
|       19 |  4102 | `	break;` |
|        - |  4103 | `				 }` |
|        - |  4104 | `/*  OP_SHL_STORE * * *` |
|        - |  4105 | ` *` |
|        - |  4106 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4107 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4108 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4109 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4110 | ` */` |
|        - |  4111 | `/* OP_SHR_STORE * * *` |
|        - |  4112 | ` *` |
|        - |  4113 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4114 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4115 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4116 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4117 | ` */` |
|        7 |  4118 | `case PH7_OP_SHL_STORE:` |
|        - |  4119 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4120 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4121 | `	ph7_value *pObj;` |
|        - |  4122 | `	sxi64 a,r;` |
|        - |  4123 | `	sxi32 b;` |
|        - |  4124 | `#ifdef UNTRUST` |
|        - |  4125 | `	if( pNos < pStack ){` |
|        - |  4126 | `		goto Abort;` |
|        - |  4127 | `	}` |
|        - |  4128 | `#endif` |
|        - |  4129 | `	/* Force the operands to be integer */` |
|       15 |  4130 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4131 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4132 | `	}` |
|       15 |  4133 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4134 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4135 | `	}` |
|        - |  4136 | `	/* Perform the requested operation */` |
|       15 |  4137 | `	a = pTos->x.iVal;` |
|       15 |  4138 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4139 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4140 | `		r = a << b;` |
|        4 |  4141 | `	}else{` |
|        9 |  4142 | `		r = a >> b;` |
|        - |  4143 | `	}` |
|        - |  4144 | `	/* Push the result */` |
|       15 |  4145 | `	pNos->x.iVal = r;` |
|       15 |  4146 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4147 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4148 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4149 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4150 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4151 | `	}` |
|       15 |  4152 | `	VmPopOperand(&pTos,1);` |
|       15 |  4153 | `	break;` |
|        - |  4154 | `				 }` |
|        - |  4155 | `/* CAT:  P1 * *` |
|        - |  4156 | ` *` |
|        - |  4157 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4158 | ` * back.` |
|        - |  4159 | ` */` |
|    57423 |  4160 | `case PH7_OP_CAT:{` |
|        - |  4161 | `	ph7_value *pNos,*pCur;` |
|   114848 |  4162 | `	if( pInstr->iP1 < 1 ){` |
|    88076 |  4163 | `		pNos = &pTos[-1];` |
|    44039 |  4164 | `	}else{` |
|    26774 |  4165 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4166 | `	}` |
|        - |  4167 | `#ifdef UNTRUST` |
|        - |  4168 | `	if( pNos < pStack ){` |
|        - |  4169 | `		goto Abort;` |
|        - |  4170 | `	}` |
|        - |  4171 | `#endif` |
|        - |  4172 | `	/* Force a string cast */` |
|   114848 |  4173 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      884 |  4174 | `		PH7_MemObjToString(pNos);` |
|      441 |  4175 | `	}` |
|   114848 |  4176 | `	pCur = &pNos[1];` |
|   231314 |  4177 | `	while( pCur <= pTos ){` |
|   116468 |  4178 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50364 |  4179 | `			PH7_MemObjToString(pCur);` |
|    25181 |  4180 | `		}` |
|        - |  4181 | `		/* Perform the concatenation */` |
|   116468 |  4182 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   116430 |  4183 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    58214 |  4184 | `		}` |
|   116468 |  4185 | `		SyBlobRelease(&pCur->sBlob);` |
|   116468 |  4186 | `		pCur++;` |
|        2 |  4187 | `	}` |
|   114848 |  4188 | `	pTos = pNos;` |
|   114848 |  4189 | `	break;` |
|        - |  4190 | `				}` |
|        - |  4191 | `/*  CAT_STORE: * * *` |
|        - |  4192 | ` *` |
|        - |  4193 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4194 | ` * back.` |
|        - |  4195 | ` */` |
|     2404 |  4196 | `case PH7_OP_CAT_STORE:{` |
|     4810 |  4197 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4198 | `	ph7_value *pObj;` |
|        - |  4199 | `#ifdef UNTRUST` |
|        - |  4200 | `	if( pNos < pStack ){` |
|        - |  4201 | `		goto Abort;` |
|        - |  4202 | `	}` |
|        - |  4203 | `#endif` |
|     4810 |  4204 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4205 | `		/* Force a string cast */` |
|      ! 0 |  4206 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4207 | `	}` |
|     4810 |  4208 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4209 | `		/* Force a string cast */` |
|      ! 0 |  4210 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4211 | `	}` |
|        - |  4212 | `	/* Perform the concatenation (Reverse order) */` |
|     4810 |  4213 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     4810 |  4214 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2404 |  4215 | `	}` |
|        - |  4216 | `	/* Perform the store operation */` |
|     4810 |  4217 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4218 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     4810 |  4219 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     4810 |  4220 | `		PH7_MemObjStore(pTos,pObj);` |
|     2404 |  4221 | `	}` |
|     4810 |  4222 | `	PH7_MemObjStore(pTos,pNos);` |
|     4810 |  4223 | `	VmPopOperand(&pTos,1);` |
|     4810 |  4224 | `	break;` |
|        - |  4225 | `				}` |
|        - |  4226 | `/* OP_AND: * * *` |
|        - |  4227 | ` *` |
|        - |  4228 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4229 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4230 | ` * stack.` |
|        - |  4231 | ` */` |
|        - |  4232 | `/* OP_OR: * * *` |
|        - |  4233 | ` *` |
|        - |  4234 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4235 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4236 | ` * stack.` |
|        - |  4237 | ` */` |
|    99146 |  4238 | `case PH7_OP_LAND:` |
|        - |  4239 | `case PH7_OP_LOR: {` |
|   198338 |  4240 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4241 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4242 | `#ifdef UNTRUST` |
|        - |  4243 | `	if( pNos < pStack ){` |
|        - |  4244 | `		goto Abort;` |
|        - |  4245 | `	}` |
|        - |  4246 | `#endif` |
|        - |  4247 | `	/* Force a boolean cast */` |
|   198338 |  4248 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4249 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4250 | `	}` |
|   198338 |  4251 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4252 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4253 | `	}` |
|   198338 |  4254 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   198338 |  4255 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   198338 |  4256 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4257 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   100230 |  4258 | `		v1 = and_logic[v1*3+v2];` |
|    50138 |  4259 | `	}else{` |
|        - |  4260 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    98110 |  4261 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4262 | `	}` |
|   198338 |  4263 | `	if( v1 == 2 ){` |
|      ! 0 |  4264 | `		v1 = 1;` |
|      ! 0 |  4265 | `	}` |
|   198338 |  4266 | `	VmPopOperand(&pTos,1);` |
|   198338 |  4267 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   198338 |  4268 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   198338 |  4269 | `	break;` |
|        - |  4270 | `				 }` |
|        - |  4271 | `/* OP_LXOR: * * *` |
|        - |  4272 | ` *` |
|        - |  4273 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4274 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4275 | ` * stack.` |
|        - |  4276 | ` * According to the PHP language reference manual:` |
|        - |  4277 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4278 | ` *  TRUE,but not both.` |
|        - |  4279 | ` */` |
|        5 |  4280 | `case PH7_OP_LXOR:{` |
|       11 |  4281 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4282 | `	sxi32 v = 0;` |
|        - |  4283 | `#ifdef UNTRUST` |
|        - |  4284 | `	if( pNos < pStack ){` |
|        - |  4285 | `		goto Abort;` |
|        - |  4286 | `	}` |
|        - |  4287 | `#endif` |
|        - |  4288 | `	/* Force a boolean cast */` |
|       11 |  4289 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4290 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4291 | `	}` |
|       11 |  4292 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4293 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4294 | `	}` |
|       11 |  4295 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4296 | `		v = 1;` |
|        3 |  4297 | `	}` |
|       11 |  4298 | `	VmPopOperand(&pTos,1);` |
|       11 |  4299 | `	pTos->x.iVal = v;` |
|       11 |  4300 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4301 | `	break;` |
|        - |  4302 | `				 }` |
|        - |  4303 | `/* OP_EQ P1 P2 P3` |
|        - |  4304 | ` *` |
|        - |  4305 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4306 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4307 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4308 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4309 | ` */` |
|        - |  4310 | `/* OP_NEQ P1 P2 P3` |
|        - |  4311 | ` *` |
|        - |  4312 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4313 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4314 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4315 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4316 | ` */` |
|     3597 |  4317 | `case PH7_OP_EQ:` |
|        - |  4318 | `case PH7_OP_NEQ: {` |
|     7196 |  4319 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4320 | `	/* Perform the comparison and act accordingly */` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pNos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|     7196 |  4326 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7196 |  4327 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       11 |  4328 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7191 |  4329 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7160 |  4330 | `		rc = rc == 0;` |
|     3581 |  4331 | `	}else{` |
|       28 |  4332 | `		rc = rc != 0;` |
|        - |  4333 | `	}` |
|     7196 |  4334 | `	VmPopOperand(&pTos,1);` |
|     7196 |  4335 | `	if( !pInstr->iP2 ){` |
|        - |  4336 | `		/* Push comparison result without taking the jump */` |
|     7196 |  4337 | `		PH7_MemObjRelease(pTos);` |
|     7196 |  4338 | `		pTos->x.iVal = rc;` |
|        - |  4339 | `		/* Invalidate any prior representation */` |
|     7196 |  4340 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3599 |  4341 | `	}else{` |
|      ! 0 |  4342 | `		if( rc ){` |
|        - |  4343 | `			/* Jump to the desired location */` |
|      ! 0 |  4344 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4345 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4346 | `		}` |
|        - |  4347 | `	}` |
|     7196 |  4348 | `	break;` |
|        - |  4349 | `				 }` |
|        - |  4350 | `/* OP_TEQ P1 P2 *` |
|        - |  4351 | ` *` |
|        - |  4352 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4353 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4354 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4355 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4356 | ` */` |
|   119636 |  4357 | `case PH7_OP_TEQ: {` |
|   239274 |  4358 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4359 | `	/* Perform the comparison and act accordingly */` |
|        - |  4360 | `#ifdef UNTRUST` |
|        - |  4361 | `	if( pNos < pStack ){` |
|        - |  4362 | `		goto Abort;` |
|        - |  4363 | `	}` |
|        - |  4364 | `#endif` |
|   239274 |  4365 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   239274 |  4366 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4367 | `		rc = 0;` |
|        2 |  4368 | `	}else{` |
|   239272 |  4369 | `		rc = rc == 0;` |
|        - |  4370 | `	}` |
|   239274 |  4371 | `	VmPopOperand(&pTos,1);` |
|   239274 |  4372 | `	if( !pInstr->iP2 ){` |
|        - |  4373 | `		/* Push comparison result without taking the jump */` |
|   239274 |  4374 | `		PH7_MemObjRelease(pTos);` |
|   239274 |  4375 | `		pTos->x.iVal = rc;` |
|        - |  4376 | `		/* Invalidate any prior representation */` |
|   239274 |  4377 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   119638 |  4378 | `	}else{` |
|      ! 0 |  4379 | `		if( rc ){` |
|        - |  4380 | `			/* Jump to the desired location */` |
|      ! 0 |  4381 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4382 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4383 | `		}` |
|        - |  4384 | `	}` |
|   239274 |  4385 | `	break;` |
|        - |  4386 | `				 }` |
|        - |  4387 | `/* OP_TNE P1 P2 *` |
|        - |  4388 | ` *` |
|        - |  4389 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4390 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4391 | ` * instruction.` |
|        - |  4392 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4393 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4394 | ` *` |
|        - |  4395 | ` */` |
|    95057 |  4396 | `case PH7_OP_TNE: {` |
|   190116 |  4397 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4398 | `	/* Perform the comparison and act accordingly */` |
|        - |  4399 | `#ifdef UNTRUST` |
|        - |  4400 | `	if( pNos < pStack ){` |
|        - |  4401 | `		goto Abort;` |
|        - |  4402 | `	}` |
|        - |  4403 | `#endif` |
|   190116 |  4404 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   190116 |  4405 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4406 | `		rc = 1;` |
|        2 |  4407 | `	}else{` |
|   190114 |  4408 | `		rc = rc != 0;` |
|        - |  4409 | `	}` |
|   190116 |  4410 | `	VmPopOperand(&pTos,1);` |
|   190116 |  4411 | `	if( !pInstr->iP2 ){` |
|        - |  4412 | `		/* Push comparison result without taking the jump */` |
|   190116 |  4413 | `		PH7_MemObjRelease(pTos);` |
|   190116 |  4414 | `		pTos->x.iVal = rc;` |
|        - |  4415 | `		/* Invalidate any prior representation */` |
|   190116 |  4416 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    95059 |  4417 | `	}else{` |
|      ! 0 |  4418 | `		if( rc ){` |
|        - |  4419 | `			/* Jump to the desired location */` |
|      ! 0 |  4420 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4421 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4422 | `		}` |
|        - |  4423 | `	}` |
|   190116 |  4424 | `	break;` |
|        - |  4425 | `				 }` |
|        - |  4426 | `/* OP_LT P1 P2 P3` |
|        - |  4427 | ` *` |
|        - |  4428 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4429 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4430 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4431 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4432 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4433 | ` *` |
|        - |  4434 | ` */` |
|        - |  4435 | `/* OP_LE P1 P2 P3` |
|        - |  4436 | ` *` |
|        - |  4437 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4438 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4439 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4440 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4441 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4442 | ` *` |
|        - |  4443 | ` */` |
|   108969 |  4444 | `case PH7_OP_LT:` |
|        - |  4445 | `case PH7_OP_LE: {` |
|   217984 |  4446 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4447 | `	/* Perform the comparison and act accordingly */` |
|        - |  4448 | `#ifdef UNTRUST` |
|        - |  4449 | `	if( pNos < pStack ){` |
|        - |  4450 | `		goto Abort;` |
|        - |  4451 | `	}` |
|        - |  4452 | `#endif` |
|   217984 |  4453 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   217984 |  4454 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4455 | `		rc = 0;` |
|   217980 |  4456 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4457 | `		rc = rc < 1;` |
|      198 |  4458 | `	}else{` |
|   217582 |  4459 | `		rc = rc < 0;` |
|        - |  4460 | `	}` |
|   217984 |  4461 | `	VmPopOperand(&pTos,1);` |
|   217984 |  4462 | `	if( !pInstr->iP2 ){` |
|        - |  4463 | `		/* Push comparison result without taking the jump */` |
|   217984 |  4464 | `		PH7_MemObjRelease(pTos);` |
|   217984 |  4465 | `		pTos->x.iVal = rc;` |
|        - |  4466 | `		/* Invalidate any prior representation */` |
|   217984 |  4467 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   109015 |  4468 | `	}else{` |
|      ! 0 |  4469 | `		if( rc ){` |
|        - |  4470 | `			/* Jump to the desired location */` |
|      ! 0 |  4471 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4472 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4473 | `		}` |
|        - |  4474 | `	}` |
|   217984 |  4475 | `	break;` |
|        - |  4476 | `				}` |
|        - |  4477 | `/* OP_GT P1 P2 P3` |
|        - |  4478 | ` *` |
|        - |  4479 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4480 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4481 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4482 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4483 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4484 | ` *` |
|        - |  4485 | ` */` |
|        - |  4486 | `/* OP_GE P1 P2 P3` |
|        - |  4487 | ` *` |
|        - |  4488 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4489 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4490 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4491 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4492 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4493 | ` *` |
|        - |  4494 | ` */` |
|    46737 |  4495 | `case PH7_OP_GT:` |
|        - |  4496 | `case PH7_OP_GE: {` |
|    93476 |  4497 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4498 | `	/* Perform the comparison and act accordingly */` |
|        - |  4499 | `#ifdef UNTRUST` |
|        - |  4500 | `	if( pNos < pStack ){` |
|        - |  4501 | `		goto Abort;` |
|        - |  4502 | `	}` |
|        - |  4503 | `#endif` |
|    93476 |  4504 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    93476 |  4505 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4506 | `		rc = 0;` |
|    93472 |  4507 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    93320 |  4508 | `		rc = rc >= 0;` |
|    46661 |  4509 | `	}else{` |
|      150 |  4510 | `		rc = rc > 0;` |
|        - |  4511 | `	}` |
|    93476 |  4512 | `	VmPopOperand(&pTos,1);` |
|    93476 |  4513 | `	if( !pInstr->iP2 ){` |
|        - |  4514 | `		/* Push comparison result without taking the jump */` |
|    93476 |  4515 | `		PH7_MemObjRelease(pTos);` |
|    93476 |  4516 | `		pTos->x.iVal = rc;` |
|        - |  4517 | `		/* Invalidate any prior representation */` |
|    93476 |  4518 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    46739 |  4519 | `	}else{` |
|      ! 0 |  4520 | `		if( rc ){` |
|        - |  4521 | `			/* Jump to the desired location */` |
|      ! 0 |  4522 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4523 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4524 | `		}` |
|        - |  4525 | `	}` |
|    93476 |  4526 | `	break;` |
|        - |  4527 | `				}` |
|        - |  4528 | `/* OP_SEQ P1 P2 *` |
|        - |  4529 | ` * Strict string comparison.` |
|        - |  4530 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4531 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4532 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4533 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4534 | ` * use PH7_OP_EQ.` |
|        - |  4535 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4536 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4537 | ` */` |
|        - |  4538 | `/* OP_SNE P1 P2 *` |
|        - |  4539 | ` * Strict string comparison.` |
|        - |  4540 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4541 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4542 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4543 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4544 | ` * use PH7_OP_EQ.` |
|        - |  4545 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4546 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4547 | ` */` |
|       18 |  4548 | `case PH7_OP_SEQ:` |
|        - |  4549 | `case PH7_OP_SNE: {` |
|       38 |  4550 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4551 | `	SyString s1,s2;` |
|        - |  4552 | `	/* Perform the comparison and act accordingly */` |
|        - |  4553 | `#ifdef UNTRUST` |
|        - |  4554 | `	if( pNos < pStack ){` |
|        - |  4555 | `		goto Abort;` |
|        - |  4556 | `	}` |
|        - |  4557 | `#endif` |
|        - |  4558 | `	/* Force a string cast */` |
|       38 |  4559 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4560 | `		PH7_MemObjToString(pTos);` |
|        2 |  4561 | `	}` |
|       38 |  4562 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4563 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4564 | `	}` |
|       38 |  4565 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4566 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4567 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4568 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4569 | `		rc = rc != 0;` |
|      ! 0 |  4570 | `	}else{` |
|       38 |  4571 | `		rc = rc == 0;` |
|        - |  4572 | `	}` |
|       38 |  4573 | `	VmPopOperand(&pTos,1);` |
|       38 |  4574 | `	if( !pInstr->iP2 ){` |
|        - |  4575 | `		/* Push comparison result without taking the jump */` |
|       38 |  4576 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4577 | `		pTos->x.iVal = rc;` |
|        - |  4578 | `		/* Invalidate any prior representation */` |
|       38 |  4579 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4580 | `	}else{` |
|      ! 0 |  4581 | `		if( rc ){` |
|        - |  4582 | `			/* Jump to the desired location */` |
|      ! 0 |  4583 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4584 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4585 | `		}` |
|        - |  4586 | `	}` |
|       38 |  4587 | `	break;` |
|        - |  4588 | `				 }` |
|        - |  4589 | `/*` |
|        - |  4590 | ` * OP_LOAD_REF * * *` |
|        - |  4591 | ` * Push the index of a referenced object on the stack.` |
|        - |  4592 | ` */` |
|       57 |  4593 | `case PH7_OP_LOAD_REF: {` |
|        - |  4594 | `	sxu32 nIdx;` |
|        - |  4595 | `#ifdef UNTRUST` |
|        - |  4596 | `	if( pTos < pStack ){` |
|        - |  4597 | `		goto Abort;` |
|        - |  4598 | `	}` |
|        - |  4599 | `#endif` |
|        - |  4600 | `	/* Extract memory object index */` |
|      115 |  4601 | `	nIdx = pTos->nIdx;` |
|      115 |  4602 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4603 | `		/* Nullify the object */` |
|       95 |  4604 | `		PH7_MemObjRelease(pTos);` |
|        - |  4605 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4606 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4607 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4608 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4609 | `	}` |
|      115 |  4610 | `	break;` |
|        - |  4611 | `					  }` |
|        - |  4612 | `/*` |
|        - |  4613 | ` * OP_STORE_REF * * P3` |
|        - |  4614 | ` * Perform an assignment operation by reference.` |
|        - |  4615 | ` */` |
|       14 |  4616 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4617 | `	 SyString sName = { 0 , 0 };` |
|        - |  4618 | `	 VmFrame *pFrameLocal;` |
|        - |  4619 | `	SyHashEntry *pEntry;` |
|        - |  4620 | `	sxu32 nIdx;` |
|        - |  4621 | `#ifdef UNTRUST` |
|        - |  4622 | `	if( pTos < pStack ){` |
|        - |  4623 | `		goto Abort;` |
|        - |  4624 | `	}` |
|        - |  4625 | `#endif` |
|       30 |  4626 | `	if( pInstr->p3 == 0 ){` |
|        - |  4627 | `		char *zName;` |
|        - |  4628 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4629 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4630 | `			/* Force a string cast */` |
|      ! 0 |  4631 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4632 | `		}` |
|      ! 0 |  4633 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4634 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4635 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4636 | `			if( zName ){` |
|      ! 0 |  4637 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4638 | `			}` |
|      ! 0 |  4639 | `		}` |
|      ! 0 |  4640 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4641 | `		pTos--;` |
|      ! 0 |  4642 | `	}else{` |
|       30 |  4643 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4644 | `	}` |
|       30 |  4645 | `	nIdx = pTos->nIdx;` |
|       30 |  4646 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4647 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4648 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4649 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4650 | `		}else{` |
|        - |  4651 | `			ph7_value *pObj;` |
|        - |  4652 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4653 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4654 | `			if( pObj == 0 ){` |
|      ! 0 |  4655 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4656 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4657 | `				goto Abort;` |
|        - |  4658 | `			}` |
|        - |  4659 | `			/* Perform the store operation */` |
|      ! 0 |  4660 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4661 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4662 | `		}` |
|       30 |  4663 | `	}else if( sName.nByte > 0){` |
|       30 |  4664 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4665 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4666 | `		}else{` |
|       30 |  4667 | `			pFrameLocal = pVm->pFrame;` |
|       50 |  4668 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4669 | `				/* Safely ignore the exception frame */` |
|       21 |  4670 | `				pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4671 | `			}` |
|        - |  4672 | `			/* Query the local frame */` |
|       30 |  4673 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4674 | `			if( pEntry ){` |
|      ! 0 |  4675 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4676 | `			}else{` |
|       30 |  4677 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4678 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4679 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4680 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4681 | `				}` |
|       30 |  4682 | `				if( rc == SXRET_OK ){` |
|       30 |  4683 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4684 | `				}` |
|        - |  4685 | `			}` |
|        - |  4686 | `		}` |
|       14 |  4687 | `	}` |
|       30 |  4688 | `	break;` |
|        - |  4689 | `				 }` |
|        - |  4690 | `/*` |
|        - |  4691 | ` * OP_UPLINK P1 * *` |
|        - |  4692 | ` * Link a variable to the top active VM frame.` |
|        - |  4693 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4694 | ` */` |
|       23 |  4695 | `case PH7_OP_UPLINK: {` |
|       47 |  4696 | `	if( pVm->pFrame->pParent ){` |
|       47 |  4697 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4698 | `		SyString sName;` |
|        - |  4699 | `		/* Perform the link */` |
|       95 |  4700 | `		while( pLink <= pTos ){` |
|       49 |  4701 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4702 | `				/* Force a string cast */` |
|      ! 0 |  4703 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4704 | `			}` |
|       49 |  4705 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       49 |  4706 | `			if( sName.nByte > 0 ){` |
|       49 |  4707 | `				VmFrameLink(&(*pVm),&sName);` |
|       24 |  4708 | `			}` |
|       49 |  4709 | `			pLink++;` |
|        1 |  4710 | `		}` |
|       23 |  4711 | `	}` |
|       47 |  4712 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       47 |  4713 | `	break;` |
|        - |  4714 | `					}` |
|        - |  4715 | `/*` |
|        - |  4716 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4717 | ` * Push an exception in the corresponding container so that` |
|        - |  4718 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4719 | ` */` |
|       10 |  4720 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       22 |  4721 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4722 | `	VmFrame *pFrameLocal;` |
|       22 |  4723 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4724 | `	/* Create the exception frame */` |
|       22 |  4725 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       22 |  4726 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4727 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4728 | `		goto Abort;` |
|        - |  4729 | `	}` |
|        - |  4730 | `	/* Mark the special frame */` |
|       22 |  4731 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       22 |  4732 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4733 | `	/* Point to the frame that trigger the exception */` |
|       22 |  4734 | `	pFrameLocal = pFrameLocal->pParent;` |
|       34 |  4735 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       13 |  4736 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4737 | `	}` |
|       22 |  4738 | `	pException->pFrame = pFrameLocal;` |
|       22 |  4739 | `	break;` |
|        - |  4740 | `							}` |
|        - |  4741 | `/*` |
|        - |  4742 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4743 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4744 | ` */` |
|        9 |  4745 | `case PH7_OP_POP_EXCEPTION: {` |
|       20 |  4746 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       20 |  4747 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4748 | `		ph7_exception **apException;` |
|        - |  4749 | `		/* Pop the loaded exception */` |
|        7 |  4750 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4751 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4752 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4753 | `		}` |
|        3 |  4754 | `	}` |
|       20 |  4755 | `	pException->pFrame = 0;` |
|        - |  4756 | `	/* Leave the exception frame */` |
|       20 |  4757 | `	VmLeaveFrame(&(*pVm));` |
|       20 |  4758 | `	break;` |
|        - |  4759 | `							}` |
|        - |  4760 |  |
|        - |  4761 | `/*` |
|        - |  4762 | ` * OP_THROW * P2 *` |
|        - |  4763 | ` * Throw an user exception.` |
|        - |  4764 | ` */` |
|       10 |  4765 | `case PH7_OP_THROW: {` |
|       22 |  4766 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       22 |  4767 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4768 | `#ifdef UNTRUST` |
|        - |  4769 | `	if( pTos < pStack ){` |
|        - |  4770 | `		goto Abort;` |
|        - |  4771 | `	}` |
|        - |  4772 | `#endif` |
|       28 |  4773 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4774 | `		/* Safely ignore the exception frame */` |
|        8 |  4775 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4776 | `	}` |
|        - |  4777 | `	/* Tell the upper layer that an exception was thrown */` |
|       22 |  4778 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       22 |  4779 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       22 |  4780 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4781 | `		ph7_class *pException;` |
|        - |  4782 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4783 | `		 */` |
|       22 |  4784 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       22 |  4785 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4786 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4787 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4788 | `			if( rc == SXERR_ABORT ){` |
|        - |  4789 | `				/* Abort processing immediately */` |
|      ! 0 |  4790 | `				goto Abort;` |
|        - |  4791 | `			}` |
|      ! 0 |  4792 | `		}else{` |
|        - |  4793 | `			/* Throw the exception */` |
|       22 |  4794 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       22 |  4795 | `			if( rc == SXERR_ABORT ){` |
|        - |  4796 | `				/* Abort processing immediately */` |
|        7 |  4797 | `				goto Abort;` |
|        - |  4798 | `			}` |
|        - |  4799 | `		}` |
|        9 |  4800 | `	}else{` |
|        - |  4801 | `		/* Expecting a class instance */` |
|      ! 0 |  4802 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4803 | `		if( rc == SXERR_ABORT ){` |
|        - |  4804 | `			/* Abort processing immediately */` |
|      ! 0 |  4805 | `			goto Abort;` |
|        - |  4806 | `		}` |
|        - |  4807 | `	}` |
|        - |  4808 | `	/* Pop the top entry */` |
|       16 |  4809 | `	VmPopOperand(&pTos,1);` |
|        - |  4810 | `	/* Perform an unconditional jump */` |
|       16 |  4811 | `	pc = nJump - 1;` |
|       16 |  4812 | `	break;` |
|        - |  4813 | `				   }` |
|        - |  4814 | `/*` |
|        - |  4815 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4816 | ` * Prepare a foreach step.` |
|        - |  4817 | ` */` |
|     4385 |  4818 | `case PH7_OP_FOREACH_INIT: {` |
|     8772 |  4819 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4820 | `	void *pName;` |
|        - |  4821 | `#ifdef UNTRUST` |
|        - |  4822 | `	if( pTos < pStack ){` |
|        - |  4823 | `		goto Abort;` |
|        - |  4824 | `	}` |
|        - |  4825 | `#endif` |
|     8772 |  4826 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4827 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4828 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4829 | `			/* Force a string cast */` |
|      ! 0 |  4830 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4831 | `		}` |
|        - |  4832 | `		/* Duplicate name */` |
|      ! 0 |  4833 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4834 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4835 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4836 | `		}` |
|      ! 0 |  4837 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4838 | `	}` |
|     8772 |  4839 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4840 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4841 | `			/* Force a string cast */` |
|      ! 0 |  4842 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4843 | `		}` |
|        - |  4844 | `		/* Duplicate name */` |
|      ! 0 |  4845 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4846 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4847 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4848 | `		}` |
|      ! 0 |  4849 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4850 | `	}` |
|        - |  4851 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     8772 |  4852 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4853 | `		/* Jump out of the loop */` |
|      ! 0 |  4854 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4855 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4856 | `		}` |
|      ! 0 |  4857 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4858 | `	}else{` |
|        - |  4859 | `		ph7_foreach_step *pStep;` |
|     8772 |  4860 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     8772 |  4861 | `		if( pStep == 0 ){` |
|      ! 0 |  4862 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4863 | `			/* Jump out of the loop */` |
|      ! 0 |  4864 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4865 | `		}else{` |
|        - |  4866 | `			/* Zero the structure */` |
|     8772 |  4867 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4868 | `			/* Prepare the step */` |
|     8772 |  4869 | `			pStep->iFlags = pInfo->iFlags;` |
|     8772 |  4870 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     8764 |  4871 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4872 | `				/* Reset the internal loop cursor */` |
|     8764 |  4873 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4874 | `				/* Mark the step */` |
|     8764 |  4875 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     8764 |  4876 | `				pStep->xIter.pMap = pMap;` |
|     8764 |  4877 | `				pMap->iRef++;` |
|     4383 |  4878 | `			}else{` |
|        9 |  4879 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4880 | `				/* Reset the loop cursor */` |
|        9 |  4881 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4882 | `				/* Mark the step */` |
|        9 |  4883 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4884 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4885 | `				pThis->iRef++;` |
|        - |  4886 | `			}` |
|        - |  4887 | `		}` |
|     8772 |  4888 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4889 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4890 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4891 | `			/* Jump out of the loop */` |
|      ! 0 |  4892 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4893 | `		}` |
|        - |  4894 | `	}` |
|     8772 |  4895 | `	VmPopOperand(&pTos,1);` |
|     8772 |  4896 | `	break;` |
|        - |  4897 | `						  }` |
|        - |  4898 | `/*` |
|        - |  4899 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4900 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4901 | ` */` |
|    70974 |  4902 | `case PH7_OP_FOREACH_STEP: {` |
|   141950 |  4903 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4904 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4905 | `	ph7_value *pValue;` |
|        - |  4906 | `	VmFrame *pFrameLocal;` |
|        - |  4907 | `	/* Peek the last step */` |
|   141950 |  4908 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   141950 |  4909 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   141950 |  4910 | `	pFrameLocal = pVm->pFrame;` |
|   146982 |  4911 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4912 | `		/* Safely ignore the exception frame */` |
|     5033 |  4913 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4914 | `	}` |
|   141950 |  4915 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   141926 |  4916 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4917 | `		ph7_hashmap_node *pNode;` |
|        - |  4918 | `		/* Extract the current node value */` |
|   141926 |  4919 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   141926 |  4920 | `		if( pNode == 0 ){` |
|        - |  4921 | `			/* No more entry to process */` |
|     8764 |  4922 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     8764 |  4923 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4924 | `				/* Break the reference with the last element */` |
|        5 |  4925 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4926 | `			}` |
|        - |  4927 | `			/* Automatically reset the loop cursor */` |
|     8764 |  4928 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4929 | `			/* Cleanup the mess left behind */` |
|     8764 |  4930 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     8764 |  4931 | `			SySetPop(&pInfo->aStep);` |
|     8764 |  4932 | `			PH7_HashmapUnref(pMap);` |
|     4383 |  4933 | `		}else{` |
|   133164 |  4934 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      259 |  4935 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      259 |  4936 | `				if( pKey ){` |
|      259 |  4937 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      129 |  4938 | `				}` |
|      129 |  4939 | `			}` |
|   133164 |  4940 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4941 | `				SyHashEntry *pEntry;` |
|        - |  4942 | `				/* Pass by reference */` |
|       13 |  4943 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4944 | `				if( pEntry ){` |
|       13 |  4945 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4946 | `				}else{` |
|      ! 0 |  4947 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4948 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4949 | `				}` |
|        7 |  4950 | `			}else{` |
|        - |  4951 | `				/* Make a copy of the entry value */` |
|   133152 |  4952 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   133152 |  4953 | `				if( pValue ){` |
|   133152 |  4954 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    66575 |  4955 | `				}` |
|        - |  4956 | `			}` |
|        - |  4957 | `		}` |
|    70964 |  4958 | `	}else{` |
|       25 |  4959 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4960 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4961 | `		SyHashEntry *pEntry;` |
|        - |  4962 | `		/* Point to the next attribute */` |
|       29 |  4963 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4964 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4965 | `			/* Check access permission */` |
|       31 |  4966 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4967 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4968 | `					break; /* Access is granted */` |
|        - |  4969 | `			}` |
|        1 |  4970 | `		}` |
|       25 |  4971 | `		if( pEntry == 0 ){` |
|        - |  4972 | `			/* Clean up the mess left behind */` |
|        9 |  4973 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  4974 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4975 | `				/* Break the reference with the last element */` |
|        3 |  4976 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  4977 | `			}` |
|        9 |  4978 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  4979 | `			SySetPop(&pInfo->aStep);` |
|        9 |  4980 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  4981 | `		}else{` |
|       17 |  4982 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  4983 | `			ph7_value *pAttrValue;` |
|       17 |  4984 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  4985 | `				/* Fill with the current attribute name */` |
|       17 |  4986 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  4987 | `				if( pKey ){` |
|       17 |  4988 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  4989 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  4990 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  4991 | `				}` |
|        8 |  4992 | `			}` |
|        - |  4993 | `			/* Extract attribute value */` |
|       17 |  4994 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  4995 | `			if( pAttrValue ){` |
|       17 |  4996 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4997 | `					/* Pass by reference */` |
|        3 |  4998 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  4999 | `					if( pEntry ){` |
|        3 |  5000 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5001 | `					}else{` |
|      ! 0 |  5002 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5003 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5004 | `					}` |
|        2 |  5005 | `				}else{` |
|        - |  5006 | `					/* Make a copy of the attribute value */` |
|       15 |  5007 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5008 | `					if( pValue ){` |
|       15 |  5009 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5010 | `					}` |
|        - |  5011 | `				}` |
|        8 |  5012 | `			}` |
|        - |  5013 | `		}` |
|        - |  5014 | `	}` |
|   141950 |  5015 | `	break;` |
|        - |  5016 | `						  }` |
|        - |  5017 | `/*` |
|        - |  5018 | ` * OP_MEMBER P1 P2` |
|        - |  5019 | ` * Load class attribute/method on the stack.` |
|        - |  5020 | ` */` |
|     1488 |  5021 | `case PH7_OP_MEMBER: {` |
|        - |  5022 | `	ph7_class_instance *pThis;` |
|        - |  5023 | `	ph7_value *pNos;` |
|        - |  5024 | `	SyString sName;` |
|     2978 |  5025 | `	if( !pInstr->iP1 ){` |
|     2920 |  5026 | `		pNos = &pTos[-1];` |
|        - |  5027 | `#ifdef UNTRUST` |
|        - |  5028 | `		if( pNos < pStack ){` |
|        - |  5029 | `			goto Abort;` |
|        - |  5030 | `		}` |
|        - |  5031 | `#endif` |
|     2920 |  5032 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5033 | `			ph7_class *pClass;` |
|        - |  5034 | `			/* Class already instantiated */` |
|     2920 |  5035 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5036 | `			/* Point to the instantiated class */` |
|     2920 |  5037 | `			pClass = pThis->pClass;` |
|        - |  5038 | `			/* Extract attribute name first */` |
|     2920 |  5039 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     2920 |  5040 | `			if( pInstr->iP2 ){` |
|        - |  5041 | `				/* Method call */` |
|      120 |  5042 | `				ph7_class_method *pMeth = 0;` |
|      120 |  5043 | `				if( sName.nByte > 0 ){` |
|        - |  5044 | `					/* Extract the target method */` |
|      120 |  5045 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       59 |  5046 | `				}` |
|      120 |  5047 | `				if( pMeth == 0 ){` |
|      ! 0 |  5048 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5049 | `						&pClass->sName,&sName` |
|        - |  5050 | `						);` |
|        - |  5051 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5052 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5053 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5054 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5055 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5056 | `				}else{` |
|        - |  5057 | `					/* Push method name on the stack */` |
|      120 |  5058 | `					PH7_MemObjRelease(pTos);` |
|      120 |  5059 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      120 |  5060 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5061 | `				}` |
|      120 |  5062 | `				pTos->nIdx = SXU32_HIGH;` |
|       61 |  5063 | `			}else{` |
|        - |  5064 | `				/* Attribute access */` |
|     2802 |  5065 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5066 | `				SyHashEntry *pEntry;` |
|        - |  5067 | `				/* Extract the target attribute */` |
|     2802 |  5068 | `				if( sName.nByte > 0 ){` |
|     2802 |  5069 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     2802 |  5070 | `					if( pEntry ){` |
|        - |  5071 | `						/* Point to the attribute value */` |
|     2800 |  5072 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1399 |  5073 | `					}` |
|     1400 |  5074 | `				}` |
|     2802 |  5075 | `				if( pObjAttr == 0 ){` |
|        - |  5076 | `					/* No such attribute,load null */` |
|        4 |  5077 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5078 | `						&pClass->sName,&sName);` |
|        - |  5079 | `					/* Call the __get magic method if available */` |
|        3 |  5080 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5081 | `				}` |
|     2802 |  5082 | `				VmPopOperand(&pTos,1);` |
|        - |  5083 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5084 | `				 * This is due to the following case:` |
|        - |  5085 | `				 *     (new TestClass())->foo;` |
|        - |  5086 | `				 */` |
|     2802 |  5087 | `				pThis->iRef++;` |
|     2802 |  5088 | `				PH7_MemObjRelease(pTos);` |
|     2802 |  5089 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     2802 |  5090 | `				if( pObjAttr ){` |
|     2800 |  5091 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5092 | `					/* Check attribute access */` |
|     2800 |  5093 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5094 | `						/* Load attribute */` |
|     2800 |  5095 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     2800 |  5096 | `						if( pValue ){` |
|     2800 |  5097 | `							if( pThis->iRef < 2 ){` |
|        - |  5098 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5099 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5100 | `								 */` |
|        3 |  5101 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5102 | `							}else{` |
|        - |  5103 | `								/* Simple load */` |
|     2798 |  5104 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5105 | `							}` |
|     2800 |  5106 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     2798 |  5107 | `								if( pThis->iRef > 1 ){` |
|        - |  5108 | `									/* Load attribute index */` |
|     2796 |  5109 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1397 |  5110 | `								}` |
|     1398 |  5111 | `							}` |
|     1399 |  5112 | `						}` |
|     1399 |  5113 | `					}` |
|     1399 |  5114 | `				}` |
|        - |  5115 | `				/* Safely unreference the object */` |
|     2802 |  5116 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5117 | `			}` |
|     1461 |  5118 | `		}else{` |
|      ! 0 |  5119 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5120 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5121 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5122 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5123 | `		}` |
|     1461 |  5124 | `	}else{` |
|        - |  5125 | `		/* Static member access using class name */` |
|       59 |  5126 | `		pNos = pTos;` |
|       59 |  5127 | `		pThis = 0;` |
|       59 |  5128 | `		if( !pInstr->p3 ){` |
|       57 |  5129 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5130 | `			pNos--;` |
|        - |  5131 | `#ifdef UNTRUST` |
|        - |  5132 | `			if( pNos < pStack ){` |
|        - |  5133 | `				goto Abort;` |
|        - |  5134 | `			}` |
|        - |  5135 | `#endif` |
|       29 |  5136 | `		}else{` |
|        - |  5137 | `			/* Attribute name already computed */` |
|        3 |  5138 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5139 | `		}` |
|       59 |  5140 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5141 | `			ph7_class *pClass = 0;` |
|       59 |  5142 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5143 | `				/* Class already instantiated */` |
|      ! 0 |  5144 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5145 | `				pClass = pThis->pClass;` |
|      ! 0 |  5146 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5147 | `			}else{` |
|        - |  5148 | `				/* Try to extract the target class */` |
|       59 |  5149 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5150 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5151 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5152 | `				}` |
|        - |  5153 | `			}` |
|       59 |  5154 | `			if( pClass == 0 ){` |
|        - |  5155 | `				/* Undefined class */` |
|      ! 0 |  5156 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5157 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5158 | `					);` |
|      ! 0 |  5159 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5160 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5161 | `				}` |
|      ! 0 |  5162 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5163 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5164 | `			}else{` |
|       59 |  5165 | `				if( pInstr->iP2 ){` |
|        - |  5166 | `					/* Method call */` |
|       25 |  5167 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5168 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5169 | `						/* Extract the target method */` |
|       25 |  5170 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5171 | `					}` |
|       25 |  5172 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5173 | `						if( pMeth ){` |
|      ! 0 |  5174 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5175 | `								&pClass->sName,&sName` |
|        - |  5176 | `								);` |
|      ! 0 |  5177 | `						}else{` |
|      ! 0 |  5178 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5179 | `								&pClass->sName,&sName` |
|        - |  5180 | `								);` |
|        - |  5181 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5182 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5183 | `						}` |
|        - |  5184 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5185 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5186 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5187 | `						}` |
|      ! 0 |  5188 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5189 | `					}else{` |
|        - |  5190 | `						/* Push method name on the stack */` |
|       25 |  5191 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5192 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5193 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5194 | `					}` |
|       25 |  5195 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5196 | `				}else{` |
|        - |  5197 | `					/* Attribute access */` |
|       35 |  5198 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5199 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5200 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5201 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5202 | `						/* ::class returns the fully qualified class name */` |
|        - |  5203 | `						/* Pop the attribute name from the stack */` |
|       27 |  5204 | `						if( !pInstr->p3 ){` |
|       27 |  5205 | `							VmPopOperand(&pTos,1);` |
|       13 |  5206 | `						}` |
|       27 |  5207 | `						PH7_MemObjRelease(pTos);` |
|        - |  5208 | `						/* Load the class name */` |
|       27 |  5209 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5210 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5211 | `					}else{` |
|        - |  5212 | `						/* Extract the target attribute */` |
|        9 |  5213 | `						if( sName.nByte > 0 ){` |
|        9 |  5214 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5215 | `						}` |
|        9 |  5216 | `						if( pAttr == 0 ){` |
|        - |  5217 | `							/* No such attribute,load null */` |
|      ! 0 |  5218 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5219 | `								&pClass->sName,&sName);` |
|        - |  5220 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5221 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5222 | `						}` |
|        - |  5223 | `						/* Pop the attribute name from the stack */` |
|        9 |  5224 | `						if( !pInstr->p3 ){` |
|        7 |  5225 | `							VmPopOperand(&pTos,1);` |
|        3 |  5226 | `						}` |
|        9 |  5227 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5228 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5229 | `						if( pAttr ){` |
|        9 |  5230 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5231 | `								/* Access to a non static attribute */` |
|      ! 0 |  5232 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5233 | `									&pClass->sName,&pAttr->sName` |
|        - |  5234 | `									);` |
|      ! 0 |  5235 | `							}else{` |
|        - |  5236 | `								ph7_value *pValue;` |
|        - |  5237 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5238 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5239 | `									/* Load the desired attribute */` |
|        9 |  5240 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5241 | `									if( pValue ){` |
|        9 |  5242 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5243 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5244 | `											/* Load index number */` |
|        3 |  5245 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5246 | `										}` |
|        4 |  5247 | `									}` |
|        4 |  5248 | `								}` |
|        - |  5249 | `							}` |
|        4 |  5250 | `						}` |
|        - |  5251 | `					}` |
|        - |  5252 | `				}` |
|       59 |  5253 | `				if( pThis ){` |
|        - |  5254 | `					/* Safely unreference the object */` |
|      ! 0 |  5255 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5256 | `				}` |
|        - |  5257 | `			}` |
|       30 |  5258 | `		}else{` |
|        - |  5259 | `			/* Pop operands */` |
|      ! 0 |  5260 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5261 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5262 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5263 | `			}` |
|      ! 0 |  5264 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5265 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5266 | `		}` |
|        - |  5267 | `	}` |
|     2978 |  5268 | `	break;` |
|        - |  5269 | `					}` |
|        - |  5270 | `/*` |
|        - |  5271 | ` * OP_NEW P1 * * *` |
|        - |  5272 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5273 | ` */` |
|      252 |  5274 | `case PH7_OP_NEW: {` |
|      506 |  5275 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      506 |  5276 | `	ph7_class *pClass = 0;` |
|        - |  5277 | `	ph7_class_instance *pNew;` |
|      506 |  5278 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5279 | `		/* Try to extract the desired class */` |
|      758 |  5280 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      504 |  5281 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      252 |  5282 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5283 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5284 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5285 | `	}` |
|      506 |  5286 | `	if( pClass == 0 ){` |
|        - |  5287 | `		/* No such class */` |
|      ! 0 |  5288 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5289 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5290 | `			);` |
|      ! 0 |  5291 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5292 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5293 | `			/* Pop given arguments */` |
|      ! 0 |  5294 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5295 | `		}` |
|      ! 0 |  5296 | `	}else{` |
|        - |  5297 | `		ph7_class_method *pCons;` |
|        - |  5298 | `		/* Create a new class instance */` |
|      506 |  5299 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      506 |  5300 | `		if( pNew == 0 ){` |
|      ! 0 |  5301 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5302 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5303 | `				&pClass->sName` |
|        - |  5304 | `			);` |
|      ! 0 |  5305 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5306 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5307 | `				/* Pop given arguments */` |
|      ! 0 |  5308 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5309 | `			}` |
|      ! 0 |  5310 | `			break;` |
|        - |  5311 | `		}` |
|        - |  5312 | `		/* Check if a constructor is available */` |
|      506 |  5313 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      506 |  5314 | `		if( pCons == 0 ){` |
|      450 |  5315 | `			SyString *pName = &pClass->sName;` |
|        - |  5316 | `			/* Check for a constructor with the same base class name */` |
|      450 |  5317 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      224 |  5318 | `		}` |
|      506 |  5319 | `		if( pCons ){` |
|        - |  5320 | `			/* Call the class constructor */` |
|       58 |  5321 | `			SySetReset(&aArg);` |
|      104 |  5322 | `			while( pArg < pTos ){` |
|       48 |  5323 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       48 |  5324 | `				pArg++;` |
|        2 |  5325 | `			}` |
|       58 |  5326 | `			if( pVm->bErrReport ){` |
|        - |  5327 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5328 | `				sxu32 n;` |
|       15 |  5329 | `				n = SySetUsed(&aArg);` |
|        - |  5330 | `				/* Emit a notice for missing arguments */` |
|       39 |  5331 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       25 |  5332 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       25 |  5333 | `					if( pFuncArg ){` |
|       25 |  5334 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5335 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5336 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5337 | `						}` |
|       12 |  5338 | `					}` |
|       25 |  5339 | `					n++;` |
|        1 |  5340 | `				}` |
|        7 |  5341 | `			}` |
|       58 |  5342 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5343 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       58 |  5344 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5345 | `				pNew->iRef = 1;` |
|      ! 0 |  5346 | `			}` |
|       28 |  5347 | `		}` |
|      506 |  5348 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5349 | `			/* Pop given arguments */` |
|       42 |  5350 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       20 |  5351 | `		}` |
|      506 |  5352 | `		PH7_MemObjRelease(pTos);` |
|      506 |  5353 | `		pTos->x.pOther = pNew;` |
|      506 |  5354 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5355 | `	}` |
|      506 |  5356 | `	break;` |
|        - |  5357 | `				 }` |
|        - |  5358 | `/*` |
|        - |  5359 | ` * OP_CLONE * * *` |
|        - |  5360 | ` * Perfome a clone operation.` |
|        - |  5361 | ` */` |
|       23 |  5362 | `case PH7_OP_CLONE: {` |
|        - |  5363 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5364 | `#ifdef UNTRUST` |
|        - |  5365 | `	if( pTos < pStack ){` |
|        - |  5366 | `		goto Abort;` |
|        - |  5367 | `	}` |
|        - |  5368 | `#endif` |
|        - |  5369 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5370 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5371 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5372 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5373 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5374 | `		break;` |
|        - |  5375 | `	}` |
|        - |  5376 | `	/* Point to the source */` |
|       44 |  5377 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5378 | `	/* Perform the clone operation */` |
|       44 |  5379 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5380 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5381 | `	if( pClone == 0 ){` |
|      ! 0 |  5382 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5383 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5384 | `	}else{` |
|        - |  5385 | `		/* Load the cloned object */` |
|       44 |  5386 | `		pTos->x.pOther = pClone;` |
|       44 |  5387 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5388 | `	}` |
|       44 |  5389 | `	break;` |
|        - |  5390 | `				   }` |
|        - |  5391 | `/*` |
|        - |  5392 | ` * OP_SWITCH * * P3` |
|        - |  5393 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5394 | ` */` |
|       18 |  5395 | `case PH7_OP_SWITCH: {` |
|       38 |  5396 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5397 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5398 | `	ph7_value sValue,sCaseValue;` |
|        - |  5399 | `	sxu32 n,nEntry;` |
|        - |  5400 | `#ifdef UNTRUST` |
|        - |  5401 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5402 | `		goto Abort;` |
|        - |  5403 | `	}` |
|        - |  5404 | `#endif` |
|        - |  5405 | `	/* Point to the case table  */` |
|       38 |  5406 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5407 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5408 | `	/* Select the appropriate case block to execute */` |
|       38 |  5409 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5410 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5411 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5412 | `		pCase = &aCase[n];` |
|       92 |  5413 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5414 | `		/* Execute the case expression first */` |
|       92 |  5415 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5416 | `		/* Compare the two expression */` |
|       92 |  5417 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5418 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5419 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5420 | `		if( rc == 0 ){` |
|        - |  5421 | `			/* Value match,jump to this block */` |
|       38 |  5422 | `			pc = pCase->nStart - 1;` |
|       38 |  5423 | `			break;` |
|        - |  5424 | `		}` |
|       29 |  5425 | `	}` |
|       38 |  5426 | `	VmPopOperand(&pTos,1);` |
|       38 |  5427 | `	if( n >= nEntry ){` |
|        - |  5428 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5429 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5430 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5431 | `		}else{` |
|        - |  5432 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5433 | `			pc = pSwitch->nOut - 1;` |
|        - |  5434 | `		}` |
|      ! 0 |  5435 | `	}` |
|       38 |  5436 | `	break;` |
|        - |  5437 | `					}` |
|        - |  5438 | `/*` |
|        - |  5439 | ` * OP_CALL P1 * *` |
|        - |  5440 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5441 | ` *  function on the stack.` |
|        - |  5442 | ` */` |
|   269297 |  5443 | `case PH7_OP_CALL: {` |
|   538640 |  5444 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5445 | `	SyHashEntry *pEntry;` |
|        - |  5446 | `	SyString sName;` |
|        - |  5447 | `	/* Extract function name */` |
|   538640 |  5448 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5449 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5450 | `			ph7_value sResult;` |
|      ! 0 |  5451 | `			SySetReset(&aArg);` |
|      ! 0 |  5452 | `			while( pArg < pTos ){` |
|      ! 0 |  5453 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5454 | `				pArg++;` |
|      ! 0 |  5455 | `			}` |
|      ! 0 |  5456 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5457 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5458 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5459 | `			SySetReset(&aArg);` |
|        - |  5460 | `			/* Pop given arguments */` |
|      ! 0 |  5461 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5462 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5463 | `			}` |
|        - |  5464 | `			/* Copy result */` |
|      ! 0 |  5465 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5466 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5467 | `		}else{` |
|        3 |  5468 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5469 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5470 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5471 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5472 | `			}else{` |
|        - |  5473 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5474 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5475 | `			}` |
|        - |  5476 | `			/* Pop given arguments */` |
|        3 |  5477 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5478 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5479 | `			}` |
|        - |  5480 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5481 | `			PH7_MemObjRelease(pTos);` |
|        - |  5482 | `		}` |
|   269118 |  5483 | `		break;` |
|        - |  5484 | `	}` |
|   538638 |  5485 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5486 | `	/* Check for a compiled function first */` |
|   538638 |  5487 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   538638 |  5488 | `	if( pEntry ){` |
|        - |  5489 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5490 | `		ph7_class_instance *pThis;` |
|        - |  5491 | `		ph7_value *pFrameStack;` |
|        - |  5492 | `		ph7_vm_func *pVmFunc;` |
|        - |  5493 | `		ph7_class *pSelf;` |
|        - |  5494 | `		VmFrame *pFrame;` |
|        - |  5495 | `		ph7_value *pObj;` |
|        - |  5496 | `		VmSlot sArg;` |
|        - |  5497 | `		sxu32 n;` |
|        - |  5498 | `		/* initialize fields */` |
|    10666 |  5499 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    10666 |  5500 | `		pThis = 0;` |
|    10666 |  5501 | `		pSelf = 0;` |
|    10666 |  5502 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5503 | `			ph7_class_method *pMeth;` |
|        - |  5504 | `			/* Class method call */` |
|     1062 |  5505 | `			ph7_value *pTarget = &pTos[-1];` |
|     1062 |  5506 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5507 | `				/* Extract the 'this' pointer */` |
|     1062 |  5508 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5509 | `					/* Instance already loaded */` |
|     1032 |  5510 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1032 |  5511 | `					pThis->iRef++;` |
|     1032 |  5512 | `					pSelf = pThis->pClass;` |
|      515 |  5513 | `				}` |
|     1062 |  5514 | `				if( pSelf == 0 ){` |
|       31 |  5515 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5516 | `						/* "Late Static Binding" class name */` |
|       37 |  5517 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5518 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5519 | `					}` |
|       31 |  5520 | `					if( pSelf == 0 ){` |
|        7 |  5521 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5522 | `					}` |
|       15 |  5523 | `				}` |
|     1062 |  5524 | `				if( pThis == 0  ){` |
|       31 |  5525 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       33 |  5526 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5527 | `						/* Safely ignore the exception frame */` |
|        3 |  5528 | `						pFrameLocal = pFrameLocal->pParent;` |
|        1 |  5529 | `					}` |
|       31 |  5530 | `					if( pFrameLocal->pParent ){` |
|        - |  5531 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5532 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5533 | `						if( pThis ){` |
|       13 |  5534 | `							pThis->iRef++;` |
|        6 |  5535 | `						}` |
|        9 |  5536 | `					}` |
|       15 |  5537 | `				}` |
|     1062 |  5538 | `				VmPopOperand(&pTos,1);` |
|     1062 |  5539 | `				PH7_MemObjRelease(pTos);` |
|        - |  5540 | `				/* Synchronize pointers */` |
|     1062 |  5541 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5542 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5543 | `				 * user have already computed the random generated unique class method name` |
|        - |  5544 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5545 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5546 | `				 */` |
|     1062 |  5547 | `				while( pArg < pStack ){` |
|      ! 0 |  5548 | `					pArg++;` |
|      ! 0 |  5549 | `				}` |
|     1062 |  5550 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5551 | `					/* Check if the call is allowed */` |
|     1062 |  5552 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1062 |  5553 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5554 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5555 | `							/* Pop given arguments */` |
|      ! 0 |  5556 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5557 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5558 | `							}` |
|        - |  5559 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5560 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5561 | `							break;` |
|        - |  5562 | `						}` |
|        2 |  5563 | `					}` |
|      530 |  5564 | `				}` |
|      530 |  5565 | `			}` |
|      530 |  5566 | `		}` |
|        - |  5567 | `		/* Check The recursion limit */` |
|    10666 |  5568 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5569 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5570 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5571 | `				&pVmFunc->sName);` |
|        - |  5572 | `			/* Pop given arguments */` |
|        3 |  5573 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5574 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5575 | `			}` |
|        - |  5576 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5577 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5578 | `			break;` |
|        - |  5579 | `		}` |
|    10664 |  5580 | `		if( pVmFunc->pNextName ){` |
|        - |  5581 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5582 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5583 | `		}` |
|        - |  5584 | `		/* Extract the formal argument set */` |
|    10664 |  5585 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5586 | `		/* Create a new VM frame  */` |
|    10664 |  5587 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    10664 |  5588 | `		if( rc != SXRET_OK ){` |
|        - |  5589 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5590 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5591 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5592 | `				&pVmFunc->sName);` |
|        - |  5593 | `			/* Pop given arguments */` |
|      ! 0 |  5594 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5595 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5596 | `			}` |
|        - |  5597 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5598 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5599 | `			break;` |
|        - |  5600 | `		}` |
|    10664 |  5601 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5602 | `			/* Install the '$this' variable */` |
|        - |  5603 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1042 |  5604 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1042 |  5605 | `			if( pObj ){` |
|        - |  5606 | `				/* Reflect the change */` |
|     1042 |  5607 | `				pObj->x.pOther = pThis;` |
|     1042 |  5608 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      520 |  5609 | `			}` |
|      520 |  5610 | `		}` |
|    10664 |  5611 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5612 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5613 | `			/* Install static variables */` |
|      ! 0 |  5614 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5615 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5616 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5617 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5618 | `					/* Initialize the static variables */` |
|      ! 0 |  5619 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5620 | `					if( pObj ){` |
|        - |  5621 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5622 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5623 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5624 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5625 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5626 | `						}` |
|      ! 0 |  5627 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5628 | `					}else{` |
|      ! 0 |  5629 | `						continue;` |
|        - |  5630 | `					}` |
|      ! 0 |  5631 | `				}` |
|        - |  5632 | `				/* Install in the current frame */` |
|      ! 0 |  5633 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5634 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5635 | `			}` |
|      ! 0 |  5636 | `		}` |
|        - |  5637 | `		/* Push arguments in the local frame */` |
|    10664 |  5638 | `		n = 0;` |
|    29994 |  5639 | `		while( pArg < pTos ){` |
|    19332 |  5640 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    19214 |  5641 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5642 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5643 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5644 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5645 | `						goto Abort;` |
|        - |  5646 | `					}` |
|      ! 0 |  5647 | `				}` |
|        - |  5648 | `				/* Make sure the given arguments are of the correct type */` |
|    19214 |  5649 | `				if( aFormalArg[n].nType > 0 ){` |
|     1066 |  5650 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5651 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5652 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5653 | `						ph7_class *pClass;` |
|        - |  5654 | `						/* Try to extract the desired class */` |
|      ! 0 |  5655 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5656 | `						if( pClass ){` |
|      ! 0 |  5657 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5658 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5659 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5660 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5661 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5662 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5663 | `								}` |
|      ! 0 |  5664 | `							}else{` |
|        - |  5665 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5666 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5667 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5668 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5669 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5670 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5671 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5672 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5673 | `								}` |
|        - |  5674 | `							}` |
|      ! 0 |  5675 | `						}` |
|     1066 |  5676 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5677 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5678 | `						/* Cast to the desired type */` |
|      ! 0 |  5679 | `						xCast(pArg);` |
|      ! 0 |  5680 | `					}` |
|      532 |  5681 | `				}` |
|    19214 |  5682 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5683 | `					/* Pass by reference */` |
|       48 |  5684 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5685 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5686 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5687 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5688 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5689 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5690 | `						}` |
|        - |  5691 | `						/* Switch to pass by value */` |
|      ! 0 |  5692 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5693 | `					}else{` |
|        - |  5694 | `						SyHashEntry *pRefEntry;` |
|        - |  5695 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5696 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5697 | `						if( pRefEntry == 0 ){` |
|       71 |  5698 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5699 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5700 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5701 | `							sArg.pUserData = 0;` |
|       48 |  5702 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5703 | `						}` |
|       48 |  5704 | `						pObj = 0;` |
|        - |  5705 | `					}` |
|       25 |  5706 | `				}else{` |
|        - |  5707 | `					/* Pass by value,make a copy of the given argument */` |
|    19168 |  5708 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5709 | `				}` |
|     9608 |  5710 | `			}else{` |
|        - |  5711 | `				char zName[32];` |
|        - |  5712 | `				SyString sArgName;` |
|        - |  5713 | `				/* Set a dummy name */` |
|      120 |  5714 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      120 |  5715 | `				sArgName.zString = zName;` |
|        - |  5716 | `				/* Annonymous argument */` |
|      120 |  5717 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5718 | `			}` |
|    19332 |  5719 | `			if( pObj ){` |
|    19286 |  5720 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5721 | `				/* Insert argument index  */` |
|    19286 |  5722 | `				sArg.nIdx = pObj->nIdx;` |
|    19286 |  5723 | `				sArg.pUserData = 0;` |
|    19286 |  5724 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|     9642 |  5725 | `			}` |
|    19332 |  5726 | `			PH7_MemObjRelease(pArg);` |
|    19332 |  5727 | `			pArg++;` |
|    19332 |  5728 | `			++n;` |
|        2 |  5729 | `		}` |
|        - |  5730 | `		/* Set up closure environment */` |
|    10664 |  5731 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5732 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5733 | `			ph7_value *pValue;` |
|        - |  5734 | `			sxu32 iEnv;` |
|        9 |  5735 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5736 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5737 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5738 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5739 | `					/* Do not install null value */` |
|        9 |  5740 | `					continue;` |
|        - |  5741 | `				}` |
|        9 |  5742 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5743 | `				if( pValue == 0 ){` |
|      ! 0 |  5744 | `					continue;` |
|        - |  5745 | `				}` |
|        - |  5746 | `				/* Invalidate any prior representation */` |
|        9 |  5747 | `				PH7_MemObjRelease(pValue);` |
|        - |  5748 | `				/* Duplicate bound variable value */` |
|        9 |  5749 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5750 | `			}` |
|        4 |  5751 | `		}` |
|        - |  5752 | `		/* Process default values */` |
|    12252 |  5753 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1590 |  5754 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1580 |  5755 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1580 |  5756 | `				if( pObj ){` |
|        - |  5757 | `					/* Evaluate the default value and extract it's result */` |
|     1580 |  5758 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1580 |  5759 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5760 | `						goto Abort;` |
|        - |  5761 | `					}` |
|        - |  5762 | `					/* Insert argument index */` |
|     1580 |  5763 | `					sArg.nIdx = pObj->nIdx;` |
|     1580 |  5764 | `					sArg.pUserData = 0;` |
|     1580 |  5765 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5766 | `					/* Make sure the default argument is of the correct type */` |
|     1580 |  5767 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5768 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5769 | `						/* Cast to the desired type */` |
|      ! 0 |  5770 | `						xCast(pObj);` |
|      ! 0 |  5771 | `					}` |
|      789 |  5772 | `				}` |
|      789 |  5773 | `			}` |
|     1590 |  5774 | `			++n;` |
|        2 |  5775 | `		}` |
|        - |  5776 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5777 | `		 * does not return anything.` |
|        - |  5778 | `		 */` |
|    10664 |  5779 | `		PH7_MemObjRelease(pTos);` |
|    10664 |  5780 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5781 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    10664 |  5782 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    10664 |  5783 | `		if( pFrameStack == 0 ){` |
|        - |  5784 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5785 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5786 | `				&pVmFunc->sName);` |
|      ! 0 |  5787 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5788 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5789 | `			}` |
|      ! 0 |  5790 | `			break;` |
|        - |  5791 | `		}` |
|    10664 |  5792 | `		if( pSelf ){` |
|        - |  5793 | `			/* Push class name */` |
|     1060 |  5794 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      529 |  5795 | `		}` |
|        - |  5796 | `		/* Increment nesting level */` |
|    10664 |  5797 | `		pVm->nRecursionDepth++;` |
|        - |  5798 | `		/* Execute function body */` |
|    10664 |  5799 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5800 | `		/* Decrement nesting level */` |
|    10664 |  5801 | `		pVm->nRecursionDepth--;` |
|    10664 |  5802 | `		if( pSelf ){` |
|        - |  5803 | `			/* Pop class name */` |
|     1060 |  5804 | `			(void)SySetPop(&pVm->aSelf);` |
|      529 |  5805 | `		}` |
|        - |  5806 | `		/* Cleanup the mess left behind */` |
|    10664 |  5807 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5808 | `			/* Return by reference,reflect that */` |
|        9 |  5809 | `			if( n != SXU32_HIGH ){` |
|        9 |  5810 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5811 | `				sxu32 i;` |
|        - |  5812 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5813 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5814 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5815 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5816 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5817 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5818 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5819 | `								&pVmFunc->sName);` |
|      ! 0 |  5820 | `						}` |
|      ! 0 |  5821 | `						n = SXU32_HIGH;` |
|      ! 0 |  5822 | `						break;` |
|        - |  5823 | `					}` |
|        3 |  5824 | `				}` |
|        5 |  5825 | `			}else{` |
|      ! 0 |  5826 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5827 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5828 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5829 | `						&pVmFunc->sName);` |
|      ! 0 |  5830 | `				}` |
|        - |  5831 | `			}` |
|        9 |  5832 | `			pTos->nIdx = n;` |
|        4 |  5833 | `		}` |
|        - |  5834 | `		/* Cleanup the mess left behind */` |
|    10664 |  5835 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5836 | `			/* An exception was throw in this frame */` |
|        7 |  5837 | `			pFrame = pFrame->pParent;` |
|        7 |  5838 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5839 | `				/* Pop the resutlt */` |
|        5 |  5840 | `				VmPopOperand(&pTos,1);` |
|        - |  5841 | `				/* Jump to this destination */` |
|        5 |  5842 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5843 | `				rc = PH7_OK;` |
|        3 |  5844 | `			}else{` |
|        3 |  5845 | `				if( pFrame->pParent ){` |
|        3 |  5846 | `					rc = PH7_EXCEPTION;` |
|        2 |  5847 | `				}else{` |
|        - |  5848 | `					/* Continue normal execution */` |
|      ! 0 |  5849 | `					rc = PH7_OK;` |
|        - |  5850 | `				}` |
|        - |  5851 | `			}` |
|        3 |  5852 | `		}` |
|        - |  5853 | `		/* Free the operand stack */` |
|    10664 |  5854 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5855 | `		/* Leave the frame */` |
|    10664 |  5856 | `		VmLeaveFrame(&(*pVm));` |
|    10664 |  5857 | `		if( rc == PH7_ABORT ){` |
|        - |  5858 | `			/* Abort processing immeditaley */` |
|        5 |  5859 | `			goto Abort;` |
|    10660 |  5860 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5861 | `			goto Exception;` |
|        - |  5862 | `		}` |
|     5330 |  5863 | `	}else{` |
|        - |  5864 | `		ph7_user_func *pFunc;` |
|        - |  5865 | `		ph7_context sCtx;` |
|        - |  5866 | `		ph7_value sRet;` |
|        - |  5867 | `		/* Look for an installed foreign function */` |
|   527974 |  5868 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   527974 |  5869 | `		if( pEntry == 0 ){` |
|        - |  5870 | `			/* Call to undefined function */` |
|        5 |  5871 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5872 | `			/* Pop given arguments */` |
|        5 |  5873 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5874 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5875 | `			}` |
|        - |  5876 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5877 | `			PH7_MemObjRelease(pTos);` |
|        5 |  5878 | `			break;` |
|        - |  5879 | `		}` |
|   527970 |  5880 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5881 | `		/* Start collecting function arguments */` |
|   527970 |  5882 | `		SySetReset(&aArg);` |
|  1405090 |  5883 | `		while( pArg < pTos ){` |
|   877122 |  5884 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   877122 |  5885 | `			pArg++;` |
|        2 |  5886 | `		}` |
|        - |  5887 | `		/* Assume a null return value */` |
|   527970 |  5888 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5889 | `		/* Init the call context */` |
|   527970 |  5890 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5891 | `		/* Call the foreign function */` |
|   527970 |  5892 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5893 | `		/* Release the call context */` |
|   527970 |  5894 | `		VmReleaseCallContext(&sCtx);` |
|   527970 |  5895 | `		if( rc == PH7_ABORT ){` |
|      355 |  5896 | `			goto Abort;` |
|   527616 |  5897 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5898 | `			goto Exception;` |
|        - |  5899 | `		}` |
|   527614 |  5900 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5901 | `			/* Pop function name and arguments */` |
|   510622 |  5902 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   255332 |  5903 | `		}` |
|        - |  5904 | `		/* Save foreign function return value */` |
|   527614 |  5905 | `		PH7_MemObjStore(&sRet,pTos);` |
|   527614 |  5906 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5907 | `	}` |
|   538270 |  5908 | `	break;` |
|        - |  5909 | `				  }` |
|        - |  5910 | `/*` |
|        - |  5911 | ` * OP_CONSUME: P1 * *` |
|        - |  5912 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5913 | ` */` |
|     9821 |  5914 | `case PH7_OP_CONSUME: {` |
|    19644 |  5915 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    19644 |  5916 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5917 |  |
|    19644 |  5918 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    19644 |  5919 | `	pCur = pOut;` |
|        - |  5920 | `	/* Start the consume process  */` |
|    39286 |  5921 | `	while( pOut <= pTos ){` |
|        - |  5922 | `		/* Force a string cast */` |
|    19644 |  5923 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      158 |  5924 | `			PH7_MemObjToString(pOut);` |
|       78 |  5925 | `		}` |
|    19644 |  5926 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5927 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5928 | `			/* Invoke the output consumer callback */` |
|    10498 |  5929 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10498 |  5930 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5931 | `				/* Increment output length */` |
|     4148 |  5932 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2073 |  5933 | `			}` |
|    10498 |  5934 | `			SyBlobRelease(&pOut->sBlob);` |
|    10498 |  5935 | `			if( rc == SXERR_ABORT ){` |
|        - |  5936 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5937 | `				goto Abort;` |
|        - |  5938 | `			}` |
|     5248 |  5939 | `		}` |
|    19644 |  5940 | `		pOut++;` |
|        2 |  5941 | `	}` |
|    19644 |  5942 | `	pTos = &pCur[-1];` |
|    19642 |  5943 | `	break;` |
|        - |  5944 | `					 }` |
|        - |  5945 |  |
|        - |  5946 | `		} /* Switch() */` |
|  9392686 |  5947 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5948 | `	} /* For(;;) */` |
|    13291 |  5949 | `Done:` |
|    26584 |  5950 | `	SySetRelease(&aArg);` |
|    26584 |  5951 | `	return SXRET_OK;` |
|      182 |  5952 | `Abort:` |
|      365 |  5953 | `	SySetRelease(&aArg);` |
|     1271 |  5954 | `	while( pTos >= pStack ){` |
|      907 |  5955 | `		PH7_MemObjRelease(pTos);` |
|      907 |  5956 | `		pTos--;` |
|        1 |  5957 | `	}` |
|      365 |  5958 | `	return PH7_ABORT;` |
|        2 |  5959 | `Exception:` |
|        5 |  5960 | `	SySetRelease(&aArg);` |
|        9 |  5961 | `	while( pTos >= pStack ){` |
|        5 |  5962 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5963 | `		pTos--;` |
|        1 |  5964 | `	}` |
|        5 |  5965 | `	return PH7_EXCEPTION;` |
|    13477 |  5966 |  |
|        - |  5967 | `/*` |
|        - |  5968 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  5969 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  5970 | ` * See block-comment on that function for additional information.` |
|        - |  5971 | ` */` |
|    13274 |  5972 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  5973 |  |
|        - |  5974 | `	ph7_value *pStack;` |
|        - |  5975 | `	sxi32 rc;` |
|        - |  5976 | `	/* Allocate a new operand stack */` |
|    13276 |  5977 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    13276 |  5978 | `	if( pStack == 0 ){` |
|      ! 0 |  5979 | `		return SXERR_MEM;` |
|        - |  5980 | `	}` |
|        - |  5981 | `	/* Execute the program */` |
|    13276 |  5982 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  5983 | `	/* Free the operand stack */` |
|    13276 |  5984 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  5985 | `	/* Execution result */` |
|    13276 |  5986 | `	return rc;` |
|     6639 |  5987 |  |
|        - |  5988 | `/*` |
|        - |  5989 | ` * Invoke any installed shutdown callbacks.` |
|        - |  5990 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  5991 | ` * or more calls to [register_shutdown_function()].` |
|        - |  5992 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  5993 | ` * execution ends.` |
|        - |  5994 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  5995 | ` * additional information.` |
|        - |  5996 | ` */` |
|     1664 |  5997 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  5998 |  |
|        - |  5999 | `	VmShutdownCB *pEntry;` |
|        - |  6000 | `	ph7_value *apArg[10];` |
|        - |  6001 | `	sxu32 n,nEntry;` |
|        - |  6002 | `	int i;` |
|        - |  6003 | `	/* Point to the stack of registered callbacks */` |
|     1666 |  6004 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    18306 |  6005 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    16642 |  6006 | `		apArg[i] = 0;` |
|     8322 |  6007 | `	}` |
|     1668 |  6008 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6009 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6010 | `		if( pEntry ){` |
|        - |  6011 | `			/* Prepare callback arguments if any */` |
|        3 |  6012 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6013 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6014 | `					break;` |
|        - |  6015 | `				}` |
|      ! 0 |  6016 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6017 | `			}` |
|        - |  6018 | `			/* Invoke the callback */` |
|        3 |  6019 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6020 | `			/*` |
|        - |  6021 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6022 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6023 | `			 */` |
|        3 |  6024 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6025 | `			if( pEntry ){` |
|        3 |  6026 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6027 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6028 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6029 | `				}` |
|        1 |  6030 | `			}` |
|        1 |  6031 | `		}` |
|        2 |  6032 | `	}` |
|     1666 |  6033 | `	SySetReset(&pVm->aShutdown);` |
|     1666 |  6034 |  |
|        - |  6035 | `/*` |
|        - |  6036 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6037 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6038 | ` * See block-comment on that function for additional information.` |
|        - |  6039 | ` */` |
|     1672 |  6040 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6041 |  |
|        - |  6042 | `	/* Make sure we are ready to execute this program */` |
|     1674 |  6043 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6044 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6045 | `	}` |
|        - |  6046 | `	/* Set the execution magic number  */` |
|     1674 |  6047 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6048 | `	/* Execute the program */` |
|     1674 |  6049 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6050 | `	/* Invoke any shutdown callbacks */` |
|     1670 |  6051 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6052 | `	/*` |
|        - |  6053 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6054 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6055 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6056 | `	 */` |
|     1670 |  6057 | `	return SXRET_OK;` |
|      838 |  6058 |  |
|        - |  6059 | `/*` |
|        - |  6060 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6061 | ` * the desired message.` |
|        - |  6062 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6063 | ` * in 'api.c' for additional information.` |
|        - |  6064 | ` */` |
|      352 |  6065 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6066 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6067 | `	SyString *pString /* Message to output */` |
|        - |  6068 | `	)` |
|        2 |  6069 |  |
|      354 |  6070 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      354 |  6071 | `	sxi32 rc = SXRET_OK;` |
|        - |  6072 | `	/* Call the output consumer */` |
|      354 |  6073 | `	if( pString->nByte > 0 ){` |
|      354 |  6074 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      354 |  6075 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6076 | `			/* Increment output length */` |
|       17 |  6077 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6078 | `		}` |
|      176 |  6079 | `	}` |
|      354 |  6080 | `	return rc;` |
|        2 |  6081 |  |
|        - |  6082 | `/*` |
|        - |  6083 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6084 | ` * callback to consume the formatted message.` |
|        - |  6085 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6086 | ` * in 'api.c' for additional information.` |
|        - |  6087 | ` */` |
|        2 |  6088 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6089 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6090 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6091 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6092 | `	)` |
|        1 |  6093 |  |
|        3 |  6094 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6095 | `	sxi32 rc = SXRET_OK;` |
|        - |  6096 | `	SyBlob sWorker;` |
|        - |  6097 | `	/* Format the message and call the output consumer */` |
|        3 |  6098 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6099 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6100 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6101 | `		/* Consume the formatted message */` |
|        3 |  6102 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6103 | `	}` |
|        3 |  6104 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6105 | `		/* Increment output length */` |
|      ! 0 |  6106 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6107 | `	}` |
|        - |  6108 | `	/* Release the working buffer */` |
|        3 |  6109 | `	SyBlobRelease(&sWorker);` |
|        3 |  6110 | `	return rc;` |
|        1 |  6111 |  |
|        - |  6112 | `/*` |
|        - |  6113 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6114 | ` * This function never fail and always return a pointer` |
|        - |  6115 | ` * to a null terminated string.` |
|        - |  6116 | ` */` |
|       10 |  6117 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6118 |  |
|       11 |  6119 | `	const char *zOp = "Unknown     ";` |
|       11 |  6120 | `	switch(nOp){` |
|        3 |  6121 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6122 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6123 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6124 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6125 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6126 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6127 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6128 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6129 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6130 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6131 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6132 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6133 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6134 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6135 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6136 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6137 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6138 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6139 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6140 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6141 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6142 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6143 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6144 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6145 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6146 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6147 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6148 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6149 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6150 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6151 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6152 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6153 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6154 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6157 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6162 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6168 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6169 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6181 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6194 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6201 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6202 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6203 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6205 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6208 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6210 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6211 | `	default:` |
|      ! 0 |  6212 | `		break;` |
|        - |  6213 | `	}` |
|       11 |  6214 | `	return zOp;` |
|        1 |  6215 |  |
|        - |  6216 | `/*` |
|        - |  6217 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6218 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6219 | ` * is responsible of consuming the generated dump.` |
|        - |  6220 | ` */` |
|        2 |  6221 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6222 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6223 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6224 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6225 | `	)` |
|        1 |  6226 |  |
|        - |  6227 | `	sxi32 rc;` |
|        3 |  6228 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6229 | `	return rc;` |
|        1 |  6230 |  |
|        - |  6231 | `/*` |
|        - |  6232 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6233 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6234 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6235 | ` * in 'compile.c' for additional information.` |
|        - |  6236 | ` */` |
|        8 |  6237 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6238 |  |
|        9 |  6239 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6240 | `	/* Evaluate and expand constant value */` |
|        9 |  6241 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6242 |  |
|        - |  6243 | `/*` |
|        - |  6244 | ` * Section:` |
|        - |  6245 | ` *  Function handling functions.` |
|        - |  6246 | ` * Status:` |
|        - |  6247 | ` *    Stable.` |
|        - |  6248 | ` */` |
|        - |  6249 | `/*` |
|        - |  6250 | ` * int func_num_args(void)` |
|        - |  6251 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6252 | ` * Parameters` |
|        - |  6253 | ` *   None.` |
|        - |  6254 | ` * Return` |
|        - |  6255 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6256 | ` *  or -1 if called from the globe scope.` |
|        - |  6257 | ` */` |
|      868 |  6258 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6259 |  |
|        - |  6260 | `	VmFrame *pFrame;` |
|        - |  6261 | `	ph7_vm *pVm;` |
|        - |  6262 | `	/* Point to the target VM */` |
|      870 |  6263 | `	pVm = pCtx->pVm;` |
|        - |  6264 | `	/* Current frame */` |
|      870 |  6265 | `	pFrame = pVm->pFrame;` |
|      870 |  6266 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6267 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6268 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6269 | `	}` |
|      870 |  6270 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6271 | `		SXUNUSED(nArg);` |
|      ! 0 |  6272 | `		SXUNUSED(apArg);` |
|        - |  6273 | `		/* Global frame,return -1 */` |
|      ! 0 |  6274 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6275 | `		return SXRET_OK;` |
|        - |  6276 | `	}` |
|        - |  6277 | `	/* Total number of arguments passed to the enclosing function */` |
|      870 |  6278 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      870 |  6279 | `	ph7_result_int(pCtx,nArg);` |
|      870 |  6280 | `	return SXRET_OK;` |
|      436 |  6281 |  |
|        - |  6282 | `/*` |
|        - |  6283 | ` * value func_get_arg(int $arg_num)` |
|        - |  6284 | ` *   Return an item from the argument list.` |
|        - |  6285 | ` * Parameters` |
|        - |  6286 | ` *  Argument number(index start from zero).` |
|        - |  6287 | ` * Return` |
|        - |  6288 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6289 | ` */` |
|       22 |  6290 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6291 |  |
|       24 |  6292 | `	ph7_value *pObj = 0;` |
|       24 |  6293 | `	VmSlot *pSlot = 0;` |
|        - |  6294 | `	VmFrame *pFrame;` |
|        - |  6295 | `	ph7_vm *pVm;` |
|        - |  6296 | `	/* Point to the target VM */` |
|       24 |  6297 | `	pVm = pCtx->pVm;` |
|        - |  6298 | `	/* Current frame */` |
|       24 |  6299 | `	pFrame = pVm->pFrame;` |
|       24 |  6300 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6301 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6302 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6303 | `	}` |
|       24 |  6304 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6305 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6306 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6307 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6308 | `		return SXRET_OK;` |
|        - |  6309 | `	}` |
|        - |  6310 | `	/* Extract the desired index */` |
|       21 |  6311 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6312 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6313 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6314 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6315 | `		return SXRET_OK;` |
|        - |  6316 | `	}` |
|        - |  6317 | `	/* Extract the desired argument */` |
|       21 |  6318 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6319 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6320 | `			/* Return the desired argument */` |
|       21 |  6321 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6322 | `		}else{` |
|        - |  6323 | `			/* No such argument,return false */` |
|      ! 0 |  6324 | `			ph7_result_bool(pCtx,0);` |
|        - |  6325 | `		}` |
|       11 |  6326 | `	}else{` |
|        - |  6327 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6328 | `		ph7_result_bool(pCtx,0);` |
|        - |  6329 | `	}` |
|       21 |  6330 | `	return SXRET_OK;` |
|       13 |  6331 |  |
|        - |  6332 | `/*` |
|        - |  6333 | ` * array func_get_args_byref(void)` |
|        - |  6334 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6335 | ` * Parameters` |
|        - |  6336 | ` *  None.` |
|        - |  6337 | ` * Return` |
|        - |  6338 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6339 | ` *  member of the current user-defined function's argument list.` |
|        - |  6340 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6341 | ` * NOTE:` |
|        - |  6342 | ` *  Arguments are returned to the array by reference.` |
|        - |  6343 | ` */` |
|        2 |  6344 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6345 |  |
|        - |  6346 | `	ph7_value *pArray;` |
|        - |  6347 | `	VmFrame *pFrame;` |
|        - |  6348 | `	VmSlot *aSlot;` |
|        - |  6349 | `	sxu32 n;` |
|        - |  6350 | `	/* Point to the current frame */` |
|        3 |  6351 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6352 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6353 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6354 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6355 | `	}` |
|        3 |  6356 | `	if( pFrame->pParent == 0 ){` |
|        - |  6357 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6358 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6359 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6360 | `		return SXRET_OK;` |
|        - |  6361 | `	}` |
|        - |  6362 | `	/* Create a new array */` |
|        3 |  6363 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6364 | `	if( pArray == 0 ){` |
|      ! 0 |  6365 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6366 | `		SXUNUSED(apArg);` |
|      ! 0 |  6367 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6368 | `		return SXRET_OK;` |
|        - |  6369 | `	}` |
|        - |  6370 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6371 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6372 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6373 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6374 | `	}` |
|        - |  6375 | `	/* Return the freshly created array */` |
|        3 |  6376 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6377 | `	return SXRET_OK;` |
|        2 |  6378 |  |
|        - |  6379 | `/*` |
|        - |  6380 | ` * array func_get_args(void)` |
|        - |  6381 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6382 | ` * Parameters` |
|        - |  6383 | ` *  None.` |
|        - |  6384 | ` * Return` |
|        - |  6385 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6386 | ` *  member of the current user-defined function's argument list.` |
|        - |  6387 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6388 | ` */` |
|       46 |  6389 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6390 |  |
|       47 |  6391 | `	ph7_value *pObj = 0;` |
|        - |  6392 | `	ph7_value *pArray;` |
|        - |  6393 | `	VmFrame *pFrame;` |
|        - |  6394 | `	VmSlot *aSlot;` |
|        - |  6395 | `	sxu32 n;` |
|        - |  6396 | `	/* Point to the current frame */` |
|       47 |  6397 | `	pFrame = pCtx->pVm->pFrame;` |
|       47 |  6398 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6399 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6400 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6401 | `	}` |
|       47 |  6402 | `	if( pFrame->pParent == 0 ){` |
|        - |  6403 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6404 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6405 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6406 | `		return SXRET_OK;` |
|        - |  6407 | `	}` |
|        - |  6408 | `	/* Create a new array */` |
|       47 |  6409 | `	pArray = ph7_context_new_array(pCtx);` |
|       47 |  6410 | `	if( pArray == 0 ){` |
|      ! 0 |  6411 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6412 | `		SXUNUSED(apArg);` |
|      ! 0 |  6413 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6414 | `		return SXRET_OK;` |
|        - |  6415 | `	}` |
|        - |  6416 | `	/* Start filling the array with the given arguments */` |
|       47 |  6417 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      143 |  6418 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|       97 |  6419 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|       97 |  6420 | `		if( pObj ){` |
|       97 |  6421 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       48 |  6422 | `		}` |
|       49 |  6423 | `	}` |
|        - |  6424 | `	/* Return the freshly created array */` |
|       47 |  6425 | `	ph7_result_value(pCtx,pArray);` |
|       47 |  6426 | `	return SXRET_OK;` |
|       24 |  6427 |  |
|        - |  6428 | `/*` |
|        - |  6429 | ` * bool function_exists(string $name)` |
|        - |  6430 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6431 | ` * Parameters` |
|        - |  6432 | ` *  The name of the desired function.` |
|        - |  6433 | ` * Return` |
|        - |  6434 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6435 | ` */` |
|     1666 |  6436 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6437 |  |
|        - |  6438 | `	const char *zName;` |
|        - |  6439 | `	ph7_vm *pVm;` |
|        - |  6440 | `	int nLen;` |
|        - |  6441 | `	int res;` |
|     1668 |  6442 | `	if( nArg < 1 ){` |
|        - |  6443 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6444 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6445 | `		return SXRET_OK;` |
|        - |  6446 | `	}` |
|        - |  6447 | `	/* Point to the target VM */` |
|     1668 |  6448 | `	pVm = pCtx->pVm;` |
|        - |  6449 | `	/* Extract the function name */` |
|     1668 |  6450 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6451 | `	/* Assume the function is not defined */` |
|     1668 |  6452 | `	res = 0;` |
|        - |  6453 | `	/* Perform the lookup */` |
|     2499 |  6454 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1662 |  6455 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6456 | `			/* Function is defined */` |
|      212 |  6457 | `			res = 1;` |
|      105 |  6458 | `	}` |
|     1668 |  6459 | `	ph7_result_bool(pCtx,res);` |
|     1668 |  6460 | `	return SXRET_OK;` |
|      835 |  6461 |  |
|        - |  6462 | `/*` |
|        - |  6463 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6464 | ` * [i.e: Whether it is callable or not].` |
|        - |  6465 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6466 | ` */` |
|    15836 |  6467 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6468 |  |
|    15838 |  6469 | `	int res = 0;` |
|    15838 |  6470 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6471 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6472 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6473 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6474 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6475 | `		if( pMethod && CallInvoke ){` |
|        - |  6476 | `			ph7_value sResult;` |
|        - |  6477 | `			sxi32 rc;` |
|        - |  6478 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6479 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6480 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6481 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6482 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6483 | `			}` |
|      ! 0 |  6484 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6485 | `		}` |
|    15838 |  6486 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6487 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       20 |  6488 | `		if( pMap->nEntry == 2 ){` |
|        - |  6489 | `			ph7_class *pClass;` |
|        - |  6490 | `			ph7_value *pV;` |
|        - |  6491 | `			/* Extract the target class */` |
|        7 |  6492 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|        7 |  6493 | `			if( pV ){` |
|        7 |  6494 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|        7 |  6495 | `				if( pClass ){` |
|        - |  6496 | `					ph7_class_method *pMethod;` |
|        - |  6497 | `					/* Extract the target method */` |
|        7 |  6498 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|        7 |  6499 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6500 | `						/* Perform the lookup */` |
|        7 |  6501 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|        7 |  6502 | `						if( pMethod ){` |
|        - |  6503 | `							/* Method is callable */` |
|        5 |  6504 | `							res = 1;` |
|        2 |  6505 | `						}` |
|        3 |  6506 | `					}` |
|        3 |  6507 | `				}` |
|        3 |  6508 | `			}` |
|        5 |  6509 | `		}` |
|    15829 |  6510 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6511 | `		const char *zName;` |
|        - |  6512 | `		int nLen;` |
|        - |  6513 | `		/* Extract the name */` |
|     4658 |  6514 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6515 | `		/* Perform the lookup */` |
|     4671 |  6516 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       26 |  6517 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6518 | `				/* Function is callable */` |
|     4644 |  6519 | `				res = 1;` |
|     2321 |  6520 | `		}` |
|     2328 |  6521 | `	}` |
|    15838 |  6522 | `	return res;` |
|        2 |  6523 |  |
|        - |  6524 | `/*` |
|        - |  6525 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6526 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6527 | ` * Parameters` |
|        - |  6528 | ` * $name` |
|        - |  6529 | ` *    The callback function to check` |
|        - |  6530 | ` * $syntax_only` |
|        - |  6531 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6532 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6533 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6534 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6535 | ` *    a string.` |
|        - |  6536 | ` * Return` |
|        - |  6537 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6538 | ` */` |
|       14 |  6539 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6540 |  |
|        - |  6541 | `	ph7_vm *pVm;` |
|        - |  6542 | `	int res;` |
|       15 |  6543 | `	if( nArg < 1 ){` |
|        - |  6544 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6545 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6546 | `		return SXRET_OK;` |
|        - |  6547 | `	}` |
|        - |  6548 | `	/* Point to the target VM */` |
|       15 |  6549 | `	pVm = pCtx->pVm;` |
|        - |  6550 | `	/* Perform the requested operation */` |
|       15 |  6551 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6552 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6553 | `	return SXRET_OK;` |
|        8 |  6554 |  |
|        - |  6555 | `/*` |
|        - |  6556 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6557 | ` * defined below.` |
|        - |  6558 | ` */` |
|     1074 |  6559 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6560 |  |
|     1075 |  6561 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6562 | `	ph7_value sName;` |
|        - |  6563 | `	sxi32 rc;` |
|        - |  6564 | `	/* Prepare the function name for insertion */` |
|     1075 |  6565 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1075 |  6566 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6567 | `	/* Perform the insertion */` |
|     1075 |  6568 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1075 |  6569 | `	PH7_MemObjRelease(&sName);` |
|     1075 |  6570 | `	return rc;` |
|        1 |  6571 |  |
|        - |  6572 | `/*` |
|        - |  6573 | ` * array get_defined_functions(void)` |
|        - |  6574 | ` *  Returns an array of all defined functions.` |
|        - |  6575 | ` * Parameter` |
|        - |  6576 | ` *  None.` |
|        - |  6577 | ` * Return` |
|        - |  6578 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6579 | ` *  both built-in (internal) and user-defined.` |
|        - |  6580 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6581 | ` *  defined ones using $arr["user"].` |
|        - |  6582 | ` * Note:` |
|        - |  6583 | ` *  NULL is returned on failure.` |
|        - |  6584 | ` */` |
|        2 |  6585 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6586 |  |
|        - |  6587 | `	ph7_value *pArray,*pEntry;` |
|        - |  6588 | `	/* NOTE:` |
|        - |  6589 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6590 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6591 | `	 */` |
|        3 |  6592 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6593 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6594 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6595 | `		SXUNUSED(apArg);` |
|        - |  6596 | `		/* Return NULL */` |
|      ! 0 |  6597 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6598 | `		return SXRET_OK;` |
|        - |  6599 | `	}` |
|        3 |  6600 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6601 | `	if( pEntry == 0 ){` |
|        - |  6602 | `		/* Return NULL */` |
|      ! 0 |  6603 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6604 | `		return SXRET_OK;` |
|        - |  6605 | `	}` |
|        - |  6606 | `	/* Fill with the appropriate information */` |
|        3 |  6607 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6608 | `	/* Create the 'internal' index */` |
|        3 |  6609 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6610 | `	/* Create the user-func array */` |
|        3 |  6611 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6612 | `	if( pEntry == 0 ){` |
|        - |  6613 | `		/* Return NULL */` |
|      ! 0 |  6614 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6615 | `		return SXRET_OK;` |
|        - |  6616 | `	}` |
|        - |  6617 | `	/* Fill with the appropriate information */` |
|        3 |  6618 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6619 | `	/* Create the 'user' index */` |
|        3 |  6620 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6621 | `	/* Return the multi-dimensional array */` |
|        3 |  6622 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6623 | `	return SXRET_OK;` |
|        2 |  6624 |  |
|        - |  6625 | `/*` |
|        - |  6626 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6627 | ` *  Register a function for execution on shutdown.` |
|        - |  6628 | ` * Note` |
|        - |  6629 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6630 | ` *  be called in the same order as they were registered.` |
|        - |  6631 | ` * Parameters` |
|        - |  6632 | ` *  $callback` |
|        - |  6633 | ` *   The shutdown callback to register.` |
|        - |  6634 | ` * $param` |
|        - |  6635 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6636 | ` * Return` |
|        - |  6637 | ` *  Nothing.` |
|        - |  6638 | ` */` |
|        2 |  6639 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6640 |  |
|        - |  6641 | `	VmShutdownCB sEntry;` |
|        - |  6642 | `	int i,j;` |
|        3 |  6643 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6644 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6645 | `		return PH7_OK;` |
|        - |  6646 | `	}` |
|        - |  6647 | `	/* Zero the Entry */` |
|        3 |  6648 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6649 | `	/* Initialize fields */` |
|        3 |  6650 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6651 | `	/* Save the callback name for later invocation name */` |
|        3 |  6652 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6653 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6654 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6655 | `	}` |
|        - |  6656 | `	/* Copy arguments */` |
|        3 |  6657 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6658 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6659 | `			/* Limit reached */` |
|      ! 0 |  6660 | `			break;` |
|        - |  6661 | `		}` |
|      ! 0 |  6662 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6663 | `	}` |
|        3 |  6664 | `	sEntry.nArg = j;` |
|        - |  6665 | `	/* Install the callback */` |
|        3 |  6666 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6667 | `	return PH7_OK;` |
|        2 |  6668 |  |
|        - |  6669 | `/*` |
|        - |  6670 | ` * Section:` |
|        - |  6671 | ` *  Class handling functions.` |
|        - |  6672 | ` * Status:` |
|        - |  6673 | ` *    Stable.` |
|        - |  6674 | ` */` |
|        - |  6675 | `/*` |
|        - |  6676 | ` * Extract the top active class. NULL is returned` |
|        - |  6677 | ` * if the class stack is empty.` |
|        - |  6678 | ` */` |
|      400 |  6679 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6680 |  |
|      402 |  6681 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6682 | `	ph7_class **apClass;` |
|      402 |  6683 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6684 | `		/* Empty stack,return NULL */` |
|       15 |  6685 | `		return 0;` |
|        - |  6686 | `	}` |
|        - |  6687 | `	/* Peek the last entry */` |
|      388 |  6688 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      388 |  6689 | `	return apClass[pSet->nUsed - 1];` |
|      202 |  6690 |  |
|        - |  6691 | `/*` |
|        - |  6692 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6693 | ` *   Get the class that declared the currently executing method.` |
|        - |  6694 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6695 | ` *` |
|        - |  6696 | ` * Parameters` |
|        - |  6697 | ` *   pVm: Target VM` |
|        - |  6698 | ` *` |
|        - |  6699 | ` * Return` |
|        - |  6700 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6701 | ` *   - Not executing within a class method` |
|        - |  6702 | ` *` |
|        - |  6703 | ` * Note` |
|        - |  6704 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6705 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6706 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6707 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6708 | ` *   declaring class.` |
|        - |  6709 | ` */` |
|       18 |  6710 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6711 |  |
|       19 |  6712 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6713 | `	ph7_vm_func *pVmFunc;` |
|        - |  6714 |  |
|        - |  6715 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6716 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6717 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6718 | `	}` |
|        - |  6719 |  |
|        - |  6720 | `	/* Check if we're in a method context */` |
|       19 |  6721 | `	if( pFrame->pParent ){` |
|       15 |  6722 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6723 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6724 | `			/* Return the declaring class */` |
|       15 |  6725 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6726 | `		}` |
|      ! 0 |  6727 | `	}` |
|        - |  6728 |  |
|        5 |  6729 | `	return 0;` |
|       10 |  6730 |  |
|        - |  6731 |  |
|        - |  6732 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6733 | `/*` |
|        - |  6734 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6735 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6736 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6737 | ` * return value indicates failure.` |
|        - |  6738 | ` */` |
|      918 |  6739 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6740 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6741 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6742 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6743 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6744 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6745 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6746 | `	)` |
|        2 |  6747 |  |
|        - |  6748 | `	ph7_value *aStack;` |
|        - |  6749 | `	VmInstr aInstr[2];` |
|        - |  6750 | `	int iCursor;` |
|        - |  6751 | `	int i;` |
|        - |  6752 | `	/* Create a new operand stack */` |
|      920 |  6753 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|      920 |  6754 | `	if( aStack == 0 ){` |
|      ! 0 |  6755 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6756 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6757 | `		return SXERR_MEM;` |
|        - |  6758 | `	}` |
|        - |  6759 | `	/* Fill the operand stack with the given arguments */` |
|     1350 |  6760 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      432 |  6761 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6762 | `		/*` |
|        - |  6763 | `		 * Symisc eXtension:` |
|        - |  6764 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6765 | `		 */` |
|      432 |  6766 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      217 |  6767 | `	}` |
|      920 |  6768 | `	iCursor = nArg + 1;` |
|      920 |  6769 | `	if( pThis ){` |
|        - |  6770 | `		/*` |
|        - |  6771 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6772 | `		 */` |
|      914 |  6773 | `		pThis->iRef++; /* Increment reference count */` |
|      914 |  6774 | `		aStack[i].x.pOther = pThis;` |
|      914 |  6775 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      456 |  6776 | `	}` |
|      920 |  6777 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|      920 |  6778 | `	i++;` |
|        - |  6779 | `	/* Push method name */` |
|      920 |  6780 | `	SyBlobReset(&aStack[i].sBlob);` |
|      920 |  6781 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|      920 |  6782 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|      920 |  6783 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6784 | `	/* Emit the CALL istruction */` |
|      920 |  6785 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      920 |  6786 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      920 |  6787 | `	aInstr[0].iP2 = 0;` |
|      920 |  6788 | `	aInstr[0].p3  = 0;` |
|        - |  6789 | `	/* Emit the DONE instruction */` |
|      920 |  6790 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      920 |  6791 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|      920 |  6792 | `	aInstr[1].iP2 = 0;` |
|      920 |  6793 | `	aInstr[1].p3  = 0;` |
|        - |  6794 | `	/* Execute the method body (if available) */` |
|      920 |  6795 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6796 | `	/* Clean up the mess left behind */` |
|      920 |  6797 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      920 |  6798 | `	return PH7_OK;` |
|      461 |  6799 |  |
|        - |  6800 | `/*` |
|        - |  6801 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6802 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6803 | ` * in the apArg[] array.` |
|        - |  6804 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6805 | ` * return value indicates failure.` |
|        - |  6806 | ` */` |
|      800 |  6807 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6808 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6809 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6810 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6811 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6812 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6813 | `	)` |
|        2 |  6814 |  |
|        - |  6815 | `	ph7_value *aStack;` |
|        - |  6816 | `	VmInstr aInstr[2];` |
|        - |  6817 | `	int i;` |
|      802 |  6818 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6819 | `		/* Don't bother processing,it's invalid anyway */` |
|      359 |  6820 | `		if( pResult ){` |
|        - |  6821 | `			/* Assume a null return value */` |
|      ! 0 |  6822 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6823 | `		}` |
|      359 |  6824 | `		return SXERR_INVALID;` |
|        - |  6825 | `	}` |
|      444 |  6826 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6827 | `		/* Class method */` |
|       11 |  6828 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6829 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6830 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6831 | `		ph7_class *pClass = 0;` |
|        - |  6832 | `		ph7_value *pValue;` |
|        - |  6833 | `		sxi32 rc;` |
|       11 |  6834 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6835 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6836 | `			if( pResult ){` |
|        - |  6837 | `				/* Assume a null return value */` |
|      ! 0 |  6838 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6839 | `			}` |
|      ! 0 |  6840 | `			return SXRET_OK;` |
|        - |  6841 | `		}` |
|        - |  6842 | `		/* Extract the class name or an instance of it */` |
|       11 |  6843 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  6844 | `		if( pValue ){` |
|       11 |  6845 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  6846 | `		}` |
|       11 |  6847 | `		if( pClass == 0 ){` |
|        - |  6848 | `			/* No such class,return NULL */` |
|      ! 0 |  6849 | `			if( pResult ){` |
|      ! 0 |  6850 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6851 | `			}` |
|      ! 0 |  6852 | `			return SXRET_OK;` |
|        - |  6853 | `		}` |
|       11 |  6854 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6855 | `			/* Point to the class instance */` |
|        5 |  6856 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  6857 | `		}` |
|        - |  6858 | `		/* Try to extract the method */` |
|       11 |  6859 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  6860 | `		if( pValue ){` |
|       11 |  6861 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  6862 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  6863 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  6864 | `			}` |
|        5 |  6865 | `		}` |
|       11 |  6866 | `		if( pMethod == 0 ){` |
|        - |  6867 | `			/* No such method,return NULL */` |
|      ! 0 |  6868 | `			if( pResult ){` |
|      ! 0 |  6869 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6870 | `			}` |
|      ! 0 |  6871 | `			return SXRET_OK;` |
|        - |  6872 | `		}` |
|        - |  6873 | `		/* Call the class method */` |
|       11 |  6874 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  6875 | `		return rc;` |
|        - |  6876 | `	}` |
|        - |  6877 | `	/* Create a new operand stack */` |
|      434 |  6878 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      434 |  6879 | `	if( aStack == 0 ){` |
|      ! 0 |  6880 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6881 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  6882 | `		if( pResult ){` |
|        - |  6883 | `			/* Assume a null return value */` |
|      ! 0 |  6884 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6885 | `		}` |
|      ! 0 |  6886 | `		return SXERR_MEM;` |
|        - |  6887 | `	}` |
|        - |  6888 | `	/* Fill the operand stack with the given arguments */` |
|     1428 |  6889 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      996 |  6890 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6891 | `		/*` |
|        - |  6892 | `		 * Symisc eXtension:` |
|        - |  6893 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6894 | `		 */` |
|      996 |  6895 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      499 |  6896 | `	}` |
|        - |  6897 | `	/* Push the function name */` |
|      434 |  6898 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      434 |  6899 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  6900 | `	/* Emit the CALL istruction */` |
|      434 |  6901 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      434 |  6902 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      434 |  6903 | `	aInstr[0].iP2 = 0;` |
|      434 |  6904 | `	aInstr[0].p3  = 0;` |
|        - |  6905 | `	/* Emit the DONE instruction */` |
|      434 |  6906 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      434 |  6907 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      434 |  6908 | `	aInstr[1].iP2 = 0;` |
|      434 |  6909 | `	aInstr[1].p3  = 0;` |
|        - |  6910 | `	/* Execute the function body (if available) */` |
|      434 |  6911 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  6912 | `	/* Clean up the mess left behind */` |
|      434 |  6913 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      434 |  6914 | `	return PH7_OK;` |
|      402 |  6915 |  |
|        - |  6916 | `/*` |
|        - |  6917 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  6918 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  6919 | ` * parameter.` |
|        - |  6920 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6921 | ` * return value indicates failure.` |
|        - |  6922 | ` */` |
|      236 |  6923 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  6924 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6925 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6926 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  6927 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  6928 | `	)` |
|        1 |  6929 |  |
|        - |  6930 | `	ph7_value *pArg;` |
|        - |  6931 | `	SySet aArg;` |
|        - |  6932 | `	va_list ap;` |
|        - |  6933 | `	sxi32 rc;` |
|      237 |  6934 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  6935 | `	/* Copy arguments one after one */` |
|      237 |  6936 | `	va_start(ap,pResult);` |
|      393 |  6937 | `	for(;;){` |
|      787 |  6938 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  6939 | `		if( pArg == 0 ){` |
|      237 |  6940 | `			break;` |
|        - |  6941 | `		}` |
|      551 |  6942 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  6943 | `	}` |
|        - |  6944 | `	/* Call the core routine */` |
|      237 |  6945 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  6946 | `	/* Cleanup */` |
|      237 |  6947 | `	SySetRelease(&aArg);` |
|      237 |  6948 | `	return rc;` |
|        1 |  6949 |  |
|        - |  6950 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  6951 | `/*` |
|        - |  6952 | ` * bool defined(string $name)` |
|        - |  6953 | ` *  Checks whether a given named constant exists.` |
|        - |  6954 | ` * Parameter:` |
|        - |  6955 | ` *  Name of the desired constant.` |
|        - |  6956 | ` * Return` |
|        - |  6957 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  6958 | ` */` |
|       14 |  6959 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6960 |  |
|        - |  6961 | `	const char *zName;` |
|       16 |  6962 | `	int nLen = 0;` |
|       16 |  6963 | `	int res = 0;` |
|       16 |  6964 | `	if( nArg < 1 ){` |
|        - |  6965 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  6966 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  6967 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6968 | `		return SXRET_OK;` |
|        - |  6969 | `	}` |
|        - |  6970 | `	/* Extract constant name */` |
|       16 |  6971 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6972 | `	/* Perform the lookup */` |
|       16 |  6973 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6974 | `		/* Already defined */` |
|       10 |  6975 | `		res = 1;` |
|        4 |  6976 | `	}` |
|       16 |  6977 | `	ph7_result_bool(pCtx,res);` |
|       16 |  6978 | `	return SXRET_OK;` |
|        9 |  6979 |  |
|        - |  6980 | `/*` |
|        - |  6981 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  6982 | ` * below.` |
|        - |  6983 | ` */` |
|        8 |  6984 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  6985 |  |
|       10 |  6986 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  6987 | `	/* Expand constant value */` |
|       10 |  6988 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  6989 |  |
|        - |  6990 | `/*` |
|        - |  6991 | ` * bool define(string $constant_name,expression value)` |
|        - |  6992 | ` *  Defines a named constant at runtime.` |
|        - |  6993 | ` * Parameter:` |
|        - |  6994 | ` *  $constant_name` |
|        - |  6995 | ` *   The name of the constant` |
|        - |  6996 | ` *  $value` |
|        - |  6997 | ` *   Constant value` |
|        - |  6998 | ` * Return:` |
|        - |  6999 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7000 | ` */` |
|       10 |  7001 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7002 |  |
|        - |  7003 | `	const char *zName;  /* Constant name */` |
|        - |  7004 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7005 | `	int nLen = 0;       /* Name length */` |
|        - |  7006 | `	sxi32 rc;` |
|       12 |  7007 | `	if( nArg < 2 ){` |
|        - |  7008 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7009 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7010 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7011 | `		return SXRET_OK;` |
|        - |  7012 | `	}` |
|       12 |  7013 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7014 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7015 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7016 | `		return SXRET_OK;` |
|        - |  7017 | `	}` |
|        - |  7018 | `	/* Extract constant name */` |
|       12 |  7019 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7020 | `	if( nLen < 1 ){` |
|      ! 0 |  7021 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7022 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7023 | `		return SXRET_OK;` |
|        - |  7024 | `	}` |
|        - |  7025 | `	/* Duplicate constant value */` |
|       12 |  7026 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7027 | `	if( pValue == 0 ){` |
|      ! 0 |  7028 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7029 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7030 | `		return SXRET_OK;` |
|        - |  7031 | `	}` |
|        - |  7032 | `	/* Initialize the memory object */` |
|       12 |  7033 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7034 | `	/* Register the constant */` |
|       12 |  7035 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7036 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7037 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7038 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7039 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7040 | `		return SXRET_OK;` |
|        - |  7041 | `	}` |
|        - |  7042 | `	/* Duplicate constant value */` |
|       12 |  7043 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7044 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7045 | `		/* Lower case the constant name */` |
|      ! 0 |  7046 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7047 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7048 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7049 | `				/* UTF-8 stream */` |
|      ! 0 |  7050 | `				zCur++;` |
|      ! 0 |  7051 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7052 | `					zCur++;` |
|      ! 0 |  7053 | `				}` |
|      ! 0 |  7054 | `				continue;` |
|        - |  7055 | `			}` |
|      ! 0 |  7056 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7057 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7058 | `				zCur[0] = (char)c;` |
|      ! 0 |  7059 | `			}` |
|      ! 0 |  7060 | `			zCur++;` |
|      ! 0 |  7061 | `		}` |
|        - |  7062 | `		/* Finally,register the constant */` |
|      ! 0 |  7063 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7064 | `	}` |
|        - |  7065 | `	/* All done,return TRUE */` |
|       12 |  7066 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7067 | `	return SXRET_OK;` |
|        7 |  7068 |  |
|        - |  7069 | `/*` |
|        - |  7070 | ` * value constant(string $name)` |
|        - |  7071 | ` *  Returns the value of a constant` |
|        - |  7072 | ` * Parameter` |
|        - |  7073 | ` *  $name` |
|        - |  7074 | ` *    Name of the constant.` |
|        - |  7075 | ` * Return` |
|        - |  7076 | ` *  Constant value or NULL if not defined.` |
|        - |  7077 | ` */` |
|        8 |  7078 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7079 |  |
|        - |  7080 | `	SyHashEntry *pEntry;` |
|        - |  7081 | `	ph7_constant *pCons;` |
|        - |  7082 | `	const char *zName; /* Constant name */` |
|        - |  7083 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7084 | `	int nLen;` |
|       10 |  7085 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7086 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7087 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7088 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7089 | `		return SXRET_OK;` |
|        - |  7090 | `	}` |
|        - |  7091 | `	/* Extract the constant name */` |
|       10 |  7092 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7093 | `	/* Perform the query */` |
|       10 |  7094 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7095 | `	if( pEntry == 0 ){` |
|        3 |  7096 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7097 | `		ph7_result_null(pCtx);` |
|        3 |  7098 | `		return SXRET_OK;` |
|        - |  7099 | `	}` |
|        8 |  7100 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7101 | `	/* Point to the structure that describe the constant */` |
|        8 |  7102 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7103 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7104 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7105 | `	/* Return that value */` |
|        8 |  7106 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7107 | `	/* Cleanup */` |
|        8 |  7108 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7109 | `	return SXRET_OK;` |
|        6 |  7110 |  |
|        - |  7111 | `/*` |
|        - |  7112 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7113 | ` * defined below.` |
|        - |  7114 | ` */` |
|      414 |  7115 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7116 |  |
|      415 |  7117 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7118 | `	ph7_value sName;` |
|        - |  7119 | `	sxi32 rc;` |
|        - |  7120 | `	/* Prepare the constant name for insertion */` |
|      415 |  7121 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      415 |  7122 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7123 | `	/* Perform the insertion */` |
|      415 |  7124 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      415 |  7125 | `	PH7_MemObjRelease(&sName);` |
|      415 |  7126 | `	return rc;` |
|        1 |  7127 |  |
|        - |  7128 | `/*` |
|        - |  7129 | ` * array get_defined_constants(void)` |
|        - |  7130 | ` *  Returns an associative array with the names of all defined` |
|        - |  7131 | ` *  constants.` |
|        - |  7132 | ` * Parameters` |
|        - |  7133 | ` *  NONE.` |
|        - |  7134 | ` * Returns` |
|        - |  7135 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7136 | ` */` |
|        2 |  7137 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7138 |  |
|        - |  7139 | `	ph7_value *pArray;` |
|        - |  7140 | `	/* Create the array first*/` |
|        3 |  7141 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7142 | `	if( pArray == 0 ){` |
|      ! 0 |  7143 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7144 | `		SXUNUSED(apArg);` |
|        - |  7145 | `		/* Return NULL */` |
|      ! 0 |  7146 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7147 | `		return SXRET_OK;` |
|        - |  7148 | `	}` |
|        - |  7149 | `	/* Fill the array with the defined constants */` |
|        3 |  7150 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7151 | `	/* Return the created array */` |
|        3 |  7152 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7153 | `	return SXRET_OK;` |
|        2 |  7154 |  |
|        - |  7155 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7156 | `/*` |
|        - |  7157 | ` * Section:` |
|        - |  7158 | ` *  Random numbers/string generators.` |
|        - |  7159 | ` * Status:` |
|        - |  7160 | ` *    Stable.` |
|        - |  7161 | ` */` |
|        - |  7162 | `/*` |
|        - |  7163 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7164 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7165 | ` * used by te SQLite3 library.` |
|        - |  7166 | ` */` |
|     1744 |  7167 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7168 |  |
|        - |  7169 | `	sxu32 iNum;` |
|     1746 |  7170 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     1746 |  7171 | `	return iNum;` |
|        2 |  7172 |  |
|        - |  7173 | `/*` |
|        - |  7174 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7175 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7176 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7177 | ` * by te SQLite3 library.` |
|        - |  7178 | ` */` |
|    55744 |  7179 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7180 |  |
|        - |  7181 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7182 | `	int i;` |
|        - |  7183 | `	/* Generate a binary string first */` |
|    55746 |  7184 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7185 | `	/* Turn the binary string into english based alphabet */` |
|   613354 |  7186 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   557610 |  7187 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   278806 |  7188 | `	 }` |
|    55746 |  7189 |  |
|        - |  7190 | `/*` |
|        - |  7191 | ` * int rand()` |
|        - |  7192 | ` * int mt_rand()` |
|        - |  7193 | ` * int rand(int $min,int $max)` |
|        - |  7194 | ` * int mt_rand(int $min,int $max)` |
|        - |  7195 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7196 | ` * Parameter` |
|        - |  7197 | ` *  $min` |
|        - |  7198 | ` *    The lowest value to return (default: 0)` |
|        - |  7199 | ` *  $max` |
|        - |  7200 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7201 | ` * Return` |
|        - |  7202 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7203 | ` * Note:` |
|        - |  7204 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7205 | ` *  by te SQLite3 library.` |
|        - |  7206 | ` */` |
|       20 |  7207 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7208 |  |
|        - |  7209 | `	sxu32 iNum;` |
|        - |  7210 | `	/* Generate the random number */` |
|       21 |  7211 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7212 | `	if( nArg > 1 ){` |
|        - |  7213 | `		sxu32 iMin,iMax;` |
|        3 |  7214 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7215 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7216 | `		if( iMin < iMax ){` |
|        3 |  7217 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7218 | `			if( iDiv > 0 ){` |
|        3 |  7219 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7220 | `			}` |
|        1 |  7221 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7222 | `			iNum %= iMax;` |
|      ! 0 |  7223 | `		}` |
|        1 |  7224 | `	}` |
|        - |  7225 | `	/* Return the number */` |
|       21 |  7226 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7227 | `	return SXRET_OK;` |
|        1 |  7228 |  |
|        - |  7229 | `/*` |
|        - |  7230 | ` * int getrandmax(void)` |
|        - |  7231 | ` * int mt_getrandmax(void)` |
|        - |  7232 | ` * int rc4_getrandmax(void)` |
|        - |  7233 | ` *   Show largest possible random value` |
|        - |  7234 | ` * Return` |
|        - |  7235 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7236 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7237 | ` * Note:` |
|        - |  7238 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7239 | ` *  by te SQLite3 library.` |
|        - |  7240 | ` */` |
|        4 |  7241 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7242 |  |
|        2 |  7243 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7244 | `	SXUNUSED(apArg);` |
|        5 |  7245 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7246 | `	return SXRET_OK;` |
|        1 |  7247 |  |
|        - |  7248 | `/*` |
|        - |  7249 | ` * string rand_str()` |
|        - |  7250 | ` * string rand_str(int $len)` |
|        - |  7251 | ` *  Generate a random string (English alphabet).` |
|        - |  7252 | ` * Parameter` |
|        - |  7253 | ` *  $len` |
|        - |  7254 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7255 | ` * Return` |
|        - |  7256 | ` *   A pseudo random string.` |
|        - |  7257 | ` * Note:` |
|        - |  7258 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7259 | ` *  by te SQLite3 library.` |
|        - |  7260 | ` *  This function is a symisc extension.` |
|        - |  7261 | ` */` |
|      120 |  7262 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7263 |  |
|        - |  7264 | `	char zString[1024];` |
|      122 |  7265 | `	int iLen = 0x10;` |
|      122 |  7266 | `	if( nArg > 0 ){` |
|        - |  7267 | `		/* Get the desired length */` |
|      122 |  7268 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7269 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7270 | `			/* Default length */` |
|        3 |  7271 | `			iLen = 0x10;` |
|        1 |  7272 | `		}` |
|       60 |  7273 | `	}` |
|        - |  7274 | `	/* Generate the random string */` |
|      122 |  7275 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7276 | `	/* Return the generated string */` |
|      122 |  7277 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7278 | `	return SXRET_OK;` |
|        2 |  7279 |  |
|        - |  7280 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7281 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7282 | `/* Unique ID private data */` |
|        - |  7283 | `struct unique_id_data` |
|        - |  7284 |  |
|        - |  7285 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7286 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7287 | `};` |
|        - |  7288 | `/*` |
|        - |  7289 | ` * Binary to hex consumer callback.` |
|        - |  7290 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7291 | ` * defined below.` |
|        - |  7292 | ` */` |
|      192 |  7293 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7294 |  |
|      193 |  7295 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7296 | `	sxu32 nBuflen;` |
|        - |  7297 | `	/* Extract result buffer length */` |
|      193 |  7298 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7299 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7300 | `			/*` |
|        - |  7301 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7302 | `			 * string will be 13 characters long` |
|        - |  7303 | `			 */` |
|       25 |  7304 | `		return SXERR_ABORT;` |
|        - |  7305 | `	}` |
|      169 |  7306 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7307 | `		return SXERR_ABORT;` |
|        - |  7308 | `	}` |
|        - |  7309 | `	/* Safely Consume the hex stream */` |
|      169 |  7310 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7311 | `	return SXRET_OK;` |
|       97 |  7312 |  |
|        - |  7313 | `/*` |
|        - |  7314 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7315 | ` *  Generate a unique ID` |
|        - |  7316 | ` * Parameter` |
|        - |  7317 | ` * $prefix` |
|        - |  7318 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7319 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7320 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7321 | ` * $more_entropy` |
|        - |  7322 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7323 | ` *  that the result will be unique.` |
|        - |  7324 | ` * Return` |
|        - |  7325 | ` *  Returns the unique identifier, as a string.` |
|        - |  7326 | ` */` |
|       24 |  7327 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7328 |  |
|        - |  7329 | `	struct unique_id_data sUniq;` |
|        - |  7330 | `	unsigned char zDigest[20];` |
|       25 |  7331 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7332 | `	const char *zPrefix;` |
|        - |  7333 | `	SHA1Context sCtx;` |
|        - |  7334 | `	char zRandom[7];` |
|        - |  7335 | `	int nPrefix;` |
|        - |  7336 | `	int entropy;` |
|        - |  7337 | `	/* Generate a random string first */` |
|       25 |  7338 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7339 | `	/* Initialize fields */` |
|       25 |  7340 | `	zPrefix = 0;` |
|       25 |  7341 | `	nPrefix = 0;` |
|       25 |  7342 | `	entropy = 0;` |
|       25 |  7343 | `	if( nArg > 0 ){` |
|        - |  7344 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7345 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7346 | `		if( nArg > 1 ){` |
|      ! 0 |  7347 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7348 | `		}` |
|      ! 0 |  7349 | `	}` |
|       25 |  7350 | `	SHA1Init(&sCtx);` |
|        - |  7351 | `	/* Generate the random ID */` |
|       25 |  7352 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7353 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7354 | `	}` |
|        - |  7355 | `	/* Append the random ID */` |
|       25 |  7356 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7357 | `	/* Append the random string */` |
|       25 |  7358 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7359 | `	/* Increment the number */` |
|       25 |  7360 | `	pVm->unique_id++;` |
|       25 |  7361 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7362 | `	/* Hexify the digest */` |
|       25 |  7363 | `	sUniq.pCtx = pCtx;` |
|       25 |  7364 | `	sUniq.entropy = entropy;` |
|       25 |  7365 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7366 | `	/* All done */` |
|       25 |  7367 | `	return PH7_OK;` |
|        1 |  7368 |  |
|        - |  7369 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7370 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7371 | `/*` |
|        - |  7372 | ` * Section:` |
|        - |  7373 | ` *  Language construct implementation as foreign functions.` |
|        - |  7374 | ` * Status:` |
|        - |  7375 | ` *    Stable.` |
|        - |  7376 | ` */` |
|        - |  7377 | `/*` |
|        - |  7378 | ` * void echo($string...)` |
|        - |  7379 | ` *  Output one or more messages.` |
|        - |  7380 | ` * Parameters` |
|        - |  7381 | ` *  $string` |
|        - |  7382 | ` *   Message to output.` |
|        - |  7383 | ` * Return` |
|        - |  7384 | ` *  NULL.` |
|        - |  7385 | ` */` |
|      ! 0 |  7386 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7387 |  |
|        - |  7388 | `	const char *zData;` |
|      ! 0 |  7389 | `	int nDataLen = 0;` |
|        - |  7390 | `	ph7_vm *pVm;` |
|        - |  7391 | `	int i,rc;` |
|        - |  7392 | `	/* Point to the target VM */` |
|      ! 0 |  7393 | `	pVm = pCtx->pVm;` |
|        - |  7394 | `	/* Output */` |
|      ! 0 |  7395 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7396 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7397 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7398 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7399 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7400 | `				/* Increment output length */` |
|      ! 0 |  7401 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7402 | `			}` |
|      ! 0 |  7403 | `			if( rc == SXERR_ABORT ){` |
|        - |  7404 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7405 | `				return PH7_ABORT;` |
|        - |  7406 | `			}` |
|      ! 0 |  7407 | `		}` |
|      ! 0 |  7408 | `	}` |
|      ! 0 |  7409 | `	return SXRET_OK;` |
|      ! 0 |  7410 |  |
|        - |  7411 | `/*` |
|        - |  7412 | ` * int print($string...)` |
|        - |  7413 | ` *  Output one or more messages.` |
|        - |  7414 | ` * Parameters` |
|        - |  7415 | ` *  $string` |
|        - |  7416 | ` *   Message to output.` |
|        - |  7417 | ` * Return` |
|        - |  7418 | ` *  1 always.` |
|        - |  7419 | ` */` |
|        2 |  7420 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7421 |  |
|        - |  7422 | `	const char *zData;` |
|        3 |  7423 | `	int nDataLen = 0;` |
|        - |  7424 | `	ph7_vm *pVm;` |
|        - |  7425 | `	int i,rc;` |
|        - |  7426 | `	/* Point to the target VM */` |
|        3 |  7427 | `	pVm = pCtx->pVm;` |
|        - |  7428 | `	/* Output */` |
|        5 |  7429 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7430 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7431 | `		if( nDataLen > 0 ){` |
|        3 |  7432 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7433 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7434 | `				/* Increment output length */` |
|        3 |  7435 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7436 | `			}` |
|        3 |  7437 | `			if( rc == SXERR_ABORT ){` |
|        - |  7438 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7439 | `				return PH7_ABORT;` |
|        - |  7440 | `			}` |
|        1 |  7441 | `		}` |
|        2 |  7442 | `	}` |
|        - |  7443 | `	/* Return 1 */` |
|        3 |  7444 | `	ph7_result_int(pCtx,1);` |
|        3 |  7445 | `	return SXRET_OK;` |
|        2 |  7446 |  |
|        - |  7447 | `/*` |
|        - |  7448 | ` * void exit(string $msg)` |
|        - |  7449 | ` * void exit(int $status)` |
|        - |  7450 | ` * void die(string $ms)` |
|        - |  7451 | ` * void die(int $status)` |
|        - |  7452 | ` *   Output a message and terminate program execution.` |
|        - |  7453 | ` * Parameter` |
|        - |  7454 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7455 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7456 | ` *  and not printed` |
|        - |  7457 | ` * Return` |
|        - |  7458 | ` *  NULL` |
|        - |  7459 | ` */` |
|      ! 0 |  7460 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7461 |  |
|      ! 0 |  7462 | `	if( nArg > 0 ){` |
|      ! 0 |  7463 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7464 | `			const char *zData;` |
|      ! 0 |  7465 | `			int iLen = 0;` |
|        - |  7466 | `			/* Print exit message */` |
|      ! 0 |  7467 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7468 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7469 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7470 | `			sxi32 iExitStatus;` |
|        - |  7471 | `			/* Record exit status code */` |
|      ! 0 |  7472 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7473 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7474 | `		}` |
|      ! 0 |  7475 | `	}` |
|        - |  7476 | `	/* Check if we are in an included file */` |
|      ! 0 |  7477 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7478 | `		/* Exit the entire process */` |
|      ! 0 |  7479 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7480 | `	}` |
|        - |  7481 | `	/* Abort processing immediately */` |
|      ! 0 |  7482 | `	return PH7_ABORT;` |
|      ! 0 |  7483 |  |
|        - |  7484 | `/*` |
|        - |  7485 | ` * bool isset($var,...)` |
|        - |  7486 | ` *  Finds out whether a variable is set.` |
|        - |  7487 | ` * Parameters` |
|        - |  7488 | ` *  One or more variable to check.` |
|        - |  7489 | ` * Return` |
|        - |  7490 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7491 | ` */` |
|    65266 |  7492 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7493 |  |
|        - |  7494 | `	ph7_value *pObj;` |
|    65268 |  7495 | `	int res = 0;` |
|        - |  7496 | `	int i;` |
|    65268 |  7497 | `	if( nArg < 1 ){` |
|        - |  7498 | `		/* Missing arguments,return false */` |
|      ! 0 |  7499 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7500 | `		return SXRET_OK;` |
|        - |  7501 | `	}` |
|        - |  7502 | `	/* Iterate over available arguments */` |
|    86580 |  7503 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    65268 |  7504 | `		pObj = apArg[i];` |
|    65268 |  7505 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    43576 |  7506 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7507 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7508 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7509 | `			}` |
|    21787 |  7510 | `		}` |
|    65268 |  7511 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    65268 |  7512 | `		if( !res ){` |
|        - |  7513 | `			/* Variable not set,return FALSE */` |
|    43956 |  7514 | `			ph7_result_bool(pCtx,0);` |
|    43956 |  7515 | `			return SXRET_OK;` |
|        - |  7516 | `		}` |
|    10658 |  7517 | `	}` |
|        - |  7518 | `	/* All given variable are set,return TRUE */` |
|    21314 |  7519 | `	ph7_result_bool(pCtx,1);` |
|    21314 |  7520 | `	return SXRET_OK;` |
|    32635 |  7521 |  |
|        - |  7522 | `/*` |
|        - |  7523 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7524 | ` * frame,the reference table and discard it's contents.` |
|        - |  7525 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7526 | ` */` |
|  2914064 |  7527 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7528 |  |
|        - |  7529 | `	ph7_value *pObj;` |
|        - |  7530 | `	VmRefObj *pRef;` |
|  2914066 |  7531 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2914066 |  7532 | `	if( pObj ){` |
|        - |  7533 | `		/* Release the object */` |
|  2914066 |  7534 | `		PH7_MemObjRelease(pObj);` |
|  1457032 |  7535 | `	}` |
|        - |  7536 | `	/* Remove old reference links */` |
|  2914066 |  7537 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2914066 |  7538 | `	if( pRef ){` |
|  2914046 |  7539 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7540 | `		/* Unlink from the reference table */` |
|  2914046 |  7541 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2914046 |  7542 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7543 | `			VmSlot sFree;` |
|        - |  7544 | `			/* Restore to the free list */` |
|  2914040 |  7545 | `			sFree.nIdx = nObjIdx;` |
|  2914040 |  7546 | `			sFree.pUserData = 0;` |
|  2914040 |  7547 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1457019 |  7548 | `		}` |
|  1457022 |  7549 | `	}` |
|  2914066 |  7550 | `	return SXRET_OK;` |
|        2 |  7551 |  |
|        - |  7552 | `/*` |
|        - |  7553 | ` * void unset($var,...)` |
|        - |  7554 | ` *   Unset one or more given variable.` |
|        - |  7555 | ` * Parameters` |
|        - |  7556 | ` *  One or more variable to unset.` |
|        - |  7557 | ` * Return` |
|        - |  7558 | ` *  Nothing.` |
|        - |  7559 | ` */` |
|     3164 |  7560 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7561 |  |
|        - |  7562 | `	ph7_value *pObj;` |
|        - |  7563 | `	ph7_vm *pVm;` |
|        - |  7564 | `	int i;` |
|        - |  7565 | `	/* Point to the target VM */` |
|     3166 |  7566 | `	pVm = pCtx->pVm;` |
|        - |  7567 | `	/* Iterate and unset */` |
|     9472 |  7568 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6308 |  7569 | `		pObj = apArg[i];` |
|     6308 |  7570 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      812 |  7571 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7572 | `				/* Throw an error */` |
|      ! 0 |  7573 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7574 | `			}` |
|      407 |  7575 | `		}else{` |
|     5497 |  7576 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7577 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5497 |  7578 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5491 |  7579 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2745 |  7580 | `			}` |
|        - |  7581 | `		}` |
|     3155 |  7582 | `	}` |
|     3166 |  7583 | `	return SXRET_OK;` |
|        2 |  7584 |  |
|        - |  7585 | `/*` |
|        - |  7586 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7587 | ` */` |
|      110 |  7588 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7589 |  |
|      111 |  7590 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7591 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7592 | `	ph7_value *pObj;` |
|        - |  7593 | `	sxu32 nIdx;` |
|        - |  7594 | `	/* Extract the memory object */` |
|      111 |  7595 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7596 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7597 | `	if( pObj ){` |
|      111 |  7598 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7599 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7600 | `				SyString sName;` |
|        - |  7601 | `				ph7_value sKey;` |
|        - |  7602 | `				/* Perform the insertion */` |
|      109 |  7603 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7604 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7605 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7606 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7607 | `			}` |
|       54 |  7608 | `		}` |
|       55 |  7609 | `	}` |
|      111 |  7610 | `	return SXRET_OK;` |
|        1 |  7611 |  |
|        - |  7612 | `/*` |
|        - |  7613 | ` * array get_defined_vars(void)` |
|        - |  7614 | ` *  Returns an array of all defined variables.` |
|        - |  7615 | ` * Parameter` |
|        - |  7616 | ` *  None` |
|        - |  7617 | ` * Return` |
|        - |  7618 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7619 | ` */` |
|        2 |  7620 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7621 |  |
|        3 |  7622 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7623 | `	ph7_value *pArray;` |
|        - |  7624 | `	/* Create a new array */` |
|        3 |  7625 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7626 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7627 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7628 | `		SXUNUSED(apArg);` |
|        - |  7629 | `		/* Return NULL */` |
|      ! 0 |  7630 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7631 | `		return SXRET_OK;` |
|        - |  7632 | `	}` |
|        - |  7633 | `	/* Superglobals first */` |
|        3 |  7634 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7635 | `	/* Then variable defined in the current frame */` |
|        3 |  7636 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7637 | `	/* Finally,return the created array */` |
|        3 |  7638 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7639 | `	return SXRET_OK;` |
|        2 |  7640 |  |
|        - |  7641 | `/*` |
|        - |  7642 | ` * bool gettype($var)` |
|        - |  7643 | ` *  Get the type of a variable` |
|        - |  7644 | ` * Parameters` |
|        - |  7645 | ` *   $var` |
|        - |  7646 | ` *    The variable being type checked.` |
|        - |  7647 | ` * Return` |
|        - |  7648 | ` *   String representation of the given variable type.` |
|        - |  7649 | ` */` |
|       30 |  7650 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7651 |  |
|       32 |  7652 | `	const char *zType = "Empty";` |
|       32 |  7653 | `	if( nArg > 0 ){` |
|       32 |  7654 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       15 |  7655 | `	}` |
|        - |  7656 | `	/* Return the variable type */` |
|       32 |  7657 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       32 |  7658 | `	return SXRET_OK;` |
|        2 |  7659 |  |
|        - |  7660 | `/*` |
|        - |  7661 | ` * string get_resource_type(resource $handle)` |
|        - |  7662 | ` *  This function gets the type of the given resource.` |
|        - |  7663 | ` * Parameters` |
|        - |  7664 | ` *  $handle` |
|        - |  7665 | ` *  The evaluated resource handle.` |
|        - |  7666 | ` * Return` |
|        - |  7667 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7668 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7669 | ` *  the return value will be the string Unknown.` |
|        - |  7670 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7671 | ` *  is not a resource.` |
|        - |  7672 | ` */` |
|        2 |  7673 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7674 |  |
|        3 |  7675 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7676 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7677 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7678 | `		return PH7_OK;` |
|        - |  7679 | `	}` |
|        3 |  7680 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7681 | `	return SXRET_OK;` |
|        2 |  7682 |  |
|        - |  7683 | `/*` |
|        - |  7684 | ` * void var_dump(expression,....)` |
|        - |  7685 | ` *   var_dump � Dumps information about a variable` |
|        - |  7686 | ` * Parameters` |
|        - |  7687 | ` *   One or more expression to dump.` |
|        - |  7688 | ` * Returns` |
|        - |  7689 | ` *  Nothing.` |
|        - |  7690 | ` */` |
|      220 |  7691 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7692 |  |
|        - |  7693 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7694 | `	int i;` |
|      222 |  7695 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7696 | `	/* Dump one or more expressions */` |
|      448 |  7697 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      228 |  7698 | `		ph7_value *pObj = apArg[i];` |
|        - |  7699 | `		/* Reset the working buffer */` |
|      228 |  7700 | `		SyBlobReset(&sDump);` |
|        - |  7701 | `		/* Dump the given expression */` |
|      228 |  7702 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7703 | `		/* Output */` |
|      228 |  7704 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      228 |  7705 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      113 |  7706 | `		}` |
|      115 |  7707 | `	}` |
|        - |  7708 | `	/* Release the working buffer */` |
|      222 |  7709 | `	SyBlobRelease(&sDump);` |
|      222 |  7710 | `	return SXRET_OK;` |
|        2 |  7711 |  |
|        - |  7712 | `/*` |
|        - |  7713 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7714 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7715 | ` * Parameters` |
|        - |  7716 | ` *   expression: Expression to dump` |
|        - |  7717 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7718 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7719 | ` *            print_r() will return the information rather than print it.` |
|        - |  7720 | ` * Return` |
|        - |  7721 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7722 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7723 | ` */` |
|       16 |  7724 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7725 |  |
|       17 |  7726 | `	int ret_string = 0;` |
|        - |  7727 | `	SyBlob sDump;` |
|       17 |  7728 | `	if( nArg < 1 ){` |
|        - |  7729 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7730 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7731 | `		return SXRET_OK;` |
|        - |  7732 | `	}` |
|       17 |  7733 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7734 | `	if ( nArg > 1 ){` |
|        - |  7735 | `		/* Where to redirect output */` |
|       11 |  7736 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7737 | `	}` |
|        - |  7738 | `	/* Generate dump */` |
|       17 |  7739 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7740 | `	if( !ret_string ){` |
|        - |  7741 | `		/* Output dump */` |
|        7 |  7742 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7743 | `		/* Return true */` |
|        7 |  7744 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7745 | `	}else{` |
|        - |  7746 | `		/* Generated dump as return value */` |
|       11 |  7747 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7748 | `	}` |
|        - |  7749 | `	/* Release the working buffer */` |
|       17 |  7750 | `	SyBlobRelease(&sDump);` |
|       17 |  7751 | `	return SXRET_OK;` |
|        9 |  7752 |  |
|        - |  7753 | `/*` |
|        - |  7754 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7755 | ` * Same job as print_r. (see coment above)` |
|        - |  7756 | ` */` |
|        2 |  7757 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7758 |  |
|        3 |  7759 | `	int ret_string = 0;` |
|        - |  7760 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7761 | `	if( nArg < 1 ){` |
|        - |  7762 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7763 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7764 | `		return SXRET_OK;` |
|        - |  7765 | `	}` |
|        3 |  7766 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7767 | `	if ( nArg > 1 ){` |
|        - |  7768 | `		/* Where to redirect output */` |
|        3 |  7769 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7770 | `	}` |
|        - |  7771 | `	/* Generate dump */` |
|        3 |  7772 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7773 | `	if( !ret_string ){` |
|        - |  7774 | `		/* Output dump */` |
|      ! 0 |  7775 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7776 | `		/* Return NULL */` |
|      ! 0 |  7777 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7778 | `	}else{` |
|        - |  7779 | `		/* Generated dump as return value */` |
|        3 |  7780 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7781 | `	}` |
|        - |  7782 | `	/* Release the working buffer */` |
|        3 |  7783 | `	SyBlobRelease(&sDump);` |
|        3 |  7784 | `	return SXRET_OK;` |
|        2 |  7785 |  |
|        - |  7786 | `/*` |
|        - |  7787 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7788 | ` *  Set/get the various assert flags.` |
|        - |  7789 | ` * Parameter` |
|        - |  7790 | ` * $what` |
|        - |  7791 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7792 | ` *   ASSERT_WARNING         Issue a warning for each failed assertion` |
|        - |  7793 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7794 | ` *   ASSERT_QUIET_EVAL      Not used` |
|        - |  7795 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7796 | ` * $value` |
|        - |  7797 | ` *   An optional new value for the option.` |
|        - |  7798 | ` * Return` |
|        - |  7799 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7800 | ` */` |
|        8 |  7801 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7802 |  |
|        9 |  7803 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7804 | `	int iOld,iNew,iValue;` |
|        9 |  7805 | `	if( nArg < 1 \|\| !ph7_value_is_int(apArg[0]) ){` |
|        - |  7806 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  7807 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7808 | `		return PH7_OK;` |
|        - |  7809 | `	}` |
|        - |  7810 | `	/* Save old assertion flags */` |
|        9 |  7811 | `	iOld = pVm->iAssertFlags;` |
|        - |  7812 | `	/* Extract the new flags */` |
|        9 |  7813 | `	iNew = ph7_value_to_int(apArg[0]);` |
|        9 |  7814 | `	if( iNew == PH7_ASSERT_DISABLE ){` |
|        7 |  7815 | `		pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        7 |  7816 | `		if( nArg > 1 ){` |
|        5 |  7817 | `			iValue = !ph7_value_to_bool(apArg[1]);` |
|        5 |  7818 | `			if( iValue ){` |
|        - |  7819 | `				/* Disable assertion */` |
|        3 |  7820 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        1 |  7821 | `			}` |
|        3 |  7822 | `		}` |
|        6 |  7823 | `	}else if( iNew == PH7_ASSERT_WARNING ){` |
|      ! 0 |  7824 | `		pVm->iAssertFlags &= ~PH7_ASSERT_WARNING;` |
|      ! 0 |  7825 | `		if( nArg > 1 ){` |
|      ! 0 |  7826 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7827 | `			if( iValue ){` |
|        - |  7828 | `				/* Issue a warning for each failed assertion */` |
|      ! 0 |  7829 | `				pVm->iAssertFlags \|= PH7_ASSERT_WARNING;` |
|      ! 0 |  7830 | `			}` |
|      ! 0 |  7831 | `		}` |
|        3 |  7832 | `	}else if( iNew == PH7_ASSERT_BAIL ){` |
|        3 |  7833 | `		pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        3 |  7834 | `		if( nArg > 1 ){` |
|        3 |  7835 | `			iValue = ph7_value_to_bool(apArg[1]);` |
|        3 |  7836 | `			if( iValue ){` |
|        - |  7837 | `				/* Terminate execution on failed assertions */` |
|        3 |  7838 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        1 |  7839 | `			}` |
|        2 |  7840 | `		}` |
|        1 |  7841 | `	}else if( iNew == PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  7842 | `		pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7843 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|        - |  7844 | `			/* Callback to call on failed assertions */` |
|      ! 0 |  7845 | `			PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  7846 | `			pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7847 | `		}` |
|      ! 0 |  7848 | `	}` |
|        - |  7849 | `	/* Return the old flags */` |
|        9 |  7850 | `	ph7_result_int(pCtx,iOld);` |
|        9 |  7851 | `	return PH7_OK;` |
|        5 |  7852 |  |
|        - |  7853 | `/*` |
|        - |  7854 | ` * bool assert(mixed $assertion)` |
|        - |  7855 | ` *  Checks if assertion is FALSE.` |
|        - |  7856 | ` * Parameter` |
|        - |  7857 | ` *  $assertion` |
|        - |  7858 | ` *    The assertion to test.` |
|        - |  7859 | ` * Return` |
|        - |  7860 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  7861 | ` */` |
|       14 |  7862 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7863 |  |
|       15 |  7864 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7865 | `	ph7_value *pAssert;` |
|        - |  7866 | `	int iFlags,iResult;` |
|       15 |  7867 | `	if( nArg < 1 ){` |
|        - |  7868 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7869 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7870 | `		return PH7_OK;` |
|        - |  7871 | `	}` |
|       15 |  7872 | `	iFlags = pVm->iAssertFlags;` |
|       15 |  7873 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  7874 | `		/* Assertion is disabled,return FALSE */` |
|      ! 0 |  7875 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7876 | `		return PH7_OK;` |
|        - |  7877 | `	}` |
|       15 |  7878 | `	pAssert = apArg[0];` |
|       15 |  7879 | `	iResult = 1; /* cc warning */` |
|       15 |  7880 | `	if( pAssert->iFlags & MEMOBJ_STRING ){` |
|        - |  7881 | `		SyString sChunk;` |
|        7 |  7882 | `		SyStringInitFromBuf(&sChunk,SyBlobData(&pAssert->sBlob),SyBlobLength(&pAssert->sBlob));` |
|        7 |  7883 | `		if( sChunk.nByte > 0 ){` |
|        5 |  7884 | `			VmEvalChunk(pVm,pCtx,&sChunk,PH7_PHP_ONLY\|PH7_PHP_EXPR,FALSE);` |
|        - |  7885 | `			/* Extract evaluation result */` |
|        5 |  7886 | `			iResult = ph7_value_to_bool(pCtx->pRet);` |
|        3 |  7887 | `		}else{` |
|        3 |  7888 | `			iResult = 0;` |
|        - |  7889 | `		}` |
|        4 |  7890 | `	}else{` |
|        - |  7891 | `		/* Perform a boolean cast */` |
|        9 |  7892 | `		iResult = ph7_value_to_bool(apArg[0]);` |
|        - |  7893 | `	}` |
|       15 |  7894 | `	if( !iResult ){` |
|        - |  7895 | `		/* Assertion failed */` |
|        9 |  7896 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
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
|      ! 0 |  7911 | `			apCbArg[2] = pAssert;` |
|      ! 0 |  7912 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  7913 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  7914 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  7915 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  7916 | `		}` |
|        9 |  7917 | `		if( iFlags & PH7_ASSERT_WARNING ){` |
|        - |  7918 | `			/* Emit a warning */` |
|        9 |  7919 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Assertion failed");` |
|        4 |  7920 | `		}` |
|        9 |  7921 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  7922 | `			/* Abort VM execution immediately */` |
|        3 |  7923 | `			return PH7_ABORT;` |
|        - |  7924 | `		}` |
|        3 |  7925 | `	}` |
|        - |  7926 | `	/* Assertion result */` |
|       13 |  7927 | `	ph7_result_bool(pCtx,iResult);` |
|       13 |  7928 | `	return PH7_OK;` |
|        8 |  7929 |  |
|        - |  7930 | `/*` |
|        - |  7931 | ` * Section:` |
|        - |  7932 | ` *  Error reporting functions.` |
|        - |  7933 | ` * Status:` |
|        - |  7934 | ` *    Stable.` |
|        - |  7935 | ` */` |
|        - |  7936 | `/*` |
|        - |  7937 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  7938 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  7939 | ` * Parameters` |
|        - |  7940 | ` *  $error_msg` |
|        - |  7941 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  7942 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  7943 | ` * $error_type` |
|        - |  7944 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  7945 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  7946 | ` * Return` |
|        - |  7947 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  7948 | ` */` |
|       12 |  7949 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7950 |  |
|       14 |  7951 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  7952 | `	int rc = PH7_OK;` |
|       14 |  7953 | `	if( nArg > 0 ){` |
|        - |  7954 | `		const char *zErr;` |
|        - |  7955 | `		int nLen;` |
|        - |  7956 | `		/* Extract the error message */` |
|       12 |  7957 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7958 | `		if( nArg > 1 ){` |
|        - |  7959 | `			/* Extract the error type */` |
|       12 |  7960 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  7961 | `			switch( nErr ){` |
|        1 |  7962 | `			case 1:   /* E_ERROR */` |
|        - |  7963 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  7964 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  7965 | `			case 256: /* E_USER_ERROR */` |
|        3 |  7966 | `				nErr = PH7_CTX_ERR;` |
|        3 |  7967 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  7968 | `				break;` |
|        1 |  7969 | `			case 2:   /* E_WARNING */` |
|        - |  7970 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  7971 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  7972 | `			case 512: /* E_USER_WARNING */` |
|        3 |  7973 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  7974 | `				break;` |
|        3 |  7975 | `			default:` |
|        8 |  7976 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  7977 | `				break;` |
|        - |  7978 | `			}` |
|        5 |  7979 | `		}` |
|        - |  7980 | `		/* Report error */` |
|       12 |  7981 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  7982 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  7983 | `			return rc;` |
|        - |  7984 | `		}` |
|        - |  7985 | `		/* Return true */` |
|       12 |  7986 | `		ph7_result_bool(pCtx,1);` |
|        7 |  7987 | `	}else{` |
|        - |  7988 | `		/* Missing arguments,return FALSE */` |
|        3 |  7989 | `		ph7_result_bool(pCtx,0);` |
|        - |  7990 | `	}` |
|       14 |  7991 | `	return rc;` |
|        8 |  7992 |  |
|        - |  7993 | `/*` |
|        - |  7994 | ` * int error_reporting([int $level])` |
|        - |  7995 | ` *  Sets which PHP errors are reported.` |
|        - |  7996 | ` * Parameters` |
|        - |  7997 | ` *  $level` |
|        - |  7998 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  7999 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8000 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8001 | ` *   levels will not always behave as expected.` |
|        - |  8002 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8003 | ` *   in the predefined constants.` |
|        - |  8004 | ` * Return` |
|        - |  8005 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8006 | ` *   parameter is given.` |
|        - |  8007 | ` */` |
|       18 |  8008 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8009 |  |
|       19 |  8010 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8011 | `	int nOld;` |
|        - |  8012 | `	/* Extract the old reporting level */` |
|       19 |  8013 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       19 |  8014 | `	if( nArg > 0 ){` |
|        - |  8015 | `		int nNew;` |
|        - |  8016 | `		/* Extract the desired error reporting level */` |
|       11 |  8017 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       11 |  8018 | `		if( !nNew ){` |
|        - |  8019 | `			/* Do not report errors at all */` |
|        5 |  8020 | `			pVm->bErrReport = 0;` |
|        3 |  8021 | `		}else{` |
|        - |  8022 | `			/* Report all errors */` |
|        7 |  8023 | `			pVm->bErrReport = 1;` |
|        - |  8024 | `		}` |
|        5 |  8025 | `	}` |
|        - |  8026 | `	/* Return the old level */` |
|       19 |  8027 | `	ph7_result_int(pCtx,nOld);` |
|       19 |  8028 | `	return PH7_OK;` |
|        1 |  8029 |  |
|        - |  8030 | `/*` |
|        - |  8031 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8032 | ` *  Send an error message somewhere.` |
|        - |  8033 | ` * Parameter` |
|        - |  8034 | ` *  $message` |
|        - |  8035 | ` *   The error message that should be logged.` |
|        - |  8036 | ` *  $message_type` |
|        - |  8037 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8038 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8039 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8040 | ` *       This is the default option.` |
|        - |  8041 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8042 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8043 | ` *    2  No longer an option.` |
|        - |  8044 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8045 | ` *       to the end of the message string.` |
|        - |  8046 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8047 | ` *  $destination` |
|        - |  8048 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8049 | ` *  $extra_headers` |
|        - |  8050 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8051 | ` * Return` |
|        - |  8052 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8053 | ` * NOTE:` |
|        - |  8054 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8055 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8056 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8057 | ` *  Otherwise this function is no-op.` |
|        - |  8058 | ` */` |
|        4 |  8059 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8060 |  |
|        - |  8061 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8062 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8063 | `	int iType = 0;` |
|        5 |  8064 | `	if( nArg < 1 ){` |
|        - |  8065 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8066 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8067 | `		return PH7_OK;` |
|        - |  8068 | `	}` |
|        5 |  8069 | `	if( pVm->xErrLog  ){` |
|        - |  8070 | `		/* Invoke the user callback */` |
|      ! 0 |  8071 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8072 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8073 | `		if( nArg > 1 ){` |
|      ! 0 |  8074 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8075 | `			if( nArg > 2 ){` |
|      ! 0 |  8076 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8077 | `				if( nArg > 3 ){` |
|      ! 0 |  8078 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8079 | `				}` |
|      ! 0 |  8080 | `			}` |
|      ! 0 |  8081 | `		}` |
|      ! 0 |  8082 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8083 | `	}` |
|        - |  8084 | `	/* Retun TRUE */` |
|        5 |  8085 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8086 | `	return PH7_OK;` |
|        3 |  8087 |  |
|        - |  8088 | `/*` |
|        - |  8089 | ` * bool restore_exception_handler(void)` |
|        - |  8090 | ` *  Restores the previously defined exception handler function.` |
|        - |  8091 | ` * Parameter` |
|        - |  8092 | ` *  None` |
|        - |  8093 | ` * Return` |
|        - |  8094 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8095 | ` */` |
|        4 |  8096 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8097 |  |
|        5 |  8098 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8099 | `	ph7_value *pOld,*pNew;` |
|        - |  8100 | `	/* Point to the old and the new handler */` |
|        5 |  8101 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8102 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8103 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8104 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8105 | `		SXUNUSED(apArg);` |
|        - |  8106 | `		/* No installed handler,return FALSE */` |
|        5 |  8107 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8108 | `		return PH7_OK;` |
|        - |  8109 | `	}` |
|        - |  8110 | `	/* Copy the old handler */` |
|      ! 0 |  8111 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8112 | `	PH7_MemObjRelease(pOld);` |
|        - |  8113 | `	/* Return TRUE */` |
|      ! 0 |  8114 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8115 | `	return PH7_OK;` |
|        3 |  8116 |  |
|        - |  8117 | `/*` |
|        - |  8118 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8119 | ` *  Sets a user-defined exception handler function.` |
|        - |  8120 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8121 | ` * NOTE` |
|        - |  8122 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8123 | ` *  the satndard PHP engine.` |
|        - |  8124 | ` * Parameters` |
|        - |  8125 | ` *  $exception_handler` |
|        - |  8126 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8127 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8128 | ` *   that was thrown.` |
|        - |  8129 | ` *  Note:` |
|        - |  8130 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8131 | ` * Return` |
|        - |  8132 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8133 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8134 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8135 | ` */` |
|        4 |  8136 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8137 |  |
|        6 |  8138 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8139 | `	ph7_value *pOld,*pNew;` |
|        - |  8140 | `	/* Point to the old and the new handler */` |
|        6 |  8141 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8142 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8143 | `	/* Return the old handler */` |
|        6 |  8144 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8145 | `	if( nArg > 0 ){` |
|        6 |  8146 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8147 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8148 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8149 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8150 | `		}else{` |
|        6 |  8151 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8152 | `			/* Install the new handler */` |
|        6 |  8153 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8154 | `		}` |
|        2 |  8155 | `	}` |
|        6 |  8156 | `	return PH7_OK;` |
|        2 |  8157 |  |
|        - |  8158 | `/*` |
|        - |  8159 | ` * bool restore_error_handler(void)` |
|        - |  8160 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8161 | ` * Parameters:` |
|        - |  8162 | ` *  None.` |
|        - |  8163 | ` * Return` |
|        - |  8164 | ` *  Always TRUE.` |
|        - |  8165 | ` */` |
|        4 |  8166 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8167 |  |
|        5 |  8168 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8169 | `	ph7_value *pOld,*pNew;` |
|        - |  8170 | `	/* Point to the old and the new handler */` |
|        5 |  8171 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8172 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8173 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8174 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8175 | `		SXUNUSED(apArg);` |
|        - |  8176 | `		/* No installed callback,return FALSE */` |
|        5 |  8177 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8178 | `		return PH7_OK;` |
|        - |  8179 | `	}` |
|        - |  8180 | `	/* Copy the old callback */` |
|      ! 0 |  8181 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8182 | `	PH7_MemObjRelease(pOld);` |
|        - |  8183 | `	/* Return TRUE */` |
|      ! 0 |  8184 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8185 | `	return PH7_OK;` |
|        3 |  8186 |  |
|        - |  8187 | `/*` |
|        - |  8188 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8189 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8190 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8191 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8192 | ` *  Sets a user-defined error handler function.` |
|        - |  8193 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8194 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8195 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8196 | ` *  conditions (using trigger_error()).` |
|        - |  8197 | ` * Parameters` |
|        - |  8198 | ` *  $error_handler` |
|        - |  8199 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8200 | ` *   describing the error.` |
|        - |  8201 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8202 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8203 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8204 | ` *   The function can be shown as:` |
|        - |  8205 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8206 | ` *     errno` |
|        - |  8207 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8208 | ` *   errstr` |
|        - |  8209 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8210 | ` *   errfile` |
|        - |  8211 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8212 | ` *     was raised in, as a string.` |
|        - |  8213 | ` *  Note:` |
|        - |  8214 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8215 | ` * Return` |
|        - |  8216 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8217 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8218 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8219 | ` */` |
|     8670 |  8220 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8221 |  |
|     8672 |  8222 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8223 | `	ph7_value *pOld,*pNew;` |
|        - |  8224 | `	/* Point to the old and the new handler */` |
|     8672 |  8225 | `	pOld = &pVm->aErrCB[0];` |
|     8672 |  8226 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8227 | `	/* Return the old handler */` |
|     8672 |  8228 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8672 |  8229 | `	if( nArg > 0 ){` |
|     8672 |  8230 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8231 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4335 |  8232 | `			PH7_MemObjRelease(pNew);` |
|     4335 |  8233 | `			ph7_result_bool(pCtx,1);` |
|     2168 |  8234 | `		}else{` |
|     4338 |  8235 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8236 | `			/* Install the new handler */` |
|     4338 |  8237 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8238 | `		}` |
|     4335 |  8239 | `	}` |
|     8672 |  8240 | `	return PH7_OK;` |
|        2 |  8241 |  |
|        - |  8242 | `/*` |
|        - |  8243 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8244 | ` *  Generates a backtrace.` |
|        - |  8245 | ` * Paramaeter` |
|        - |  8246 | ` *  $options` |
|        - |  8247 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8248 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8249 | ` *   all the function/method arguments, to save memory.` |
|        - |  8250 | ` * $limit` |
|        - |  8251 | ` *   (Not Used)` |
|        - |  8252 | ` * Return` |
|        - |  8253 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8254 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8255 | ` *          Name        Type      Description` |
|        - |  8256 | ` *          ------      ------     -----------` |
|        - |  8257 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8258 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8259 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8260 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8261 | ` *          object      object    The current object.` |
|        - |  8262 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8263 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8264 | ` */` |
|      376 |  8265 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8266 |  |
|      378 |  8267 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8268 | `	ph7_value *pArray;` |
|        - |  8269 | `	ph7_class *pClass;` |
|        - |  8270 | `	ph7_value *pValue;` |
|        - |  8271 | `	SyString *pFile;` |
|        - |  8272 | `	/* Create a new array */` |
|      378 |  8273 | `	pArray = ph7_context_new_array(pCtx);` |
|      378 |  8274 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      378 |  8275 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8276 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8277 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8278 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8279 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8280 | `		SXUNUSED(apArg);` |
|      ! 0 |  8281 | `		return PH7_OK;` |
|        - |  8282 | `	}` |
|        - |  8283 | `	/* Dump running function name and it's arguments  */` |
|      378 |  8284 | `	if( pVm->pFrame->pParent ){` |
|      378 |  8285 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8286 | `		ph7_vm_func *pFunc;` |
|        - |  8287 | `		ph7_value *pArg;` |
|      378 |  8288 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8289 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8290 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8291 | `		}` |
|      378 |  8292 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      378 |  8293 | `		if( pFrame->pParent && pFunc ){` |
|      378 |  8294 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      378 |  8295 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      378 |  8296 | `			ph7_value_reset_string_cursor(pValue);` |
|      188 |  8297 | `		}` |
|        - |  8298 | `		/* Function arguments */` |
|      378 |  8299 | `		pArg = ph7_context_new_array(pCtx);` |
|      378 |  8300 | `		if( pArg  ){` |
|        - |  8301 | `			ph7_value *pObj;` |
|        - |  8302 | `			VmSlot *aSlot;` |
|        - |  8303 | `			sxu32 n;` |
|        - |  8304 | `			/* Start filling the array with the given arguments */` |
|      378 |  8305 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1498 |  8306 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1122 |  8307 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1122 |  8308 | `				if( pObj ){` |
|     1122 |  8309 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      560 |  8310 | `				}` |
|      562 |  8311 | `			}` |
|        - |  8312 | `			/* Save the array */` |
|      378 |  8313 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      188 |  8314 | `		}` |
|      188 |  8315 | `	}` |
|      378 |  8316 | `	ph7_value_int(pValue,1);` |
|        - |  8317 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8318 | `	 * line numbers at run-time. )` |
|        - |  8319 | `	 */` |
|      378 |  8320 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8321 | `	/* Current processed script */` |
|      378 |  8322 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      378 |  8323 | `	if( pFile ){` |
|      378 |  8324 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      378 |  8325 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      378 |  8326 | `		ph7_value_reset_string_cursor(pValue);` |
|      188 |  8327 | `	}` |
|        - |  8328 | `	/* Top class */` |
|      378 |  8329 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      378 |  8330 | `	if( pClass ){` |
|      374 |  8331 | `		ph7_value_reset_string_cursor(pValue);` |
|      374 |  8332 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      374 |  8333 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      186 |  8334 | `	}` |
|        - |  8335 | `	/* Return the freshly created array */` |
|      378 |  8336 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8337 | `	/*` |
|        - |  8338 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8339 | `	 * as soon we return from this function.` |
|        - |  8340 | `	 */` |
|      378 |  8341 | `	return PH7_OK;` |
|      190 |  8342 |  |
|        - |  8343 | `/*` |
|        - |  8344 | ` * Generate a small backtrace.` |
|        - |  8345 | ` * Store the generated dump in the given BLOB` |
|        - |  8346 | ` */` |
|        4 |  8347 | `static int VmMiniBacktrace(` |
|        - |  8348 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8349 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8350 | `	)` |
|        1 |  8351 |  |
|        5 |  8352 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8353 | `	ph7_vm_func *pFunc;` |
|        - |  8354 | `	ph7_class *pClass;` |
|        - |  8355 | `	SyString *pFile;` |
|        - |  8356 | `	/* Called function */` |
|        5 |  8357 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8358 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8359 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8360 | `	}` |
|        5 |  8361 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8362 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8363 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8364 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8365 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8366 | `	}else{` |
|      ! 0 |  8367 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8368 | `	}` |
|        5 |  8369 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8370 | `	/* Current processed script */` |
|        5 |  8371 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8372 | `	if( pFile ){` |
|        5 |  8373 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8374 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8375 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8376 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8377 | `	}` |
|        - |  8378 | `	/* Top class */` |
|        5 |  8379 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8380 | `	if( pClass ){` |
|      ! 0 |  8381 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8382 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8383 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8384 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8385 | `	}` |
|        5 |  8386 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8387 | `	/* All done */` |
|        5 |  8388 | `	return SXRET_OK;` |
|        1 |  8389 |  |
|        - |  8390 | `/*` |
|        - |  8391 | ` * void debug_print_backtrace()` |
|        - |  8392 | ` *  Prints a backtrace` |
|        - |  8393 | ` * Parameters` |
|        - |  8394 | ` * None` |
|        - |  8395 | ` * Return` |
|        - |  8396 | ` * NULL` |
|        - |  8397 | ` */` |
|        2 |  8398 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8399 |  |
|        3 |  8400 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8401 | `	SyBlob sDump;` |
|        3 |  8402 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8403 | `	/* Generate the backtrace */` |
|        3 |  8404 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8405 | `	/* Output backtrace */` |
|        3 |  8406 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8407 | `	/* All done,cleanup */` |
|        3 |  8408 | `	SyBlobRelease(&sDump);` |
|        1 |  8409 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8410 | `	SXUNUSED(apArg);` |
|        3 |  8411 | `	return PH7_OK;` |
|        1 |  8412 |  |
|        - |  8413 | `/*` |
|        - |  8414 | ` * string debug_string_backtrace()` |
|        - |  8415 | ` *  Generate a backtrace` |
|        - |  8416 | ` * Parameters` |
|        - |  8417 | ` * None` |
|        - |  8418 | ` * Return` |
|        - |  8419 | ` *  A mini backtrace().` |
|        - |  8420 | ` * Note that this is a symisc extension.` |
|        - |  8421 | ` */` |
|        2 |  8422 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8423 |  |
|        3 |  8424 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8425 | `	SyBlob sDump;` |
|        3 |  8426 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8427 | `	/* Generate the backtrace */` |
|        3 |  8428 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8429 | `	/* Return the backtrace */` |
|        3 |  8430 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8431 | `	/* All done,cleanup */` |
|        3 |  8432 | `	SyBlobRelease(&sDump);` |
|        1 |  8433 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8434 | `	SXUNUSED(apArg);` |
|        3 |  8435 | `	return PH7_OK;` |
|        1 |  8436 |  |
|        - |  8437 | `/*` |
|        - |  8438 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8439 | ` * exception is triggered.` |
|        - |  8440 | ` */` |
|      360 |  8441 | `static sxi32 VmUncaughtException(` |
|        - |  8442 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8443 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8444 | `	)` |
|        1 |  8445 |  |
|        - |  8446 | `	ph7_value *apArg[2],sArg;` |
|      361 |  8447 | `	int nArg = 1;` |
|        - |  8448 | `	sxi32 rc;` |
|      361 |  8449 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8450 | `		/* Nesting limit reached */` |
|      ! 0 |  8451 | `		return SXRET_OK;` |
|        - |  8452 | `	}` |
|        - |  8453 | `	/* Call any exception handler if available */` |
|      361 |  8454 | `	PH7_MemObjInit(pVm,&sArg);` |
|      361 |  8455 | `	if( pThis ){` |
|        - |  8456 | `		/* Load the exception instance */` |
|      361 |  8457 | `		sArg.x.pOther = pThis;` |
|      361 |  8458 | `		pThis->iRef++;` |
|      361 |  8459 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      181 |  8460 | `	}else{` |
|      ! 0 |  8461 | `		nArg = 0;` |
|        - |  8462 | `	}` |
|      361 |  8463 | `	apArg[0] = &sArg;` |
|        - |  8464 | `	/* Call the exception handler if available */` |
|      361 |  8465 | `	pVm->nExceptDepth++;` |
|      361 |  8466 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      361 |  8467 | `	pVm->nExceptDepth--;` |
|      361 |  8468 | `	if( rc != SXRET_OK ){` |
|        - |  8469 | `		SyBlob sMsgBuf;` |
|      359 |  8470 | `		const char *zClass = "Exception";` |
|      359 |  8471 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8472 | `		const char *zMsg;` |
|        - |  8473 | `		sxu32 nMsg;` |
|        - |  8474 | `		const char *zFuncName;` |
|        - |  8475 | `		int nFuncLen;` |
|      359 |  8476 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      359 |  8477 | `		if( pThis ){` |
|        - |  8478 | `			ph7_class_method *pGetMessage;` |
|        - |  8479 | `			ph7_value sMsg;` |
|        - |  8480 | `			const char *zTmp;` |
|        - |  8481 | `			int nTmp;` |
|      359 |  8482 | `			zClass = pThis->pClass->sName.zString;` |
|      359 |  8483 | `			nClass = pThis->pClass->sName.nByte;` |
|      359 |  8484 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      359 |  8485 | `			if( pGetMessage ){` |
|      359 |  8486 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      359 |  8487 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      359 |  8488 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      359 |  8489 | `					if( zTmp && nTmp > 0 ){` |
|      359 |  8490 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      179 |  8491 | `					}` |
|      179 |  8492 | `				}` |
|      359 |  8493 | `				PH7_MemObjRelease(&sMsg);` |
|      179 |  8494 | `			}` |
|      179 |  8495 | `		}` |
|      359 |  8496 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8497 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8498 | `		}` |
|      359 |  8499 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      359 |  8500 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      359 |  8501 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      359 |  8502 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      359 |  8503 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8504 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      359 |  8505 | `		rc = SXERR_ABORT;` |
|      179 |  8506 | `	}` |
|      361 |  8507 | `	PH7_MemObjRelease(&sArg);` |
|      361 |  8508 | `	return rc;` |
|      181 |  8509 |  |
|        - |  8510 | `/*` |
|        - |  8511 | ` * Throw an user exception.` |
|        - |  8512 | ` */` |
|      374 |  8513 | `static sxi32 VmThrowException(` |
|        - |  8514 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8515 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8516 | `	)` |
|        2 |  8517 |  |
|        - |  8518 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8519 | `	ph7_exception **apException;` |
|        - |  8520 | `	ph7_exception *pException;` |
|        - |  8521 | `	/* Point to the stack of loaded exceptions */` |
|      376 |  8522 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      376 |  8523 | `	pException = 0;` |
|      376 |  8524 | `	pCatch = 0;` |
|      376 |  8525 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8526 | `		ph7_exception_block *aCatch;` |
|        - |  8527 | `		ph7_class *pClass;` |
|        - |  8528 | `		sxu32 j;` |
|        - |  8529 | `		/* Locate the appropriate block to execute */` |
|       16 |  8530 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       16 |  8531 | `		(void)SySetPop(&pVm->aException);` |
|       16 |  8532 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       16 |  8533 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       16 |  8534 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8535 | `			/* Extract the target class */` |
|       16 |  8536 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  8537 | `			if( pClass == 0 ){` |
|        - |  8538 | `				/* No such class */` |
|      ! 0 |  8539 | `				continue;` |
|        - |  8540 | `			}` |
|       16 |  8541 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8542 | `				/* Catch block found,break immeditaley */` |
|       16 |  8543 | `				pCatch = &aCatch[j];` |
|       16 |  8544 | `				break;` |
|        - |  8545 | `			}` |
|      ! 0 |  8546 | `		}` |
|        7 |  8547 | `	}` |
|        - |  8548 | `	/* Execute the cached block if available */` |
|      376 |  8549 | `	if( pCatch == 0 ){` |
|        - |  8550 | `		sxi32 rc;` |
|      361 |  8551 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      361 |  8552 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8553 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8554 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8555 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8556 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8557 | `			}` |
|      ! 0 |  8558 | `			if( pException->pFrame == pFrame ){` |
|        - |  8559 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8560 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8561 | `			}` |
|      ! 0 |  8562 | `		}` |
|      361 |  8563 | `		return rc;` |
|      ! 0 |  8564 | `	}else{` |
|       16 |  8565 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8566 | `		sxi32 rc;` |
|       24 |  8567 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8568 | `			/* Safely ignore the exception frame */` |
|       10 |  8569 | `			pFrame = pFrame->pParent;` |
|        2 |  8570 | `		}` |
|       16 |  8571 | `		if( pException->pFrame == pFrame ){` |
|        - |  8572 | `			/* Tell the upper layer that the exception was caught */` |
|        8 |  8573 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        3 |  8574 | `		}` |
|        - |  8575 | `		/* Create a private frame first */` |
|       16 |  8576 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       16 |  8577 | `		if( rc == SXRET_OK ){` |
|        - |  8578 | `			/* Mark as catch frame */` |
|       16 |  8579 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       16 |  8580 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       16 |  8581 | `			if( pObj ){` |
|        - |  8582 | `				/* Install the exception instance */` |
|       16 |  8583 | `				pThis->iRef++; /* Increment reference count */` |
|       16 |  8584 | `				pObj->x.pOther = pThis;` |
|       16 |  8585 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        7 |  8586 | `			}` |
|        - |  8587 | `			/* Exceute the block */` |
|       16 |  8588 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8589 | `			/* Leave the frame */` |
|       16 |  8590 | `			VmLeaveFrame(&(*pVm));` |
|        7 |  8591 | `		}` |
|        - |  8592 | `	}` |
|        - |  8593 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8594 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8595 | `	 */` |
|       16 |  8596 | `	return SXRET_OK;` |
|      189 |  8597 |  |
|        - |  8598 | `/*` |
|        - |  8599 | ` * Section:` |
|        - |  8600 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8601 | ` * Status:` |
|        - |  8602 | ` *    Stable.` |
|        - |  8603 | ` */` |
|        - |  8604 | `/*` |
|        - |  8605 | ` * string ph7version(void)` |
|        - |  8606 | ` *  Returns the running version of the PH7 version.` |
|        - |  8607 | ` * Parameters` |
|        - |  8608 | ` *  None` |
|        - |  8609 | ` * Return` |
|        - |  8610 | ` * Current PH7 version.` |
|        - |  8611 | ` */` |
|        2 |  8612 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8613 |  |
|        1 |  8614 | `	SXUNUSED(nArg);` |
|        1 |  8615 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8616 | `	/* Current engine version */` |
|        3 |  8617 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8618 | `	return PH7_OK;` |
|        1 |  8619 |  |
|        - |  8620 | `/*` |
|        - |  8621 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8622 | ` */` |
|        - |  8623 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8624 | ` "<html><head>"\` |
|        - |  8625 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8626 | ` "<style type=\"text/css\">"\` |
|        - |  8627 | ` "div {"\` |
|        - |  8628 | `     "border: 1px solid #cccccc;"\` |
|        - |  8629 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8630 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8631 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8632 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8633 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8634 | `     "-o-border-radius: 10px;"\` |
|        - |  8635 | `     "border-radius: 10px;"\` |
|        - |  8636 | `     "padding-left: 2em;"\` |
|        - |  8637 | `     "background-color: white;"\` |
|        - |  8638 | `     "margin-left: auto;"\` |
|        - |  8639 | `     "font-family: verdana;"\` |
|        - |  8640 | `     "padding-right: 2em;"\` |
|        - |  8641 | `     "margin-right: auto;"\` |
|        - |  8642 | `     "}"\` |
|        - |  8643 | `     "body {"\` |
|        - |  8644 | `     "padding: 0.2em;"\` |
|        - |  8645 | `     "font-style: normal;"\` |
|        - |  8646 | `     "font-size: medium;"\` |
|        - |  8647 | `     "background-color: #f2f2f2;"\` |
|        - |  8648 | `     "}"\` |
|        - |  8649 | `     "hr {"\` |
|        - |  8650 | `     "border-style: solid none none;"\` |
|        - |  8651 | `     "border-width: 1px medium medium;"\` |
|        - |  8652 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8653 | `     "height: 1px;"\` |
|        - |  8654 | `     "}"\` |
|        - |  8655 | `     "a {"\` |
|        - |  8656 | `     "color: #3366cc;"\` |
|        - |  8657 | `     "text-decoration: none;"\` |
|        - |  8658 | `     "}"\` |
|        - |  8659 | `     "a:hover {"\` |
|        - |  8660 | `     "color: #999999;"\` |
|        - |  8661 | `     "}"\` |
|        - |  8662 | `     "a:active {"\` |
|        - |  8663 | `     "color: #663399;"\` |
|        - |  8664 | `     "}"\` |
|        - |  8665 | `     "h1 {"\` |
|        - |  8666 | `     "margin: 0;"\` |
|        - |  8667 | `     "padding: 0;"\` |
|        - |  8668 | `     "font-family: Verdana;"\` |
|        - |  8669 | `     "font-weight: bold;"\` |
|        - |  8670 | `     "font-style: normal;"\` |
|        - |  8671 | `     "font-size: medium;"\` |
|        - |  8672 | `     "text-transform: capitalize;"\` |
|        - |  8673 | `     "color: #0a328c;"\` |
|        - |  8674 | `     "}"\` |
|        - |  8675 | `     "p {"\` |
|        - |  8676 | `     "margin: 0 auto;"\` |
|        - |  8677 | `     "font-size: medium;"\` |
|        - |  8678 | `     "font-style: normal;"\` |
|        - |  8679 | `     "font-family: verdana;"\` |
|        - |  8680 | `     "}"\` |
|        - |  8681 | `"</style></head><body>"\` |
|        - |  8682 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8683 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8684 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8685 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8686 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8687 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8688 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8689 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8690 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8691 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8692 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8693 |  |
|        - |  8694 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8695 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8696 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8697 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8698 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8699 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8700 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8701 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8702 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8703 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8704 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8705 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8706 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8707 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8708 |  |
|        - |  8709 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8710 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8711 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8712 | `"&nbsp;*<br>"\` |
|        - |  8713 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8714 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8715 | `"&nbsp;* are met:<br>"\` |
|        - |  8716 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8717 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8718 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8719 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8720 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8721 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8722 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8723 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8724 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8725 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8726 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8727 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8728 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8729 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8730 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8731 | `"&nbsp;*<br>"\` |
|        - |  8732 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8733 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8734 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8735 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8736 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8737 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8738 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8739 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8740 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8741 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8742 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8743 | `"&nbsp;*/<br>"\` |
|        - |  8744 | `"</span></small></small></p>"\` |
|        - |  8745 | `"</div></body></html>"` |
|        - |  8746 | `/*` |
|        - |  8747 | ` * bool ph7credits(void)` |
|        - |  8748 | ` * bool ph7info(void)` |
|        - |  8749 | ` * bool ph7copyright(void)` |
|        - |  8750 | ` *  Prints out the credits for PH7 engine` |
|        - |  8751 | ` * Parameters` |
|        - |  8752 | ` *  None` |
|        - |  8753 | ` * Return` |
|        - |  8754 | ` *  Always TRUE` |
|        - |  8755 | ` */` |
|        2 |  8756 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8757 |  |
|        3 |  8758 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8759 | `	/* Expand the HTML page above*/` |
|        3 |  8760 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8761 | `	ph7_context_output_format(` |
|        1 |  8762 | `		pCtx,` |
|        - |  8763 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8764 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8765 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8766 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8767 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8768 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8769 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8770 | `#ifdef __WINNT__` |
|        - |  8771 | `		"Windows NT"` |
|        - |  8772 | `#elif defined(__UNIXES__)` |
|        - |  8773 | `		"UNIX-Like"` |
|        - |  8774 | `#else` |
|        - |  8775 | `		"Other OS"` |
|        - |  8776 | `#endif` |
|        - |  8777 | `		);` |
|        3 |  8778 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8779 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8780 | `	SXUNUSED(apArg);` |
|        - |  8781 | `	/* Return TRUE */` |
|        - |  8782 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8783 | `	return PH7_OK;` |
|        1 |  8784 |  |
|        - |  8785 | `/*` |
|        - |  8786 | ` * Section:` |
|        - |  8787 | ` *    URL related routines.` |
|        - |  8788 | ` * Status:` |
|        - |  8789 | ` *    Stable.` |
|        - |  8790 | ` */` |
|        - |  8791 | `/*` |
|        - |  8792 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8793 | ` *  Parse a URL and return its fields.` |
|        - |  8794 | ` * Parameters` |
|        - |  8795 | ` *  $url` |
|        - |  8796 | ` *   The URL to parse.` |
|        - |  8797 | ` * $component` |
|        - |  8798 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8799 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8800 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8801 | ` *  in which case the return value will be an integer).` |
|        - |  8802 | ` * Return` |
|        - |  8803 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8804 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8805 | ` *  this array are:` |
|        - |  8806 | ` *   scheme - e.g. http` |
|        - |  8807 | ` *   host` |
|        - |  8808 | ` *   port` |
|        - |  8809 | ` *   user` |
|        - |  8810 | ` *   pass` |
|        - |  8811 | ` *   path` |
|        - |  8812 | ` *   query - after the question mark ?` |
|        - |  8813 | ` *   fragment - after the hashmark #` |
|        - |  8814 | ` * Note:` |
|        - |  8815 | ` *  FALSE is returned on failure.` |
|        - |  8816 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  8817 | ` *  with the standard PHP engine.` |
|        - |  8818 | ` */` |
|       28 |  8819 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8820 |  |
|        - |  8821 | `	const char *zStr; /* Input string */` |
|        - |  8822 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  8823 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  8824 | `	int nLen;` |
|        - |  8825 | `	sxi32 rc;` |
|       29 |  8826 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8827 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8828 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8829 | `		return PH7_OK;` |
|        - |  8830 | `	}` |
|        - |  8831 | `	/* Extract the given URI */` |
|       29 |  8832 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  8833 | `	if( nLen < 1 ){` |
|        - |  8834 | `		/* Nothing to process,return FALSE */` |
|        3 |  8835 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8836 | `		return PH7_OK;` |
|        - |  8837 | `	}` |
|        - |  8838 | `	/* Get a parse */` |
|       27 |  8839 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  8840 | `	if( rc != SXRET_OK ){` |
|        - |  8841 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  8842 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8843 | `		return PH7_OK;` |
|        - |  8844 | `	}` |
|       27 |  8845 | `	if( nArg > 1 ){` |
|      ! 0 |  8846 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  8847 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  8848 | `		switch(nComponent){` |
|      ! 0 |  8849 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  8850 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  8851 | `			if( pComp->nByte < 1 ){` |
|        - |  8852 | `				/* No available value,return NULL */` |
|      ! 0 |  8853 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8854 | `			}else{` |
|      ! 0 |  8855 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8856 | `			}` |
|      ! 0 |  8857 | `			break;` |
|      ! 0 |  8858 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  8859 | `			pComp = &sURI.sHost;` |
|      ! 0 |  8860 | `			if( pComp->nByte < 1 ){` |
|        - |  8861 | `				/* No available value,return NULL */` |
|      ! 0 |  8862 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8863 | `			}else{` |
|      ! 0 |  8864 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8865 | `			}` |
|      ! 0 |  8866 | `			break;` |
|      ! 0 |  8867 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  8868 | `			pComp = &sURI.sPort;` |
|      ! 0 |  8869 | `			if( pComp->nByte < 1 ){` |
|        - |  8870 | `				/* No available value,return NULL */` |
|      ! 0 |  8871 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8872 | `			}else{` |
|      ! 0 |  8873 | `				int iPort = 0;` |
|        - |  8874 | `				/* Cast the value to integer */` |
|      ! 0 |  8875 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  8876 | `				ph7_result_int(pCtx,iPort);` |
|        - |  8877 | `			}` |
|      ! 0 |  8878 | `			break;` |
|      ! 0 |  8879 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  8880 | `			pComp = &sURI.sUser;` |
|      ! 0 |  8881 | `			if( pComp->nByte < 1 ){` |
|        - |  8882 | `				/* No available value,return NULL */` |
|      ! 0 |  8883 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8884 | `			}else{` |
|      ! 0 |  8885 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8886 | `			}` |
|      ! 0 |  8887 | `			break;` |
|      ! 0 |  8888 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  8889 | `			pComp = &sURI.sPass;` |
|      ! 0 |  8890 | `			if( pComp->nByte < 1 ){` |
|        - |  8891 | `				/* No available value,return NULL */` |
|      ! 0 |  8892 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8893 | `			}else{` |
|      ! 0 |  8894 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8895 | `			}` |
|      ! 0 |  8896 | `			break;` |
|      ! 0 |  8897 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  8898 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  8899 | `			if( pComp->nByte < 1 ){` |
|        - |  8900 | `				/* No available value,return NULL */` |
|      ! 0 |  8901 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8902 | `			}else{` |
|      ! 0 |  8903 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8904 | `			}` |
|      ! 0 |  8905 | `			break;` |
|      ! 0 |  8906 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  8907 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  8908 | `			if( pComp->nByte < 1 ){` |
|        - |  8909 | `				/* No available value,return NULL */` |
|      ! 0 |  8910 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8911 | `			}else{` |
|      ! 0 |  8912 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8913 | `			}` |
|      ! 0 |  8914 | `			break;` |
|      ! 0 |  8915 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  8916 | `			pComp = &sURI.sPath;` |
|      ! 0 |  8917 | `			if( pComp->nByte < 1 ){` |
|        - |  8918 | `				/* No available value,return NULL */` |
|      ! 0 |  8919 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8920 | `			}else{` |
|      ! 0 |  8921 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8922 | `			}` |
|      ! 0 |  8923 | `			break;` |
|      ! 0 |  8924 | `		default:` |
|        - |  8925 | `			/* No such entry,return NULL */` |
|      ! 0 |  8926 | `			ph7_result_null(pCtx);` |
|      ! 0 |  8927 | `			break;` |
|        - |  8928 | `		}` |
|      ! 0 |  8929 | `	}else{` |
|        - |  8930 | `		ph7_value *pArray,*pValue;` |
|        - |  8931 | `		/* Return an associative array */` |
|       27 |  8932 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  8933 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  8934 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8935 | `			/* Out of memory */` |
|      ! 0 |  8936 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  8937 | `			/* Return false */` |
|      ! 0 |  8938 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  8939 | `			return PH7_OK;` |
|        - |  8940 | `		}` |
|        - |  8941 | `		/* Fill the array */` |
|       27 |  8942 | `		pComp = &sURI.sScheme;` |
|       27 |  8943 | `		if( pComp->nByte > 0 ){` |
|       19 |  8944 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  8945 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  8946 | `		}` |
|        - |  8947 | `		/* Reset the string cursor */` |
|       27 |  8948 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8949 | `		pComp = &sURI.sHost;` |
|       27 |  8950 | `		if( pComp->nByte > 0 ){` |
|       25 |  8951 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  8952 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  8953 | `		}` |
|        - |  8954 | `		/* Reset the string cursor */` |
|       27 |  8955 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8956 | `		pComp = &sURI.sPort;` |
|       27 |  8957 | `		if( pComp->nByte > 0 ){` |
|       11 |  8958 | `			int iPort = 0;/* cc warning */` |
|        - |  8959 | `			/* Convert to integer */` |
|       11 |  8960 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  8961 | `			ph7_value_int(pValue,iPort);` |
|       11 |  8962 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  8963 | `		}` |
|        - |  8964 | `		/* Reset the string cursor */` |
|       27 |  8965 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8966 | `		pComp = &sURI.sUser;` |
|       27 |  8967 | `		if( pComp->nByte > 0 ){` |
|        7 |  8968 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  8969 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  8970 | `		}` |
|        - |  8971 | `		/* Reset the string cursor */` |
|       27 |  8972 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8973 | `		pComp = &sURI.sPass;` |
|       27 |  8974 | `		if( pComp->nByte > 0 ){` |
|        7 |  8975 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  8976 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  8977 | `		}` |
|        - |  8978 | `		/* Reset the string cursor */` |
|       27 |  8979 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8980 | `		pComp = &sURI.sPath;` |
|       27 |  8981 | `		if( pComp->nByte > 0 ){` |
|       17 |  8982 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  8983 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  8984 | `		}` |
|        - |  8985 | `		/* Reset the string cursor */` |
|       27 |  8986 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8987 | `		pComp = &sURI.sQuery;` |
|       27 |  8988 | `		if( pComp->nByte > 0 ){` |
|        5 |  8989 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  8990 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  8991 | `		}` |
|        - |  8992 | `		/* Reset the string cursor */` |
|       27 |  8993 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  8994 | `		pComp = &sURI.sFragment;` |
|       27 |  8995 | `		if( pComp->nByte > 0 ){` |
|        5 |  8996 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  8997 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  8998 | `		}` |
|        - |  8999 | `		/* Return the created array */` |
|       27 |  9000 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9001 | `		/* NOTE:` |
|        - |  9002 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9003 | `		 * automatically as soon we return from this function.` |
|        - |  9004 | `		 */` |
|        - |  9005 | `	}` |
|        - |  9006 | `	/* All done */` |
|       27 |  9007 | `	return PH7_OK;` |
|       15 |  9008 |  |
|        - |  9009 | `/*` |
|        - |  9010 | ` * Section:` |
|        - |  9011 | ` *   Array related routines.` |
|        - |  9012 | ` * Status:` |
|        - |  9013 | ` *    Stable.` |
|        - |  9014 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9015 | ` *  Array related functions that need access to the underlying` |
|        - |  9016 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9017 | ` */` |
|        - |  9018 | `/*` |
|        - |  9019 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9020 | ` * of the following structure.` |
|        - |  9021 | ` */` |
|        - |  9022 | `struct compact_data` |
|        - |  9023 |  |
|        - |  9024 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9025 | `	int nRecCount;      /* Recursion count */` |
|        - |  9026 | `};` |
|        - |  9027 | `/*` |
|        - |  9028 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9029 | ` */` |
|      ! 0 |  9030 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9031 |  |
|      ! 0 |  9032 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9033 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9034 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9035 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9036 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9037 | `		SyString sVar;` |
|      ! 0 |  9038 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9039 | `		if( sVar.nByte > 0 ){` |
|        - |  9040 | `			/* Query the current frame */` |
|      ! 0 |  9041 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9042 | `			/* ^` |
|        - |  9043 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9044 | `			 */` |
|      ! 0 |  9045 | `			if( pKey ){` |
|        - |  9046 | `				/* Perform the insertion */` |
|      ! 0 |  9047 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9048 | `			}` |
|      ! 0 |  9049 | `		}` |
|      ! 0 |  9050 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9051 | `		int rc;` |
|        - |  9052 | `		/* Recursively traverse this array */` |
|      ! 0 |  9053 | `		pData->nRecCount++;` |
|      ! 0 |  9054 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9055 | `		pData->nRecCount--;` |
|      ! 0 |  9056 | `		return rc;` |
|        - |  9057 | `	}` |
|      ! 0 |  9058 | `	return SXRET_OK;` |
|      ! 0 |  9059 |  |
|        - |  9060 | `/*` |
|        - |  9061 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9062 | ` *  Create array containing variables and their values.` |
|        - |  9063 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9064 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9065 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9066 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9067 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9068 | ` * Parameters` |
|        - |  9069 | ` *  $varname` |
|        - |  9070 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9071 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9072 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9073 | ` *   it recursively.` |
|        - |  9074 | ` * Return` |
|        - |  9075 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9076 | ` */` |
|        2 |  9077 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9078 |  |
|        - |  9079 | `	ph7_value *pArray,*pObj;` |
|        3 |  9080 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9081 | `	const char *zName;` |
|        - |  9082 | `	SyString sVar;` |
|        - |  9083 | `	int i,nLen;` |
|        3 |  9084 | `	if( nArg < 1 ){` |
|        - |  9085 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9086 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9087 | `		return PH7_OK;` |
|        - |  9088 | `	}` |
|        - |  9089 | `	/* Create the array */` |
|        3 |  9090 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9091 | `	if( pArray == 0 ){` |
|        - |  9092 | `		/* Out of memory */` |
|      ! 0 |  9093 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9094 | `		/* Return NULL */` |
|      ! 0 |  9095 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9096 | `		return PH7_OK;` |
|        - |  9097 | `	}` |
|        - |  9098 | `	/* Perform the requested operation */` |
|        7 |  9099 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9100 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9101 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9102 | `				struct compact_data sData;` |
|      ! 0 |  9103 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9104 | `				/* Recursively walk the array */` |
|      ! 0 |  9105 | `				sData.nRecCount = 0;` |
|      ! 0 |  9106 | `				sData.pArray = pArray;` |
|      ! 0 |  9107 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9108 | `			}` |
|      ! 0 |  9109 | `		}else{` |
|        - |  9110 | `			/* Extract variable name */` |
|        5 |  9111 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9112 | `			if( nLen > 0 ){` |
|        5 |  9113 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9114 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9115 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9116 | `				if( pObj ){` |
|        5 |  9117 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9118 | `				}` |
|        2 |  9119 | `			}` |
|        - |  9120 | `		}` |
|        3 |  9121 | `	}` |
|        - |  9122 | `	/* Return the array */` |
|        3 |  9123 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9124 | `	return PH7_OK;` |
|        2 |  9125 |  |
|        - |  9126 | `/*` |
|        - |  9127 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9128 | ` * of the following structure.` |
|        - |  9129 | ` */` |
|        - |  9130 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9131 | `struct extract_aux_data` |
|        - |  9132 |  |
|        - |  9133 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9134 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9135 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9136 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9137 | `	int iFlags;           /* Control flags */` |
|        - |  9138 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9139 | `};` |
|        - |  9140 | `/* Forward declaration */` |
|        - |  9141 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9142 | `/*` |
|        - |  9143 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9144 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9145 | ` * Parameters` |
|        - |  9146 | ` * $var_array` |
|        - |  9147 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9148 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9149 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9150 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9151 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9152 | ` * $extract_type` |
|        - |  9153 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9154 | ` *  It can be one of the following values:` |
|        - |  9155 | ` *   EXTR_OVERWRITE` |
|        - |  9156 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9157 | ` *   EXTR_SKIP` |
|        - |  9158 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9159 | ` *   EXTR_PREFIX_SAME` |
|        - |  9160 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9161 | ` *   EXTR_PREFIX_ALL` |
|        - |  9162 | ` *       Prefix all variable names with prefix.` |
|        - |  9163 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9164 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9165 | ` *   EXTR_IF_EXISTS` |
|        - |  9166 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9167 | ` *       otherwise do nothing.` |
|        - |  9168 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9169 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9170 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9171 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9172 | ` *      the current symbol table.` |
|        - |  9173 | ` * $prefix` |
|        - |  9174 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9175 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9176 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9177 | ` *  underscore character.` |
|        - |  9178 | ` * Return` |
|        - |  9179 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9180 | ` */` |
|        4 |  9181 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9182 |  |
|        - |  9183 | `	extract_aux_data sAux;` |
|        - |  9184 | `	ph7_hashmap *pMap;` |
|        5 |  9185 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9186 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9187 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9188 | `		return PH7_OK;` |
|        - |  9189 | `	}` |
|        - |  9190 | `	/* Point to the target hashmap */` |
|        5 |  9191 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9192 | `	if( pMap->nEntry < 1 ){` |
|        - |  9193 | `		/* Empty map,return  0 */` |
|      ! 0 |  9194 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9195 | `		return PH7_OK;` |
|        - |  9196 | `	}` |
|        - |  9197 | `	/* Prepare the aux data */` |
|        5 |  9198 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9199 | `	if( nArg > 1 ){` |
|        3 |  9200 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9201 | `		if( nArg > 2 ){` |
|      ! 0 |  9202 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9203 | `		}` |
|        1 |  9204 | `	}` |
|        5 |  9205 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9206 | `	/* Invoke the worker callback */` |
|        5 |  9207 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9208 | `	/* Number of variables successfully imported */` |
|        5 |  9209 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9210 | `	return PH7_OK;` |
|        3 |  9211 |  |
|        - |  9212 | `/*` |
|        - |  9213 | ` * Worker callback for the [extract()] function defined` |
|        - |  9214 | ` * below.` |
|        - |  9215 | ` */` |
|        8 |  9216 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9217 |  |
|        9 |  9218 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9219 | `	int iFlags = pAux->iFlags;` |
|        9 |  9220 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9221 | `	ph7_value *pObj;` |
|        - |  9222 | `	SyString sVar;` |
|        9 |  9223 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9224 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9225 | `	}` |
|        - |  9226 | `	/* Perform a string cast */` |
|        9 |  9227 | `	PH7_MemObjToString(pKey);` |
|        9 |  9228 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9229 | `		/* Unavailable variable name */` |
|      ! 0 |  9230 | `		return SXRET_OK;` |
|        - |  9231 | `	}` |
|        9 |  9232 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9233 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9234 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9235 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9236 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9237 | `			);` |
|      ! 0 |  9238 | `	}else{` |
|       13 |  9239 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9240 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9241 | `	}` |
|        9 |  9242 | `	sVar.zString = pAux->zWorker;` |
|        - |  9243 | `	/* Try to extract the variable */` |
|        9 |  9244 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9245 | `	if( pObj ){` |
|        - |  9246 | `		/* Collision */` |
|        5 |  9247 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9248 | `			return SXRET_OK;` |
|        - |  9249 | `		}` |
|        5 |  9250 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9251 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9252 | `				/* Already prefixed */` |
|      ! 0 |  9253 | `				return SXRET_OK;` |
|        - |  9254 | `			}` |
|      ! 0 |  9255 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9256 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9257 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9258 | `				);` |
|      ! 0 |  9259 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9260 | `		}` |
|        3 |  9261 | `	}else{` |
|        - |  9262 | `		/* Create the variable */` |
|        5 |  9263 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9264 | `	}` |
|        9 |  9265 | `	if( pObj ){` |
|        - |  9266 | `		/* Overwrite the old value */` |
|        9 |  9267 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9268 | `		/* Increment counter */` |
|        9 |  9269 | `		pAux->iCount++;` |
|        4 |  9270 | `	}` |
|        9 |  9271 | `	return SXRET_OK;` |
|        5 |  9272 |  |
|        - |  9273 | `/*` |
|        - |  9274 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9275 | ` * defined below.` |
|        - |  9276 | ` */` |
|        2 |  9277 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9278 |  |
|        3 |  9279 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9280 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9281 | `	ph7_value *pObj;` |
|        - |  9282 | `	SyString sVar;` |
|        - |  9283 | `	/* Perform a string cast */` |
|        3 |  9284 | `	PH7_MemObjToString(pKey);` |
|        3 |  9285 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9286 | `		/* Unavailable variable name */` |
|      ! 0 |  9287 | `		return SXRET_OK;` |
|        - |  9288 | `	}` |
|        3 |  9289 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9290 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9291 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9292 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9293 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9294 | `			);` |
|        2 |  9295 | `	}else{` |
|      ! 0 |  9296 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9297 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9298 | `	}` |
|        3 |  9299 | `	sVar.zString = pAux->zWorker;` |
|        - |  9300 | `	/* Extract the variable */` |
|        3 |  9301 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9302 | `	if( pObj ){` |
|        3 |  9303 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9304 | `	}` |
|        3 |  9305 | `	return SXRET_OK;` |
|        2 |  9306 |  |
|        - |  9307 | `/*` |
|        - |  9308 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9309 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9310 | ` * Parameters` |
|        - |  9311 | ` * $types` |
|        - |  9312 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9313 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9314 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9315 | ` *  POST includes the POST uploaded file information.` |
|        - |  9316 | ` *  Note:` |
|        - |  9317 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9318 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9319 | ` * $prefix` |
|        - |  9320 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9321 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9322 | ` *  variable named $pref_userid.` |
|        - |  9323 | ` * Return` |
|        - |  9324 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9325 | ` */` |
|        2 |  9326 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9327 |  |
|        - |  9328 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9329 | `	extract_aux_data sAux;` |
|        - |  9330 | `	int nLen,nPrefixLen;` |
|        - |  9331 | `	ph7_value *pSuper;` |
|        - |  9332 | `	ph7_vm *pVm;` |
|        - |  9333 | `	/* By default import only $_GET variables  */` |
|        3 |  9334 | `	zImport = "G";` |
|        3 |  9335 | `	nLen = (int)sizeof(char);` |
|        3 |  9336 | `	zPrefix = 0;` |
|        3 |  9337 | `	nPrefixLen = 0;` |
|        3 |  9338 | `	if( nArg > 0 ){` |
|        3 |  9339 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9340 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9341 | `		}` |
|        3 |  9342 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9343 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9344 | `		}` |
|        1 |  9345 | `	}` |
|        - |  9346 | `	/* Point to the underlying VM */` |
|        3 |  9347 | `	pVm = pCtx->pVm;` |
|        - |  9348 | `	/* Initialize the aux data */` |
|        3 |  9349 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9350 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9351 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9352 | `	sAux.pVm = pVm;` |
|        - |  9353 | `	/* Extract */` |
|        3 |  9354 | `	zEnd = &zImport[nLen];` |
|        5 |  9355 | `	while( zImport < zEnd ){` |
|        3 |  9356 | `		int c = zImport[0];` |
|        3 |  9357 | `		pSuper = 0;` |
|        3 |  9358 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9359 | `			/* Import $_GET variables */` |
|        3 |  9360 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9361 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9362 | `			/* Import $_POST variables */` |
|      ! 0 |  9363 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9364 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9365 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9366 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9367 | `		}` |
|        3 |  9368 | `		if( pSuper ){` |
|        - |  9369 | `			/* Iterate throw array entries */` |
|        3 |  9370 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9371 | `		}` |
|        - |  9372 | `		/* Advance the cursor */` |
|        3 |  9373 | `		zImport++;` |
|        1 |  9374 | `	}` |
|        - |  9375 | `	/* All done,return TRUE*/` |
|        3 |  9376 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9377 | `	return PH7_OK;` |
|        1 |  9378 |  |
|        - |  9379 | `/*` |
|        - |  9380 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9381 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9382 | ` * information.` |
|        - |  9383 | ` */` |
|     9396 |  9384 | `static sxi32 VmEvalChunk(` |
|        - |  9385 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9386 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9387 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9388 | `	int iFlags,         /* Compile flag */` |
|        - |  9389 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9390 | `	)` |
|        2 |  9391 |  |
|        - |  9392 | `	SySet *pByteCode,aByteCode;` |
|     9398 |  9393 | `	ProcConsumer xErr = 0;` |
|     9398 |  9394 | `	void *pErrData = 0;` |
|        - |  9395 | `	/* Initialize bytecode container */` |
|     9398 |  9396 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9398 |  9397 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9398 | `	/* Reset the code generator */` |
|     9398 |  9399 | `	if( bTrueReturn ){` |
|        - |  9400 | `		/* Included file,log compile-time errors */` |
|     7469 |  9401 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7469 |  9402 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3734 |  9403 | `	}` |
|     9398 |  9404 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9405 | `	/* Swap bytecode container */` |
|     9398 |  9406 | `	pByteCode = pVm->pByteContainer;` |
|     9398 |  9407 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9408 | `	/* Compile the chunk */` |
|     9398 |  9409 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14096 |  9410 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9411 | `		/* Compilation error,return false */` |
|        3 |  9412 | `		if( pCtx ){` |
|        3 |  9413 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9414 | `		}` |
|        2 |  9415 | `	}else{` |
|        - |  9416 | `		/* Mount any newly defined classes */` |
|        - |  9417 | `		SyHashEntry *pEntry;` |
|        - |  9418 | `		ph7_class *pClass;` |
|        - |  9419 | `		ph7_value sResult; /* Return value */` |
|        - |  9420 | `		sxi32 rc;` |
|     9396 |  9421 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   257271 |  9422 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   243180 |  9423 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9424 | `			/* Only mount classes that haven't been mounted yet */` |
|   243180 |  9425 | `			if( !pClass->bMounted ){` |
|    52912 |  9426 | `				rc = VmMountUserClass(pVm,pClass);` |
|    52912 |  9427 | `				if( rc != SXRET_OK ){` |
|        - |  9428 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9429 | `					if( pCtx ){` |
|      ! 0 |  9430 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9431 | `					}` |
|      ! 0 |  9432 | `					goto Cleanup;` |
|        - |  9433 | `				}` |
|    26455 |  9434 | `			}` |
|        2 |  9435 | `		}` |
|     9396 |  9436 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9437 | `			/* Out of memory */` |
|      ! 0 |  9438 | `			if( pCtx ){` |
|      ! 0 |  9439 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9440 | `			}` |
|      ! 0 |  9441 | `			goto Cleanup;` |
|        - |  9442 | `		}` |
|     9396 |  9443 | `		if( bTrueReturn ){` |
|        - |  9444 | `			/* Assume a boolean true return value */` |
|     7469 |  9445 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3735 |  9446 | `		}else{` |
|        - |  9447 | `			/* Assume a null return value */` |
|     1928 |  9448 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9449 | `		}` |
|        - |  9450 | `		/* Execute the compiled chunk */` |
|     9396 |  9451 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9396 |  9452 | `		if( pCtx ){` |
|        - |  9453 | `			/* Set the execution result */` |
|     7486 |  9454 | `			ph7_result_value(pCtx,&sResult);` |
|     3742 |  9455 | `		}` |
|     9396 |  9456 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9457 | `	}` |
|     4698 |  9458 | `Cleanup:` |
|        - |  9459 | `	/* Cleanup the mess left behind */` |
|     9398 |  9460 | `	pVm->pByteContainer = pByteCode;` |
|     9398 |  9461 | `	SySetRelease(&aByteCode);` |
|     9398 |  9462 | `	return SXRET_OK;` |
|        2 |  9463 |  |
|        - |  9464 | `/*` |
|        - |  9465 | ` * value eval(string $code)` |
|        - |  9466 | ` *   Evaluate a string as PHP code.` |
|        - |  9467 | ` * Parameter` |
|        - |  9468 | ` *  code: PHP code to evaluate.` |
|        - |  9469 | ` * Return` |
|        - |  9470 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9471 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9472 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9473 | ` */` |
|       16 |  9474 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9475 |  |
|        - |  9476 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9477 | `	if( nArg < 1 ){` |
|        - |  9478 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9479 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9480 | `		return SXRET_OK;` |
|        - |  9481 | `	}` |
|        - |  9482 | `	/* Chunk to evaluate */` |
|       18 |  9483 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9484 | `	if( sChunk.nByte < 1 ){` |
|        - |  9485 | `		/* Empty string,return NULL */` |
|        3 |  9486 | `		ph7_result_null(pCtx);` |
|        3 |  9487 | `		return SXRET_OK;` |
|        - |  9488 | `	}` |
|        - |  9489 | `	/* Eval the chunk */` |
|       16 |  9490 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9491 | `	return SXRET_OK;` |
|       10 |  9492 |  |
|        - |  9493 | `/*` |
|        - |  9494 | ` * Check if a file path is already included.` |
|        - |  9495 | ` */` |
|    14932 |  9496 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9497 |  |
|        - |  9498 | `	SyString *aEntries;` |
|        - |  9499 | `	sxu32 n;` |
|    14933 |  9500 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9501 | `	/* Perform a linear search */` |
| 55730729 |  9502 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 55715803 |  9503 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9504 | `			/* Already included */` |
|        7 |  9505 | `			return TRUE;` |
|        - |  9506 | `		}` |
| 27857899 |  9507 | `	}` |
|    14927 |  9508 | `	return FALSE;` |
|     7467 |  9509 |  |
|        - |  9510 | `/*` |
|        - |  9511 | ` * Push a file path in the appropriate VM container.` |
|        - |  9512 | ` */` |
|    16834 |  9513 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9514 |  |
|        - |  9515 | `	SyString sPath;` |
|        - |  9516 | `	char *zDup;` |
|        - |  9517 | `#ifdef __WINNT__` |
|        - |  9518 | `	char *zCur;` |
|        - |  9519 | `#endif` |
|        - |  9520 | `	sxi32 rc;` |
|    16836 |  9521 | `	if( nLen < 0 ){` |
|     1904 |  9522 | `		nLen = SyStrlen(zPath);` |
|      951 |  9523 | `	}` |
|        - |  9524 | `	/* Duplicate the file path first */` |
|    16836 |  9525 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    16836 |  9526 | `	if( zDup == 0 ){` |
|      ! 0 |  9527 | `		return SXERR_MEM;` |
|        - |  9528 | `	}` |
|        - |  9529 | `#ifdef __WINNT__` |
|        - |  9530 | `	/* Normalize path on windows` |
|        - |  9531 | `	 * Example:` |
|        - |  9532 | `	 *    Path/To/File.php` |
|        - |  9533 | `	 * becomes` |
|        - |  9534 | `	 *   path\to\file.php` |
|        - |  9535 | `	 */` |
|        2 |  9536 | `	zCur = zDup;` |
|        2 |  9537 | `	while( zCur[0] != 0 ){` |
|        2 |  9538 | `		if( zCur[0] == '/' ){` |
|        2 |  9539 | `			zCur[0] = '\\';` |
|        2 |  9540 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9541 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9542 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9543 | `		}` |
|        2 |  9544 | `		zCur++;` |
|        2 |  9545 | `	}` |
|        - |  9546 | `#endif` |
|        - |  9547 | `	/* Install the file path */` |
|    16836 |  9548 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    16836 |  9549 | `	if( !bMain ){` |
|    14933 |  9550 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9551 | `			/* Already included */` |
|        7 |  9552 | `			*pNew = 0;` |
|        4 |  9553 | `		}else{` |
|        - |  9554 | `			/* Insert in the corresponding container */` |
|    14927 |  9555 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    14927 |  9556 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9557 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9558 | `				return rc;` |
|        - |  9559 | `			}` |
|    14927 |  9560 | `			*pNew = 1;` |
|        - |  9561 | `		}` |
|     7466 |  9562 | `	}` |
|    16836 |  9563 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    16836 |  9564 | `	return SXRET_OK;` |
|     8419 |  9565 |  |
|        - |  9566 | `/*` |
|        - |  9567 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9568 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9569 | ` * indicates failure.` |
|        - |  9570 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9571 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9572 | ` * operations.` |
|        - |  9573 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9574 | ` * this function is a no-op.` |
|        - |  9575 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9576 | ` * constructs for more information.` |
|        - |  9577 | ` */` |
|     7474 |  9578 | `static sxi32 VmExecIncludedFile(` |
|        - |  9579 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9580 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9581 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9582 | `	 )` |
|        2 |  9583 |  |
|        - |  9584 | `	sxi32 rc;` |
|        - |  9585 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9586 | `	const ph7_io_stream *pStream;` |
|        - |  9587 | `	SyBlob sContents;` |
|        - |  9588 | `	void *pHandle;` |
|        - |  9589 | `	ph7_vm *pVm;` |
|        - |  9590 | `	int isNew;` |
|        - |  9591 | `	/* Initialize fields */` |
|     7476 |  9592 | `	pVm = pCtx->pVm;` |
|     7476 |  9593 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7476 |  9594 | `	isNew = 0;` |
|        - |  9595 | `	/* Extract the associated stream */` |
|     7476 |  9596 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9597 | `	/*` |
|        - |  9598 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9599 | `	 * in a read-only mode.` |
|        - |  9600 | `	 */` |
|     7476 |  9601 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7476 |  9602 | `	if( pHandle == 0 ){` |
|        3 |  9603 | `		return SXERR_IO;` |
|        - |  9604 | `	}` |
|     7473 |  9605 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7473 |  9606 | `	if( IncludeOnce && !isNew ){` |
|        - |  9607 | `		/* Already included */` |
|        5 |  9608 | `		rc = SXERR_EXISTS;` |
|        3 |  9609 | `	}else{` |
|        - |  9610 | `		/* Read the whole file contents */` |
|     7469 |  9611 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7469 |  9612 | `		if( rc == SXRET_OK ){` |
|        - |  9613 | `			SyString sScript;` |
|        - |  9614 | `			/* Compile and execute the script */` |
|     7469 |  9615 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7469 |  9616 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3734 |  9617 | `		}` |
|        - |  9618 | `	}` |
|        - |  9619 | `	/* Pop from the set of included file */` |
|     7473 |  9620 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9621 | `	/* Close the handle */` |
|     7473 |  9622 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9623 | `	/* Release the working buffer */` |
|     7473 |  9624 | `	SyBlobRelease(&sContents);` |
|        - |  9625 | `#else` |
|        - |  9626 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9627 | `	SXUNUSED(pPath);` |
|        - |  9628 | `	SXUNUSED(IncludeOnce);` |
|        - |  9629 | `	rc = SXERR_IO;` |
|        - |  9630 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7473 |  9631 | `	return rc;` |
|     3739 |  9632 |  |
|        - |  9633 | `/*` |
|        - |  9634 | ` * string get_include_path(void)` |
|        - |  9635 | ` *  Gets the current include_path configuration option.` |
|        - |  9636 | ` * Parameter` |
|        - |  9637 | ` *  None` |
|        - |  9638 | ` * Return` |
|        - |  9639 | ` *  Included paths as a string` |
|        - |  9640 | ` */` |
|        2 |  9641 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9642 |  |
|        3 |  9643 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9644 | `	SyString *aEntry;` |
|        - |  9645 | `	int dir_sep;` |
|        - |  9646 | `	sxu32 n;` |
|        - |  9647 | `#ifdef __WINNT__` |
|        1 |  9648 | `	dir_sep = ';';` |
|        - |  9649 | `#else` |
|        - |  9650 | `	/* Assume UNIX path separator */` |
|        2 |  9651 | `	dir_sep = ':';` |
|        - |  9652 | `#endif` |
|        1 |  9653 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9654 | `	SXUNUSED(apArg);` |
|        - |  9655 | `	/* Point to the list of import paths */` |
|        3 |  9656 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9657 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9658 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9659 | `		if( n > 0 ){` |
|        - |  9660 | `			/* Append dir seprator */` |
|      ! 0 |  9661 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9662 | `		}` |
|        - |  9663 | `		/* Append path */` |
|        3 |  9664 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9665 | `	}` |
|        3 |  9666 | `	return PH7_OK;` |
|        1 |  9667 |  |
|        - |  9668 | `/*` |
|        - |  9669 | ` * string get_get_included_files(void)` |
|        - |  9670 | ` *  Gets the current include_path configuration option.` |
|        - |  9671 | ` * Parameter` |
|        - |  9672 | ` *  None` |
|        - |  9673 | ` * Return` |
|        - |  9674 | ` *  Included paths as a string` |
|        - |  9675 | ` */` |
|        2 |  9676 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9677 |  |
|        3 |  9678 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9679 | `	ph7_value *pArray,*pWorker;` |
|        - |  9680 | `	SyString *pEntry;` |
|        - |  9681 | `	int c,d;` |
|        - |  9682 | `	/* Create an array and a working value */` |
|        3 |  9683 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9684 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9685 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9686 | `		/* Out of memory,return null */` |
|      ! 0 |  9687 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9688 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9689 | `		SXUNUSED(apArg);` |
|      ! 0 |  9690 | `		return PH7_OK;` |
|        - |  9691 | `	}` |
|        3 |  9692 | `	c = d = '/';` |
|        - |  9693 | `#ifdef __WINNT__` |
|        1 |  9694 | `	d = '\\';` |
|        - |  9695 | `#endif` |
|        - |  9696 | `	/* Iterate throw entries */` |
|        3 |  9697 | `	SySetResetCursor(pFiles);` |
|     3627 |  9698 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9699 | `		const char *zBase,*zEnd;` |
|        - |  9700 | `		int iLen;` |
|        - |  9701 | `		/* reset the string cursor */` |
|     3625 |  9702 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9703 | `		/* Extract base name */` |
|     3625 |  9704 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9705 | `		/* Ignore trailing '/' */` |
|     5437 |  9706 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9707 | `			zEnd--;` |
|      ! 0 |  9708 | `		}` |
|     3625 |  9709 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   111273 |  9710 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   105837 |  9711 | `			zEnd--;` |
|        1 |  9712 | `		}` |
|     3625 |  9713 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3625 |  9714 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9715 | `		/* Copy entry name */` |
|     3625 |  9716 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9717 | `		/* Perform the insertion */` |
|     3625 |  9718 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9719 | `	}` |
|        - |  9720 | `	/* All done,return the created array */` |
|        3 |  9721 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9722 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9723 | `	 * by the engine as soon we return from this foreign` |
|        - |  9724 | `	 * function.` |
|        - |  9725 | `	 */` |
|        3 |  9726 | `	return PH7_OK;` |
|        2 |  9727 |  |
|        - |  9728 | `/*` |
|        - |  9729 | ` * include:` |
|        - |  9730 | ` * According to the PHP reference manual.` |
|        - |  9731 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9732 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9733 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9734 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9735 | ` *  and the current working directory before failing. The include()` |
|        - |  9736 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9737 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9738 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9739 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9740 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9741 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9742 | ` *  directory to find the requested file.` |
|        - |  9743 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9744 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9745 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9746 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9747 | ` */` |
|     7462 |  9748 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9749 |  |
|        - |  9750 | `	SyString sFile;` |
|        - |  9751 | `	sxi32 rc;` |
|     7464 |  9752 | `	if( nArg < 1 ){` |
|        - |  9753 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9754 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9755 | `		return SXRET_OK;` |
|        - |  9756 | `	}` |
|        - |  9757 | `	/* File to include */` |
|     7464 |  9758 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7464 |  9759 | `	if( sFile.nByte < 1 ){` |
|        - |  9760 | `		/* Empty string,return NULL */` |
|      ! 0 |  9761 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9762 | `		return SXRET_OK;` |
|        - |  9763 | `	}` |
|        - |  9764 | `	/* Open,compile and execute the desired script */` |
|     7464 |  9765 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7464 |  9766 | `	if( rc != SXRET_OK ){` |
|        - |  9767 | `		/* Emit a warning and return false */` |
|        3 |  9768 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9769 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9770 | `	}` |
|     7464 |  9771 | `	return SXRET_OK;` |
|     3733 |  9772 |  |
|        - |  9773 | `/*` |
|        - |  9774 | ` * include_once:` |
|        - |  9775 | ` *  According to the PHP reference manual.` |
|        - |  9776 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9777 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9778 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9779 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9780 | ` *   just once.` |
|        - |  9781 | ` */` |
|        4 |  9782 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9783 |  |
|        - |  9784 | `	SyString sFile;` |
|        - |  9785 | `	sxi32 rc;` |
|        5 |  9786 | `	if( nArg < 1 ){` |
|        - |  9787 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9788 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9789 | `		return SXRET_OK;` |
|        - |  9790 | `	}` |
|        - |  9791 | `	/* File to include */` |
|        5 |  9792 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9793 | `	if( sFile.nByte < 1 ){` |
|        - |  9794 | `		/* Empty string,return NULL */` |
|      ! 0 |  9795 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9796 | `		return SXRET_OK;` |
|        - |  9797 | `	}` |
|        - |  9798 | `	/* Open,compile and execute the desired script */` |
|        5 |  9799 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9800 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9801 | `		/* File already included,return TRUE */` |
|        3 |  9802 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9803 | `		return SXRET_OK;` |
|        - |  9804 | `	}` |
|        3 |  9805 | `	if( rc != SXRET_OK ){` |
|        - |  9806 | `		/* Emit a warning and return false */` |
|      ! 0 |  9807 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9808 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9809 | ` 	}` |
|        3 |  9810 | `	return SXRET_OK;` |
|        3 |  9811 |  |
|        - |  9812 | `/*` |
|        - |  9813 | ` * require.` |
|        - |  9814 | ` *  According to the PHP reference manual.` |
|        - |  9815 | ` *   require() is identical to include() except upon failure it will` |
|        - |  9816 | ` *   also produce a fatal level error.` |
|        - |  9817 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  9818 | ` *   emits a warning  which allows the script to continue.` |
|        - |  9819 | ` */` |
|        4 |  9820 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9821 |  |
|        - |  9822 | `	SyString sFile;` |
|        - |  9823 | `	sxi32 rc;` |
|        5 |  9824 | `	if( nArg < 1 ){` |
|        - |  9825 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9826 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9827 | `		return SXRET_OK;` |
|        - |  9828 | `	}` |
|        - |  9829 | `	/* File to include */` |
|        5 |  9830 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9831 | `	if( sFile.nByte < 1 ){` |
|        - |  9832 | `		/* Empty string,return NULL */` |
|      ! 0 |  9833 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9834 | `		return SXRET_OK;` |
|        - |  9835 | `	}` |
|        - |  9836 | `	/* Open,compile and execute the desired script */` |
|        5 |  9837 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 |  9838 | `	if( rc != SXRET_OK ){` |
|        - |  9839 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9840 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9841 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9842 | `		return PH7_ABORT;` |
|        - |  9843 | `	}` |
|        5 |  9844 | `	return SXRET_OK;` |
|        3 |  9845 |  |
|        - |  9846 | `/*` |
|        - |  9847 | ` * require_once:` |
|        - |  9848 | ` *  According to the PHP reference manual.` |
|        - |  9849 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  9850 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  9851 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  9852 | ` *   and how it differs from its non _once siblings.` |
|        - |  9853 | ` */` |
|        4 |  9854 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9855 |  |
|        - |  9856 | `	SyString sFile;` |
|        - |  9857 | `	sxi32 rc;` |
|        5 |  9858 | `	if( nArg < 1 ){` |
|        - |  9859 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9860 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9861 | `		return SXRET_OK;` |
|        - |  9862 | `	}` |
|        - |  9863 | `	/* File to include */` |
|        5 |  9864 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9865 | `	if( sFile.nByte < 1 ){` |
|        - |  9866 | `		/* Empty string,return NULL */` |
|      ! 0 |  9867 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9868 | `		return SXRET_OK;` |
|        - |  9869 | `	}` |
|        - |  9870 | `	/* Open,compile and execute the desired script */` |
|        5 |  9871 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9872 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9873 | `		/* File already included,return TRUE */` |
|        3 |  9874 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9875 | `		return SXRET_OK;` |
|        - |  9876 | `	}` |
|        3 |  9877 | `	if( rc != SXRET_OK ){` |
|        - |  9878 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9879 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9880 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9881 | `		return PH7_ABORT;` |
|        - |  9882 | `	}` |
|        3 |  9883 | `	return SXRET_OK;` |
|        3 |  9884 |  |
|        - |  9885 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  9886 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  9887 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  9888 | `/* Table of built-in VM functions. */` |
|        - |  9889 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9890 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9891 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9892 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9893 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9894 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9895 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9896 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9897 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9898 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9899 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9900 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9901 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9902 | `	    /* Constants management */` |
|        - |  9903 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9904 | `	{ "define",   vm_builtin_define               },` |
|        - |  9905 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9906 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9907 | `	   /* Class/Object functions */` |
|        - |  9908 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9909 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9910 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9911 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9912 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9913 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9914 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9915 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9916 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9917 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9918 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - |  9919 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - |  9920 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - |  9921 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - |  9922 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - |  9923 | `	{ "is_a", vm_builtin_is_a },` |
|        - |  9924 | `	   /* Random numbers/strings generators */` |
|        - |  9925 | `	{ "rand",          vm_builtin_rand            },` |
|        - |  9926 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - |  9927 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - |  9928 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - |  9929 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - |  9930 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9931 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9932 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - |  9933 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9934 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9935 | `	   /* Language constructs functions */` |
|        - |  9936 | `	{ "echo",  vm_builtin_echo                    },` |
|        - |  9937 | `	{ "print", vm_builtin_print                   },` |
|        - |  9938 | `	{ "exit",  vm_builtin_exit                    },` |
|        - |  9939 | `	{ "die",   vm_builtin_exit                    },` |
|        - |  9940 | `	{ "eval",  vm_builtin_eval                    },` |
|        - |  9941 | `	  /* Variable handling functions */` |
|        - |  9942 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - |  9943 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - |  9944 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - |  9945 | `	{ "isset",     vm_builtin_isset                },` |
|        - |  9946 | `	{ "unset",     vm_builtin_unset                },` |
|        - |  9947 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - |  9948 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - |  9949 | `	{ "var_export",vm_builtin_var_export           },` |
|        - |  9950 | `	  /* Ouput control functions */` |
|        - |  9951 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - |  9952 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - |  9953 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - |  9954 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - |  9955 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - |  9956 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - |  9957 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - |  9958 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - |  9959 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - |  9960 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - |  9961 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - |  9962 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - |  9963 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - |  9964 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - |  9965 | `	  /* Assertion functions */` |
|        - |  9966 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - |  9967 | `	{ "assert",          vm_builtin_assert         },` |
|        - |  9968 | `	  /* Error reporting functions */` |
|        - |  9969 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - |  9970 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - |  9971 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - |  9972 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - |  9973 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - |  9974 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - |  9975 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - |  9976 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - |  9977 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - |  9978 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - |  9979 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - |  9980 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - |  9981 | `	  /* Release info */` |
|        - |  9982 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - |  9983 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - |  9984 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - |  9985 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - |  9986 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - |  9987 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - |  9988 | `	  /* hashmap */` |
|        - |  9989 | `	{"compact",          vm_builtin_compact       },` |
|        - |  9990 | `	{"extract",          vm_builtin_extract       },` |
|        - |  9991 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - |  9992 | `	  /* URL related function */` |
|        - |  9993 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - |  9994 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - |  9995 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9996 | `	   /* XML processing functions */` |
|        - |  9997 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - |  9998 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - |  9999 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10000 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10001 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10002 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10003 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10004 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10005 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10006 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10007 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10008 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10009 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10010 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10011 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10012 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10013 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10014 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10015 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10016 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10017 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10018 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10019 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10020 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10021 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10022 | `	   /* Command line processing */` |
|        - | 10023 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10024 | `	   /* JSON encoding/decoding */` |
|        - | 10025 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10026 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10027 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10028 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10029 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10030 | `	   /* Files/URI inclusion facility */` |
|        - | 10031 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10032 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10033 | `	{ "include",      vm_builtin_include          },` |
|        - | 10034 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10035 | `	{ "require",      vm_builtin_require          },` |
|        - | 10036 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10037 | `};` |
|        - | 10038 | `/*` |
|        - | 10039 | ` * Register the built-in VM functions defined above.` |
|        - | 10040 | ` */` |
|     1672 | 10041 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10042 |  |
|        - | 10043 | `	sxi32 rc;` |
|        - | 10044 | `	sxu32 n;` |
|   209002 | 10045 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10046 | `		/* Note that these special functions have access` |
|        - | 10047 | `		 * to the underlying virtual machine as their` |
|        - | 10048 | `		 * private data.` |
|        - | 10049 | `		 */` |
|   207330 | 10050 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   207330 | 10051 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10052 | `			return rc;` |
|        - | 10053 | `		}` |
|   103666 | 10054 | `	}` |
|     1674 | 10055 | `	return SXRET_OK;` |
|      838 | 10056 |  |
|        - | 10057 | `/*` |
|        - | 10058 | ` * Check if the given name refer to an installed class.` |
|        - | 10059 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10060 | ` */` |
|    10604 | 10061 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10062 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10063 | `	const char *zName,  /* Name of the target class */` |
|        - | 10064 | `	sxu32 nByte,        /* zName length */` |
|        - | 10065 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10066 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10067 | `						 */` |
|        - | 10068 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10069 | `	)` |
|        2 | 10070 |  |
|        - | 10071 | `	SyHashEntry *pEntry;` |
|        - | 10072 | `	ph7_class *pClass;` |
|     5302 | 10073 | `		SXUNUSED(iNest);` |
|        - | 10074 | `	/* Perform a hash lookup */` |
|    10606 | 10075 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10076 |  |
|    10606 | 10077 | `	if( pEntry == 0 ){` |
|        - | 10078 | `		/* No such entry,return NULL */` |
|      ! 0 | 10079 | `		return 0;` |
|        - | 10080 | `	}` |
|    10606 | 10081 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    10606 | 10082 | `	if( !iLoadable ){` |
|        - | 10083 | `		/* Return the first class seen */` |
|     9714 | 10084 | `		return pClass;` |
|      ! 0 | 10085 | `	}else{` |
|        - | 10086 | `		/* Check the collision list */` |
|      894 | 10087 | `		while(pClass){` |
|      894 | 10088 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10089 | `				/* Class is loadable */` |
|      894 | 10090 | `				return pClass;` |
|        - | 10091 | `			}` |
|        - | 10092 | `			/* Point to the next entry */` |
|      ! 0 | 10093 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10094 | `		}` |
|        - | 10095 | `	}` |
|        - | 10096 | `	/* No such loadable class */` |
|      ! 0 | 10097 | `	return 0;` |
|     5304 | 10098 |  |
|        - | 10099 | `/*` |
|        - | 10100 | ` * Reference Table Implementation` |
|        - | 10101 | ` * Status: stable <chm@symisc.net>` |
|        - | 10102 | ` * Intro` |
|        - | 10103 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10104 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10105 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10106 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10107 | ` *  Refer to the official for more information on this powerful` |
|        - | 10108 | ` *  extension.` |
|        - | 10109 | ` */` |
|        - | 10110 | `/*` |
|        - | 10111 | ` * Allocate a new reference entry.` |
|        - | 10112 | ` */` |
|  2938686 | 10113 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10114 |  |
|        - | 10115 | `	VmRefObj *pRef;` |
|        - | 10116 | `	/* Allocate a new instance */` |
|  2938688 | 10117 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2938688 | 10118 | `	if( pRef == 0 ){` |
|      ! 0 | 10119 | `		return 0;` |
|        - | 10120 | `	}` |
|        - | 10121 | `	/* Zero the structure */` |
|  2938688 | 10122 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10123 | `	/* Initialize fields */` |
|  2938688 | 10124 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2938688 | 10125 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2938688 | 10126 | `	pRef->nIdx = nIdx;` |
|  2938688 | 10127 | `	return pRef;` |
|  1469345 | 10128 |  |
|        - | 10129 | `/*` |
|        - | 10130 | ` * Default hash function used by the reference table` |
|        - | 10131 | ` * for lookup/insertion operations.` |
|        - | 10132 | ` */` |
| 16369942 | 10133 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10134 |  |
|        - | 10135 | `	/* Calculate the hash based on the memory object index */` |
| 16369944 | 10136 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10137 |  |
|        - | 10138 | `/*` |
|        - | 10139 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10140 | ` * in the reference table.` |
|        - | 10141 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10142 | ` * otherwise.` |
|        - | 10143 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10144 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10145 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10146 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10147 | ` * Refer to the official for more information on this powerful` |
|        - | 10148 | ` * extension.` |
|        - | 10149 | ` */` |
|  8781172 | 10150 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10151 |  |
|        - | 10152 | `	VmRefObj *pRef;` |
|        - | 10153 | `	sxu32 nBucket;` |
|        - | 10154 | `	/* Point to the appropriate bucket */` |
|  8781174 | 10155 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10156 | `	/* Perform the lookup */` |
|  8781174 | 10157 | `	pRef = pVm->apRefObj[nBucket];` |
| 18519249 | 10158 | `	for(;;){` |
| 37040129 | 10159 | `		if( pRef == 0 ){` |
|  3005734 | 10160 | `			break;` |
|        - | 10161 | `		}` |
| 34034397 | 10162 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10163 | `			/* Entry found */` |
|  5775442 | 10164 | `			return pRef;` |
|        - | 10165 | `		}` |
|        - | 10166 | `		/* Point to the next entry */` |
| 28258957 | 10167 | `		pRef = pRef->pNextCollide;` |
|        2 | 10168 | `	}` |
|        - | 10169 | `	/* No such entry,return NULL */` |
|  3005734 | 10170 | `	return 0;` |
|  4390588 | 10171 |  |
|        - | 10172 | `/*` |
|        - | 10173 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10174 | ` *` |
|        - | 10175 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10176 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10177 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10178 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10179 | ` * Refer to the official for more information on this powerful` |
|        - | 10180 | ` * extension.` |
|        - | 10181 | ` */` |
|  2938686 | 10182 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10183 |  |
|        - | 10184 | `	sxu32 nBucket;` |
|  2938688 | 10185 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10186 | `		VmRefObj **apNew;` |
|        - | 10187 | `		sxu32 nNew;` |
|        - | 10188 | `		/* Allocate a larger table */` |
|     2572 | 10189 | `		nNew = pVm->nRefSize << 1;` |
|     2572 | 10190 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     2572 | 10191 | `		if( apNew ){` |
|     2572 | 10192 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10193 | `			sxu32 n;` |
|        - | 10194 | `			/* Zero the structure */` |
|     2572 | 10195 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10196 | `			/* Rehash all referenced entries */` |
|  2825344 | 10197 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10198 | `				/* Remove old collision links */` |
|  2822774 | 10199 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10200 | `				/* Point to the appropriate bucket */` |
|  2822774 | 10201 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10202 | `				/* Insert the entry  */` |
|  2822774 | 10203 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2822774 | 10204 | `				if( apNew[nBucket] ){` |
|  2298896 | 10205 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10206 | `				}` |
|  2822774 | 10207 | `				apNew[nBucket] = pEntry;` |
|        - | 10208 | `				/* Point to the next entry */` |
|  2822774 | 10209 | `				pEntry = pEntry->pNext;` |
|  1411388 | 10210 | `			}` |
|        - | 10211 | `			/* Release the old table */` |
|     2572 | 10212 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10213 | `			/* Install the new one */` |
|     2572 | 10214 | `			pVm->apRefObj = apNew;` |
|     2572 | 10215 | `			pVm->nRefSize = nNew;` |
|     1285 | 10216 | `		}` |
|     1285 | 10217 | `	}` |
|        - | 10218 | `	/* Point to the appropriate bucket */` |
|  2938688 | 10219 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10220 | `	/* Insert the entry */` |
|  2938688 | 10221 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2938688 | 10222 | `	if( pVm->apRefObj[nBucket] ){` |
|  2431871 | 10223 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1216413 | 10224 | `	}` |
|  2938688 | 10225 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2938688 | 10226 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2938688 | 10227 | `	pVm->nRefUsed++;` |
|  2938688 | 10228 | `	return SXRET_OK;` |
|        2 | 10229 |  |
|        - | 10230 | `/*` |
|        - | 10231 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10232 | ` * the reference table.` |
|        - | 10233 | ` * This function is invoked when the user perform an unset` |
|        - | 10234 | ` * call [i.e: unset($var); ].` |
|        - | 10235 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10236 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10237 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10238 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10239 | ` * Refer to the official for more information on this powerful` |
|        - | 10240 | ` * extension.` |
|        - | 10241 | ` */` |
|  2914044 | 10242 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10243 |  |
|        - | 10244 | `	ph7_hashmap_node **apNode;` |
|        - | 10245 | `	SyHashEntry **apEntry;` |
|        - | 10246 | `	sxu32 n;` |
|        - | 10247 | `	/* Point to the reference table */` |
|  2914046 | 10248 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2914046 | 10249 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10250 | `	/* Unlink the entry from the reference table */` |
|  2986014 | 10251 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    71970 | 10252 | `		if( apEntry[n] ){` |
|    71920 | 10253 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    35959 | 10254 | `		}` |
|    35986 | 10255 | `	}` |
|  5758678 | 10256 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2844634 | 10257 | `		if( apNode[n] ){` |
|     5595 | 10258 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2797 | 10259 | `		}` |
|  1422318 | 10260 | `	}` |
|  2914046 | 10261 | `	if( pRef->pPrevCollide ){` |
|  1086734 | 10262 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   543348 | 10263 | `	}else{` |
|  1827314 | 10264 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10265 | `	}` |
|  2914046 | 10266 | `	if( pRef->pNextCollide ){` |
|  1626659 | 10267 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   813838 | 10268 | `	}` |
|  2914046 | 10269 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10270 | `	/* Release the node */` |
|  2914046 | 10271 | `	SySetRelease(&pRef->aReference);` |
|  2914046 | 10272 | `	SySetRelease(&pRef->aArrEntries);` |
|  2914046 | 10273 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2914046 | 10274 | `	pVm->nRefUsed--;` |
|  2914046 | 10275 | `	return SXRET_OK;` |
|        2 | 10276 |  |
|        - | 10277 | `/*` |
|        - | 10278 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10279 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10280 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10281 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10282 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10283 | ` * Refer to the official for more information on this powerful` |
|        - | 10284 | ` * extension.` |
|        - | 10285 | ` */` |
|  2960988 | 10286 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10287 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10288 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10289 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10290 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10291 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10292 | `	)` |
|        2 | 10293 |  |
|  2960990 | 10294 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10295 | `	VmRefObj *pRef;` |
|        - | 10296 | `	/* Check if the referenced object already exists */` |
|  2960990 | 10297 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2960990 | 10298 | `	if( pRef == 0 ){` |
|        - | 10299 | `		/* Create a new entry */` |
|  2938688 | 10300 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2938688 | 10301 | `		if( pRef == 0 ){` |
|      ! 0 | 10302 | `			return SXERR_MEM;` |
|        - | 10303 | `		}` |
|  2938688 | 10304 | `		pRef->iFlags = iFlags;` |
|        - | 10305 | `		/* Install the entry */` |
|  2938688 | 10306 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1469343 | 10307 | `	}` |
|  2965902 | 10308 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10309 | `		/* Safely ignore the exception frame */` |
|     4914 | 10310 | `		pFrame = pFrame->pParent;` |
|        2 | 10311 | `	}` |
|  2960990 | 10312 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10313 | `		VmSlot sRef;` |
|        - | 10314 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10315 | `		 * be deleted when we leave this frame.` |
|        - | 10316 | `		 */` |
|    67078 | 10317 | `		sRef.nIdx = nIdx;` |
|    67078 | 10318 | `		sRef.pUserData = pEntry;` |
|    67078 | 10319 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10320 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10321 | `		}` |
|    33538 | 10322 | `	}` |
|  2960990 | 10323 | `	if( pEntry ){` |
|        - | 10324 | `		/* Address of the hash-entry */` |
|    89194 | 10325 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    44596 | 10326 | `	}` |
|  2960990 | 10327 | `	if( pMapEntry ){` |
|        - | 10328 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2867730 | 10329 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1433864 | 10330 | `	}` |
|  2960990 | 10331 | `	return SXRET_OK;` |
|  1480496 | 10332 |  |
|        - | 10333 | `/*` |
|        - | 10334 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10335 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10336 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10337 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10338 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10339 | ` * Refer to the official for more information on this powerful` |
|        - | 10340 | ` * extension.` |
|        - | 10341 | ` */` |
|  2906120 | 10342 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10343 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10344 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10345 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10346 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10347 | `	)` |
|        2 | 10348 |  |
|        - | 10349 | `	VmRefObj *pRef;` |
|        - | 10350 | `	sxu32 n;` |
|        - | 10351 | `	/* Check if the referenced object already exists */` |
|  2906122 | 10352 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2906122 | 10353 | `	if( pRef == 0 ){` |
|        - | 10354 | `		/* Not such entry */` |
|    67028 | 10355 | `		return SXERR_NOTFOUND;` |
|        - | 10356 | `	}` |
|        - | 10357 | `	/* Remove the desired entry */` |
|  2839096 | 10358 | `	if( pEntry ){` |
|        - | 10359 | `		SyHashEntry **apEntry;` |
|       51 | 10360 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 10361 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 10362 | `			if( apEntry[n] == pEntry ){` |
|        - | 10363 | `				/* Nullify the entry */` |
|       51 | 10364 | `				apEntry[n] = 0;` |
|        - | 10365 | `				/*` |
|        - | 10366 | `				 * NOTE:` |
|        - | 10367 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10368 | `				 * we avoid wasting spaces.` |
|        - | 10369 | `				 */` |
|       25 | 10370 | `			}` |
|       73 | 10371 | `		}` |
|       25 | 10372 | `	}` |
|  2839096 | 10373 | `	if( pMapEntry ){` |
|        - | 10374 | `		ph7_hashmap_node **apNode;` |
|  2839046 | 10375 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5678178 | 10376 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2839134 | 10377 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10378 | `				/* nullify the entry */` |
|  2839046 | 10379 | `				apNode[n] = 0;` |
|  1419522 | 10380 | `			}` |
|  1419568 | 10381 | `		}` |
|  1419522 | 10382 | `	}` |
|  2839096 | 10383 | `	return SXRET_OK;` |
|  1453062 | 10384 |  |
|        - | 10385 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10386 | `/*` |
|        - | 10387 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10388 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10389 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10390 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10391 | ` * For more information on how to register IO stream devices,please` |
|        - | 10392 | ` * refer to the official documentation.` |
|        - | 10393 | ` */` |
|    21928 | 10394 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10395 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10396 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10397 | `	int nByte              /* *pzDevice length*/` |
|        - | 10398 | `	)` |
|        2 | 10399 |  |
|        - | 10400 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10401 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10402 | `	SyString sDev,sCur;` |
|        - | 10403 | `	sxu32 n,nEntry;` |
|        - | 10404 | `	int rc;` |
|        - | 10405 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    21930 | 10406 | `	zNext = zCur = zIn = *pzDevice;` |
|    21930 | 10407 | `	zEnd = &zIn[nByte];` |
|  1394794 | 10408 | `	while( zIn < zEnd ){` |
|  1372868 | 10409 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10410 | `			/* Got one */` |
|        3 | 10411 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10412 | `			break;` |
|        - | 10413 | `		}` |
|        - | 10414 | `		/* Advance the cursor */` |
|  1372866 | 10415 | `		zIn++;` |
|        2 | 10416 | `	}` |
|    21930 | 10417 | `	if( zIn >= zEnd ){` |
|        - | 10418 | `		/* No such scheme,return the default stream */` |
|    21928 | 10419 | `		return pVm->pDefStream;` |
|        - | 10420 | `	}` |
|        3 | 10421 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10422 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10423 | `	SyStringFullTrim(&sDev);` |
|        - | 10424 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10425 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10426 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10427 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10428 | `		pStream = apStream[n];` |
|        3 | 10429 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10430 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10431 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10432 | `		if( rc == 0 ){` |
|        - | 10433 | `			/* Stream device found */` |
|        3 | 10434 | `			*pzDevice = zNext;` |
|        3 | 10435 | `			return pStream;` |
|        - | 10436 | `		}` |
|      ! 0 | 10437 | `	}` |
|        - | 10438 | `	/* No such stream,return NULL */` |
|      ! 0 | 10439 | `	return 0;` |
|    10966 | 10440 |  |
|        - | 10441 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10442 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10443 |  |
